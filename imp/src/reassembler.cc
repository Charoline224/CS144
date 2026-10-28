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
    if(tmp_store.find(i) != tmp_store.end())continue;
    tmp_store[i] = data[i - first_index];
  }
  
  //再开始写入
  string ready;
  //持续寻找连续
  uint64_t i = next_index;
  while(tmp_store.find(i) != tmp_store.end()){
    auto n = tmp_store.find(i);
    ready += n->second;
    i++;
    tmp_store.erase(n);
  }
  write(next_index, ready);
}
  




//   //已经添加过的片段跳过
//   if(first_index + data.size() <  next_index)return;
//   //检查有没有合适的/重复的,找到本次处理的seg
//   Segment seg = checkstore(first_index, data, is_last_substring);
//       first_index = seg.index;
//       data = seg.data;
//       is_last_substring = seg.is_last;
//   const u_int64_t data_size = data.size();
//   //符合要求，开始写
//   if (first_index <= next_index ){
//     uint64_t from = next_index - first_index;
//     u_int64_t index = from;
//     while(index < data_size && output_.writer().available_capacity() > 0){
//       u_int64_t size = data_size - index;
//       size_t out_len = min(output_.writer().available_capacity(), size);
//       output_.writer().push(data.substr(index, out_len));
//       index += out_len;
//     }
//     next_index += (index - from);
//     //写不下的：
//     if(index < data_size-1){
//       Segment more  = Segment{index+1, data.substr(index, data_size - index), is_last_substring};
//       store(more);
//     }else{
//         //写到最后一调，关闭output
//       if(is_last_substring){
//         output_.writer().close();
//       }
//     }
//   }else {
//     min_store_idx = min(min_store_idx, first_index);
//     store(seg);
//   }

// }




// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t total = tmp_store.size();
  return total;
}
