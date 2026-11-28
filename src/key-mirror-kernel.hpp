// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "key-mirror-device.hpp"
#include <bit>

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_KEY_INLINE __host__ __device__ inline
#else
#define DOME_KEY_INLINE inline
#endif

namespace dome::detail::key_mirror {
struct View {
  const Bin *bins;
  const uint32_t *order;
  const Vec3 *rest,*delta;
  uint32_t bin_count;
};

struct Progress { uint32_t donor=missing,neighbor=0,next=0,end=0; };
inline constexpr uint32_t pending=1u<<31;
inline constexpr uint32_t visits_per_launch=256;
static_assert(batch_capacity*visits_per_launch<pending);

DOME_KEY_INLINE bool before(Cell a,Cell b) {
  if(a.x!=b.x)return a.x<b.x;
  if(a.y!=b.y)return a.y<b.y;
  return a.z<b.z;
}

DOME_KEY_INLINE Bin find(View view,Cell wanted) {
  uint32_t lo=0,hi=view.bin_count;
  while(lo<hi){
    const auto mid=lo+(hi-lo)/2;
    if(before(view.bins[mid].cell,wanted))lo=mid+1;
    else hi=mid;
  }
  if(lo<view.bin_count){
    const auto bin=view.bins[lo];
    if(bin.cell.x==wanted.x&&bin.cell.y==wanted.y&&bin.cell.z==wanted.z)return bin;
  }
  return {};
}

DOME_KEY_INLINE uint32_t float_bits(float value) {
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
  return __float_as_uint(value);
#else
  return std::bit_cast<uint32_t>(value);
#endif
}

DOME_KEY_INLINE bool same_channel(float a,float b) {
  const auto left=float_bits(a),right=float_bits(b);
  // The source has already been checked for finiteness. Only signed zeros
  // have distinct encodings that compare equal as displacement components.
  return left==right||!((left|right)&0x7fffffffu);
}

template<class Precision>
DOME_KEY_INLINE uint32_t visit(View view,Vec3 target,Cell cell,Metric metric,uint32_t allowance,
                              Progress &state,uint32_t &donor) {
  using Real=typename Precision::Scalar;
  const auto twice_plane=Precision::from_bits(metric.twice_plane);
  const auto square=Precision::from_bits(metric.squared_tolerance);
  uint32_t visits=0;
  for(;;){
    if(state.next==state.end){
      if(state.neighbor==27)break;
      const int n=int(state.neighbor++);
      const auto bin=find(view,{cell.x+n/9-1,cell.y+n/3%3-1,cell.z+n%3-1});
      state.next=bin.first;
      state.end=bin.first+bin.count;
      continue;
    }
    if(visits==allowance)return visits|pending;
    ++visits;
    const auto vertex=view.order[state.next++];
    const auto q=view.rest[vertex];
    const Real x=Real(target.x)+Real(q.x)-twice_plane;
    const Real y=Real(target.y)-Real(q.y),z=Real(target.z)-Real(q.z);
    if(x*x+y*y+z*z>square)continue;
    if(state.donor!=missing){
      const auto previous=view.delta[state.donor],candidate=view.delta[vertex];
      if(!same_channel(previous.x,candidate.x)||!same_channel(previous.y,candidate.y)||
         !same_channel(previous.z,candidate.z)){
        state={conflict,27,0,0};
        break;
      }
    }
    // Keep the last exact agreement in the historical cell/vertex order.
    state.donor=vertex;
  }
  donor=state.donor;
  return visits;
}
}
#undef DOME_KEY_INLINE
