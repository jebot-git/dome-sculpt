// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-padding-device.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_BAKE_PADDING_FN __host__ __device__ inline
#else
#define DOME_BAKE_PADDING_FN inline
#endif

namespace dome::detail::bake_padding {
inline constexpr uint32_t source_mask=(1u<<24)-1;
inline constexpr uint32_t unvisited=UINT32_MAX;
struct Frontier {
  uint32_t *owners,*queue,*size;
  uint32_t resolution;
};

DOME_BAKE_PADDING_FN void neighbors(uint32_t resolution,uint32_t pixel,auto visit){
  const int x=int(pixel%resolution),y=int(pixel/resolution),n=int(resolution);
  for(int row=y>0?y-1:0;row<=y+1&&row<n;++row)
    for(int column=x>0?x-1:0;column<=x+1&&column<n;++column)
      if(row!=y||column!=x)visit(uint32_t(row*n+column));
}

DOME_BAKE_PADDING_FN void seed(Frontier view,const uint8_t *coverage,uint32_t pixel,auto enqueue){
  view.owners[pixel]=0;
  if(coverage[pixel])return;
  auto first=unvisited;
  neighbors(view.resolution,pixel,[&](uint32_t neighbor){
    if(coverage[neighbor]==1&&neighbor<first)first=neighbor;
  });
  view.owners[pixel]=first==unvisited?unvisited:(1u<<24)|first;
  if(first!=unvisited)enqueue(view.size,view.queue,pixel);
}

DOME_BAKE_PADDING_FN void expand(Frontier view,uint32_t index,uint32_t distance,auto load,auto lower,auto enqueue){
  const auto pixel=view.queue[index];
  const auto key=(distance<<24)|(load(view.owners+pixel)&source_mask);
  neighbors(view.resolution,pixel,[&](uint32_t target){
    // An earlier distance always wins. At equal distance the earliest
    // row-major seed wins, as in the native multi-source FIFO traversal.
    if(lower(view.owners+target,key)==unvisited)enqueue(view.size,view.queue,target);
  });
}

DOME_BAKE_PADDING_FN Pixel pair_at(Frontier view,uint32_t index){
  const auto target=view.queue[index];
  return {target,view.owners[target]&source_mask};
}
}
#undef DOME_BAKE_PADDING_FN
