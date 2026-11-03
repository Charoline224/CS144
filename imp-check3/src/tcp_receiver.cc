#include "tcp_receiver.hh"
#include "debug.hh"
#include <algorithm>
#include <iostream>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  if(message.RST){
    reassembler_.reader().set_error();
    return;
  }
  uint64_t first_index;
  if(message.SYN){
    zero_point_ = message.seqno;
    syn_received = true;
    first_index = message.seqno.unwrap(zero_point_, reassembler_.writer().bytes_pushed() + 1); 
  }else{
    first_index = message.seqno.unwrap(zero_point_, reassembler_.writer().bytes_pushed() + 1); 
    first_index--;
  }
  std::string data = message.payload;
  bool is_last = false;
  if(message.FIN){
    is_last = true;
  }  
  reassembler_.insert(first_index, data, is_last);

  (void)message;
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage msg = TCPReceiverMessage{};
  if(error || reassembler_.writer().has_error()){
    msg.RST = true;
    return msg;
  }
  if(!syn_received){
    msg.RST = false;
    msg.window_size = ((reader().bytes_popped() + writer().capacity()) - (reassembler_.writer().bytes_pushed())) >= (uint64_t)UINT16_MAX ? UINT16_MAX:uint16_t(reader().bytes_popped() + writer().capacity()) - (reassembler_.writer().bytes_pushed()) ;
    return msg;
  }
  Wrap32 ackno = Wrap32::wrap(reassembler_.writer().bytes_pushed() + 1
    + (reassembler_.writer().is_closed() ? 1 : 0),zero_point_);
  msg.ackno = ackno;
  msg.window_size = ((reader().bytes_popped() + writer().capacity()) - (reassembler_.writer().bytes_pushed())) >= (uint64_t)UINT16_MAX ? UINT16_MAX:uint16_t(reader().bytes_popped() + writer().capacity()) - (reassembler_.writer().bytes_pushed()) ;
  msg.RST = false;
  return msg;
}