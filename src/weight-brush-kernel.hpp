// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "weight-brush-device.hpp"
#if defined(__SYCL_DEVICE_ONLY__)
#include <sycl/ext/intel/math/imf_rounding_math.hpp>
#endif

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_WEIGHT_BRUSH_FN __host__ __device__ inline
#else
#define DOME_WEIGHT_BRUSH_FN inline
#endif

namespace dome::detail::weight_brush {
struct Stroke {
  const Capsule *direct,*reflected;
  uint32_t count;
  unsigned sides;
};

DOME_WEIGHT_BRUSH_FN float minimum(float a,float b){return b<a?b:a;}
DOME_WEIGHT_BRUSH_FN float maximum(float a,float b){return a<b?b:a;}
DOME_WEIGHT_BRUSH_FN float clamp_unit(float value){return minimum(maximum(value,0),1);}
DOME_WEIGHT_BRUSH_FN float divide(float a,float b){
#if defined(__SYCL_DEVICE_ONLY__)
  return sycl::ext::intel::math::fdiv_rn(a,b);
#elif defined(__HIP_DEVICE_COMPILE__)
  return float(double(a)/double(b));
#elif defined(__CUDA_ARCH__)
  return __fdiv_rn(a,b);
#else
  return a/b;
#endif
}
DOME_WEIGHT_BRUSH_FN float square_root(float value){
#if defined(__SYCL_DEVICE_ONLY__)
  return sycl::ext::intel::math::fsqrt_rn(value);
#elif defined(__HIP_DEVICE_COMPILE__)
  return float(std::sqrt(double(value)));
#elif defined(__CUDA_ARCH__)
  return __fsqrt_rn(value);
#else
  return std::sqrt(value);
#endif
}
DOME_WEIGHT_BRUSH_FN Vec3 difference(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
DOME_WEIGHT_BRUSH_FN float inner(Vec3 a,Vec3 b){return (a.x*b.x+a.y*b.y)+a.z*b.z;}

inline Capsule capsule(Vec3 from,Vec3 to,float radius,float strength,float pressure) {
  const auto direction=to-from,end=from+direction;
  return {from,direction,
          {minimum(from.x,end.x),minimum(from.y,end.y),minimum(from.z,end.z)},
          {maximum(from.x,end.x),maximum(from.y,end.y),maximum(from.z,end.z)},
          inner(direction,direction),radius,strength,pressure};
}

DOME_WEIGHT_BRUSH_FN float falloff(Vec3 p,const Capsule &brush) {
  if(brush.strength==0||brush.pressure==0)return 0;
  // Endpoints bound the rounded projection componentwise. This enlarged
  // radius rejects only definite misses, including near subnormal distances.
  const float reach=maximum(2*brush.radius,1e-12f);
  if(brush.minimum.x-p.x>reach||p.x-brush.maximum.x>reach||
     brush.minimum.y-p.y>reach||p.y-brush.maximum.y>reach||
     brush.minimum.z-p.z>reach||p.z-brush.maximum.z>reach)return 0;
  const auto d=brush.direction;
  const float t=brush.squared_length>1e-20f?
      clamp_unit(divide(inner(difference(p,brush.from),d),brush.squared_length)):0;
  const auto a=brush.from;
  const auto offset=difference(p,{a.x+d.x*t,a.y+d.y*t,a.z+d.z*t});
  const float u=clamp_unit(1-divide(square_root(inner(offset,offset)),brush.radius));
  return u*u*(3-2*u)*brush.strength*brush.pressure;
}

DOME_WEIGHT_BRUSH_FN float strongest(Stroke brush,Vec3 p) {
  float direct=0,mirror=0;
  for(uint32_t i=0;i<brush.count;++i){
    if(brush.sides&Direct)direct=maximum(direct,falloff(p,brush.direct[i]));
    if(brush.sides&Reflected)mirror=maximum(mirror,falloff(p,brush.reflected[i]));
  }
  return direct>=mirror?direct:-mirror;
}
}
#undef DOME_WEIGHT_BRUSH_FN
