#include "reassembler.hh"
#include <algorithm>
#include <string>
#include <iostream>
using namespace std;


//写入缓冲区 next和output一定改变
void Reassembler::write(uint64_t first_index, string data)
{
    // 接收窗口右边界（不包含）
    const uint64_t window_end = output_.reader().bytes_popped() + capacity_;
    // 当前片段的右边界（不包含）
    const uint64_t data_end = first_index + data.size();
    // 实际允许写入的右边界
    const uint64_t end = min(window_end, data_end);

    // 已经写过，或没有可写入的数据
    if (end > next_index) {
      const uint64_t len = end - next_index;
      output_.writer().push(data.substr(0, len));
        // 根据实际写入的字节数更新
      next_index = output_.writer().bytes_pushed();
    }

    // 只有全部数据都组装完成才能关闭
    if (last_index != -1 &&
        next_index == static_cast<uint64_t>(last_index)) {
        output_.writer().close();
    }

}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // 接收窗口右边界（不包含）
  const uint64_t window_end = output_.reader().bytes_popped() + capacity_;
  //已经添加过的片段跳过
  if(first_index + data.size() <  next_index)return;
  //记录最后一个字节index
  if(is_last_substring){
    last_index = first_index + data.size();
  }

  //先存储
  for(uint64_t i = max(first_index, next_index); i < min(window_end, first_index + data.size()); i++){
    tmp_store[i%capacity_] =  data[i - first_index];
    received[i%capacity_] = true;
  }
  //再开始写入
  string ready;
  //持续寻找连续
  for(uint64_t i = next_index; i < window_end; i++) {
    if(!received[i%capacity_]){
      break;
    }
    ready += tmp_store[i%capacity_];
    received[i%capacity_] = false;
  }
  write(next_index, ready);
}
  






// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  
  uint64_t total = 0;
  for(uint64_t i  = 0; i < capacity_; i++){
    if(received[i])total++;
  }
  return total;
}
