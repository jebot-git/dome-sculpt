// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint-source-coordinate.hpp"
#include "paint-grid-filter-kernel.hpp"
#include <bit>

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_COORD_FN __host__ __device__ inline
#else
#define DOME_COORD_FN inline
#endif

namespace dome::paint_compute {
namespace coordinates {
inline constexpr uint64_t sign=1ull<<63, hidden=1ull<<52;
inline constexpr uint64_t one=0x3ff0000000000000ull,two=0x4000000000000000ull;

DOME_COORD_FN uint64_t subtract(uint64_t a,uint64_t b){
  return paint_grid::add(a,b^sign);
}
// Validated F32 coefficients and offsets bound both coordinates to ±30000.
DOME_COORD_FN int truncated(uint64_t bits){
  const int exponent=int((bits>>52)&0x7ffu)-1023;
  if(exponent<0)return 0;
  const int magnitude=int((hidden|(bits&(hidden-1)))>>(52-exponent));
  return bits&sign?-magnitude:magnitude;
}
DOME_COORD_FN uint64_t wrap(uint64_t bits,int mode){
  if(!(bits&~sign))return 0;
  if(mode==33071)return bits&sign?0:bits>one?one:bits;
  const int integral=truncated(bits);
  if(mode==33648){
    auto r=subtract(bits,paint_grid::integer((integral/2)*2));
    if(!(r&~sign))return 0;
    if((r&sign)&&(r&~sign))r=paint_grid::add(r,two);
    return r>one?subtract(two,r):r;
  }
  const auto exact_integer=paint_grid::integer(integral);
  const bool below=(bits&sign)&&(bits&~sign)&&bits!=exact_integer;
  return subtract(bits,below?paint_grid::integer(integral-1):exact_integer);
}
DOME_COORD_FN float narrow(uint64_t bits){
  const auto word=paint_grid::narrow_float_bits(bits);
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
  return __uint_as_float(word);
#else
  return std::bit_cast<float>(word);
#endif
}
DOME_COORD_FN int address(int p,int extent,int mode){
  if(unsigned(p)<unsigned(extent))return p;
  if(mode==33071)return p<0?0:extent-1;
  const int limit=mode==33648?2*extent:extent;
  p%=limit;
  if(p<0)p+=limit;
  return p<extent?p:limit-1-p;
}
DOME_COORD_FN int floor(float value){
  const int integral=int(value);
  return integral-(value<float(integral));
}
DOME_COORD_FN detail::SourceTextureAxis axis(uint64_t coordinate,int extent,int mode,bool nearest){
  // Keep the host sampler's binary32 boundaries after wrapping.
  const volatile float scaled=narrow(wrap(coordinate,mode))*float(extent);
  const float at=scaled-.5f;
  if(nearest){
    const auto value=address(floor(at+.5f),extent,mode);
    return {value,value,0};
  }
  const int first=floor(at);
  return {address(first,extent,mode),address(first+1,extent,mode),at-first};
}
}

DOME_COORD_FN SourcePixelTaps source_coordinates(const SourceCoordinateJob &job,uint32_t x,uint32_t y){
  const auto a=job.columns[x],b=job.rows[y];
  const auto u=paint_grid::add(paint_grid::add(a.u,b.u),job.transform.offset_u);
  const auto v=paint_grid::add(paint_grid::add(a.v,b.v),job.transform.offset_v);
  return {coordinates::axis(u,job.width,job.transform.wrap_s,job.nearest),
          coordinates::axis(v,job.height,job.transform.wrap_t,job.nearest)};
}
}
#undef DOME_COORD_FN
