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
  uint32_t bin_count;
};

struct Candidates { uint32_t neighbor=0,next=0,end=0; };
struct Progress { Candidates cursor; uint32_t donor=missing; };
struct TriangleProgress { Candidates cursor; TriangleMatch match{missing,0}; };
inline constexpr uint32_t pending=1u<<31;
inline constexpr uint32_t visits_per_launch=256;
static_assert(batch_capacity*(visits_per_launch+8)<pending);

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

DOME_KEY_INLINE bool candidates(View view,Cell cell,Candidates &cursor) {
  while(cursor.next==cursor.end) {
    if(cursor.neighbor==27)return false;
    const int n=int(cursor.neighbor++);
    const auto bin=find(view,{cell.x+n/9-1,cell.y+n/3%3-1,cell.z+n%3-1});
    cursor.next=bin.first;
    cursor.end=bin.first+bin.count;
  }
  return true;
}

template<class Real>
DOME_KEY_INLINE Real reflected_square(Vec3 p,Vec3 q,Real twice_plane) {
  const Real x=Real(p.x)+Real(q.x)-twice_plane;
  const Real y=Real(p.y)-Real(q.y),z=Real(p.z)-Real(q.z);
  return x*x+y*y+z*z;
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
DOME_KEY_INLINE uint32_t visit(View view,const Vec3 *rest,const Vec3 *delta,size_t target,
                              Cell cell,Metric metric,uint32_t allowance,
                              Progress &state,uint32_t &donor) {
  const auto twice_plane=Precision::from_bits(metric.twice_plane);
  const auto square=Precision::from_bits(metric.squared_tolerance);
  uint32_t visits=0;
  while(candidates(view,cell,state.cursor)){
    if(visits==allowance)return visits|pending;
    ++visits;
    const auto vertex=view.order[state.cursor.next++];
    if(reflected_square(rest[target],rest[vertex],twice_plane)>square)continue;
    if(state.donor!=missing){
      const auto previous=delta[state.donor],candidate=delta[vertex];
      if(!same_channel(previous.x,candidate.x)||!same_channel(previous.y,candidate.y)||
         !same_channel(previous.z,candidate.z)){
        state={{27,0,0},conflict};
        break;
      }
    }
    // Keep the last exact agreement in the historical cell/vertex order.
    state.donor=vertex;
  }
  donor=state.donor;
  return visits;
}

template<class Precision>
DOME_KEY_INLINE uint32_t visit(View view,const Vec3 *rest,const uint32_t *indices,
    size_t triangle,Cell cell,Metric metric,uint32_t allowance,TriangleProgress &state,
    TriangleMatch &result) {
  using Real=typename Precision::Scalar;
  const auto twice_plane=Precision::from_bits(metric.twice_plane);
  const auto square=Precision::from_bits(metric.squared_tolerance);
  const auto target=triangle*3;
  const Vec3 p[]{rest[indices[target]],rest[indices[target+1]],rest[indices[target+2]]};
  uint32_t visits=0;
  while(candidates(view,cell,state.cursor)){
    if(visits>=allowance)return visits|pending;
    const auto source=view.order[state.cursor.next++]*3;
    for(uint32_t start=0;start<3;++start){
      visits+=3;
      Real largest{0.f};
      bool good=true;
      for(uint32_t k=0;k<3;++k){
        const auto d=reflected_square(p[k],rest[indices[source+(start+3-k)%3]],twice_plane);
        if(d>square){good=false;break;}
        if(d>largest)largest=d;
      }
      if(!good)continue;
      if(state.match.first_corner!=missing){
        state={{27,0,0},{conflict,0}};
        result=state.match;
        return visits;
      }
      state.match={source+start,Precision::bits(largest)};
    }
  }
  result=state.match;
  return visits;
}
}
#undef DOME_KEY_INLINE
