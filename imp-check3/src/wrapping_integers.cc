#include "wrapping_integers.hh"
#include "debug.hh"
#include <vector>
#include <iostream>
using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return Wrap32 { uint32_t(n + uint64_t(zero_point.raw_value_)) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  Wrap32 check_seqno = wrap(checkpoint, zero_point);
  uint32_t left_dis = check_seqno.raw_value_ - raw_value_;
  uint32_t right_dis = raw_value_ - check_seqno.raw_value_;
  if(left_dis <= checkpoint && left_dis < right_dis){
    return checkpoint - left_dis;
  }else{
    return checkpoint + right_dis;
  }
}
