// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint-composite-kernel.hpp"
#include <cfloat>

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_BAKE_FILTER_FN __host__ __device__ inline
#else
#define DOME_BAKE_FILTER_FN inline
#endif

namespace dome::detail::bake_material {
DOME_BAKE_FILTER_FN bool finite_uv(const float (&uv)[2]){
  return uv[0]>=-FLT_MAX&&uv[0]<=FLT_MAX&&uv[1]>=-FLT_MAX&&uv[1]<=FLT_MAX;
}

DOME_BAKE_FILTER_FN SourceTextureAxis legacy_axis(float pixel,uint32_t size,bool repeat){
  const int32_t truncated=int32_t(pixel);
  const int32_t first=truncated-(float(truncated)>pixel);
  const int32_t extent=int32_t(size);
  auto address=[&](int32_t i){
    if(repeat){const int32_t index=i%extent;return index<0?index+extent:index;}
    return i<0?0:i>=extent?extent-1:i;
  };
  return {address(first),address(first+1),pixel-first};
}

DOME_BAKE_FILTER_FN SourceTextureAxis clamped_axis(float uv,uint32_t resolution){
  const float unit=uv<0?0:uv>1?1:uv;
  return legacy_axis(paint_compute::sub(paint_compute::mul(unit,float(resolution)),.5f),resolution,false);
}

struct CoveredNormalMap {
  const uint8_t *rgba=nullptr,*coverage=nullptr;
  const int32_t *padding=nullptr;
  uint32_t resolution=0;
};

// Premultiplied XYZ and coverage are independent of the original image and
// carry the current writable UV0/Mikk frame to the existing normal resolver.
DOME_BAKE_FILTER_FN void sample_covered_normal(const CoveredNormalMap &map,const float (&uv)[2],
    const paint_compute::Quantization &table,float (&filtered)[4]){
  using namespace paint_compute;
  const auto x=clamped_axis(uv[0],map.resolution),y=clamped_axis(uv[1],map.resolution);
  for(unsigned tap=0;tap<4;++tap){
    const bool right=tap&1,bottom=tap&2;
    const auto pixel=size_t(bottom?y.second:y.first)*map.resolution+(right?x.second:x.first);
    const auto owner=map.padding[pixel];
    if(owner<0)continue;
    const float weight=mul(mul(right?x.fraction:sub(1.f,x.fraction),bottom?y.fraction:sub(1.f,y.fraction)),
                           table.unit[map.coverage[owner]]);
    filtered[3]=add(filtered[3],weight);
    for(unsigned c=0;c<3;++c){
      const float direction=sub(mul(table.unit[map.rgba[4*size_t(owner)+c]],2.f),1.f);
      filtered[c]=add(filtered[c],mul(direction,weight));
    }
  }
}
} // namespace dome::detail::bake_material
#undef DOME_BAKE_FILTER_FN
