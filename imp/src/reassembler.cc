#include "reassembler.hh"
#include <algorithm>
#include <string>
#include <iostream>
using namespace std;


int iscontain(u_int64_t index1, u_int64_t index2, string data1, string data2){
  if(index1 < index2 && index1 + data1.size() > index2 + data2.size()){
    return 1;
  }else if(index2 <= index1 && index1 + data1.size() <= index2 + data2.size()) {
    return 2;
  }return 0;
}

Segment merge_segments(const Segment& a, const Segment& b)
{
  const Segment* left = &a;
  const Segment* right = &b;
  if (b.index < a.index) {
    left = &b;
    right = &a;
  }
  const uint64_t left_end = left->index + left->data.size(); // 末尾后一位
  if (right->index > left_end) {
    std::cout<<"segments have a gap";
  }
  const uint64_t overlap =
      min<uint64_t>(left_end - right->index, right->data.size());
  std::string merged = left->data;
  merged += right->data.substr(overlap);

  return Segment{left->index, merged};
}


//维护不重叠
Segment Reassembler::checkstore(u_int64_t index, string data){
  const uint64_t window_end =
        output_.reader().bytes_popped() + capacity_;
  if (index >= window_end) {
    return Segment{index, ""};
  }
  uint64_t end = min(window_end, index+data.size());
  Segment seg = Segment{index, data.substr(0, end - index )};
  for ( auto it = tmp_store.begin(); it != tmp_store.end(); ){
      const auto i = it->first;
      auto& tmp = it->second;
      uint64_t last = i + tmp.data.size();
      //查看是否有过期的
      if(last < next_index){
        it = tmp_store.erase(it);
        continue;
      }
      //查看包含关系有无
      int flag = iscontain(seg.index, i, seg.data, tmp.data);
      if(flag == 1){
        it = tmp_store.erase(it);
        continue;
      }else if(flag == 2){
        seg = tmp;
      }else{
        //如果没有重叠
        if(i > seg.index + seg.data.size() || last < seg.index)break;
        //有重叠，合并，更新seg
        Segment n = merge_segments(tmp, seg);
        it = tmp_store.erase(it);
        seg = n;
        continue;
      }
      ++it;
  }
  tmp_store.emplace(seg.index, seg);
  return seg;
}
//写入缓冲区 next和output一定改变
void Reassembler::write(uint64_t first_index, string data)
{
    // 接收窗口右边界（不包含）
    const uint64_t window_end =
        output_.reader().bytes_popped() + capacity_;

    // 当前片段的右边界（不包含）
    const uint64_t data_end = first_index + data.size();

    // 实际允许写入的右边界
    const uint64_t end = min(window_end, data_end);

    // 只有覆盖 next_index 的片段才能交给 write()
    if (first_index > next_index) {
        return;
    }

    // 已经写过，或没有可写入的数据
    if (end > next_index) {
        // 将绝对索引转换为字符串内部偏移
        const uint64_t offset = next_index - first_index;
        const uint64_t len = end - next_index;

output_.writer().push(data.substr(offset, len));


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
  //已经添加过的片段跳过
  if(first_index + data.size() <  next_index)return;
  if(is_last_substring){
    last_index = first_index + data.size();
  }
  //来的片段直接写入
  if (first_index <= next_index ){
    write(first_index, data);
    
    //检查写入之后next的更新可否导致store变化
    auto it = tmp_store.begin() ;
    while (it != tmp_store.end()  && output_.writer().available_capacity() > 0) {
       const uint64_t start = it->first;
    const string seg_data = it->second.data;
    const uint64_t end = start + seg_data.size();

    // 前面存在缺口，暂时不能写入
    if (start > next_index) {
        break;
    }

    // 整个片段已经写入过，删除
    if (end <= next_index) {
        it = tmp_store.erase(it);
        continue;
    }

    // 尝试写入连续部分
    write(start, seg_data);

    // 已经完整写入，删除暂存片段
    if (next_index >= end) {
        it = tmp_store.erase(it);
        continue;
    }

    // 未能完整写入：删除旧片段
    it = tmp_store.erase(it);

    // 超出接收窗口的数据不能继续保存
    const uint64_t window_end =
        output_.reader().bytes_popped() + capacity_;

    const uint64_t remaining_end = min(end, window_end);

    // 只保留窗口内尚未写入的部分
    if (next_index < remaining_end) {
        tmp_store.emplace(
            next_index,
            Segment{
                next_index,
                seg_data.substr(
                    next_index - start,
                    remaining_end - next_index
                )
            }
        );
    }

    break;
    }
  }else{
    checkstore(first_index, data);
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

}


// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t total = 0;
  for (const auto& entry : tmp_store) {
    total += entry.second.data.size();
  }
  return total;
}
