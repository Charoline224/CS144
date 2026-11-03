#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include <algorithm>
using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  
  return {next_sendseq_ - max_ackno_};
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return {re_cnt_};
}

void TCPSender::push( const TransmitFunction& transmit )
{
  uint64_t tmp_window_size =( window_size_ == 0?1:window_size_);
  while(tmp_window_size + max_ackno_ > next_sendseq_ ){
    //构造msg
    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap(next_sendseq_, isn_);
    uint64_t space = tmp_window_size + max_ackno_ >= next_sendseq_?tmp_window_size + max_ackno_ - next_sendseq_:0;
    msg.SYN = (next_sendseq_ == 0);
    uint64_t read_size = min({reader().bytes_buffered(), space - msg.SYN,  uint64_t(TCPConfig::MAX_PAYLOAD_SIZE)});
    read(reader(), read_size, msg.payload);
    if(writer().is_closed() && reader().is_finished() && !is_fin && 
      msg.sequence_length() < space) {
      msg.FIN = true;
      is_fin = true;
    }
    if(input_.has_error())msg.RST = true;
    //发完了没
    if(msg.sequence_length()== 0)return;
    //发送
    transmit(msg);
    //维护nextsend
    next_sendseq_ += msg.sequence_length();
    //维护unackmsgh和计时器
    if(msg.sequence_length() > 0){
      if(!timer_running_)timer_running_ = true;
      unack_msg_.push_back(msg);
    }
  }
  

  (void)transmit;
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg = TCPSenderMessage{}; 
  msg.seqno = Wrap32::wrap(next_sendseq_, isn_);
  msg.RST = input_.has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if(msg.RST){
    input_.set_error();
    return;
  }
  window_size_ = msg.window_size;
  if (msg.ackno.has_value()) {
    const uint64_t ackno_abs = msg.ackno->unwrap(isn_, max_ackno_);
    if(ackno_abs > max_ackno_ && ackno_abs <= next_sendseq_){
      max_ackno_ = ackno_abs;
      //更新unack数组
      while (!unack_msg_.empty()) {
        const auto& segment = unack_msg_.front();
        const uint64_t start_abs = segment.seqno.unwrap(isn_, max_ackno_);
        const uint64_t end_abs = start_abs + segment.sequence_length();
        if (end_abs > ackno_abs) {
          break;
        }
        unack_msg_.pop_front();
      }
      RTO_ms_ = initial_RTO_ms_;
      timer_running_ = !unack_msg_.empty();
      re_cnt_ = 0;
    }
  }
  
  (void)msg;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if(!timer_running_){
    return ;
  }
  if(ms_since_last_tick >= RTO_ms_){
    //重传
    TCPSenderMessage msg = unack_msg_.front();
    transmit(msg);
    if(window_size_ > 0){
      re_cnt_++;
    }
    // 重启倒计时
      RTO_ms_ = initial_RTO_ms_ * (1ULL << re_cnt_);
  }else{
    RTO_ms_ -= ms_since_last_tick;
  }
  (void)transmit;
}

