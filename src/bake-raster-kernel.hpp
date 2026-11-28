// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-raster-device.hpp"
#include "surface-ray-kernel.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_BAKE_RASTER_INLINE __host__ __device__ inline
#else
#define DOME_BAKE_RASTER_INLINE inline
#endif

namespace dome::detail::bake_raster {
struct View {
  const float *axis;
  const uint8_t *writable;
  uint32_t *owners;
  uint32_t resolution;
};

template<class Real>
DOME_BAKE_RASTER_INLINE Real edge(const float (&a)[2],const float (&b)[2],Real x,Real y){
  return (Real(b[0])-Real(a[0]))*(y-Real(a[1]))-(Real(b[1])-Real(a[1]))*(x-Real(a[0]));
}

template<class Precision,class Claim>
DOME_BAKE_RASTER_INLINE Sample raster(View view,const Span *spans,uint32_t span_count,
    uint32_t slot,uint32_t first_candidate,Claim claim){
  // The spans form a partition of the flattened candidate stream. A span
  // may resume a rectangle at an arbitrary column of a subsequent batch.
  uint32_t lo=0,hi=span_count;
  while(hi-lo>1){
    const auto mid=(lo+hi)/2;
    if(spans[mid].first<=slot)lo=mid;
    else hi=mid;
  }
  const auto &face=spans[lo];
  const auto in_rectangle=face.rectangle_offset+slot-face.first;
  const auto x=face.left+in_rectangle%face.width,y=face.bottom+in_rectangle/face.width;
  const auto pixel=y*view.resolution+x;
  if(view.writable&&!view.writable[pixel])return {};
  using Real=typename Precision::Scalar;
  const Real u(view.axis[x]),v(view.axis[y]),area=Precision::from_bits(face.area_bits);
  Sample result{pixel,lo,{}};
  for(unsigned corner=0;corner<3;++corner){
    const float weight=Precision::narrow(edge(face.uv[(corner+1)%3],face.uv[(corner+2)%3],u,v)/area);
    const bool includes_edge=face.top_left_edges&(1u<<corner);
    if(weight<-1e-7f||(weight<=1e-7f&&!includes_edge))return {};
    result.barycentric[corner]=weight;
  }
  claim(view.owners+pixel,first_candidate+slot);
  return result;
}

template<class Reserve,class Overlap>
DOME_BAKE_RASTER_INLINE void compact(View view,const Sample *candidates,uint32_t count,uint32_t first_candidate,
    uint32_t block,Block *ranges,Sample *output,Reserve reserve,Overlap add_overlap){
  const auto first=block*candidates_per_block;
  uint32_t winners=0,writes=0,overlaps=0;
  for(unsigned lane=0;lane<candidates_per_block&&first+lane<count;++lane){
    const auto slot=first+lane,pixel=candidates[slot].pixel;
    if(pixel==UINT32_MAX)continue;
    if(view.owners[pixel]==first_candidate+slot){
      winners|=1u<<lane;
      ++writes;
    }else ++overlaps;
  }
  const uint32_t offset=writes?reserve(writes):0;
  ranges[block]={offset,writes};
  if(overlaps)add_overlap(overlaps);
  unsigned written=0;
  for(unsigned lane=0;lane<candidates_per_block;++lane)
    if(winners&(1u<<lane))output[offset+written++]=candidates[first+lane];
}
} // namespace dome::detail::bake_raster
#undef DOME_BAKE_RASTER_INLINE
