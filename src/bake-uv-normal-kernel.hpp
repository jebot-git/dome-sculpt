// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "emission-raster-device.hpp"
#include "paint-grid-filter-kernel.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_UV_NORMAL_FN __host__ __device__ inline
#else
#define DOME_UV_NORMAL_FN inline
#endif
#if defined(__SYCL_DEVICE_ONLY__)
#define DOME_UV_NORMAL_ARITH [[clang::noinline]] static inline
#else
#define DOME_UV_NORMAL_ARITH DOME_UV_NORMAL_FN
#endif

namespace dome::detail::bake_uv_normal {
// The raw Mikk frame encoder uses binary64 even on devices that have no FP64.
// Its inputs originate in binary32, so nonzero intermediates stay normal in
// binary64 throughout interpolation, frame inversion and normalization.
using Wide=uint64_t;
constexpr Wide sign=Wide(1)<<63,hidden=Wide(1)<<52,one=std::bit_cast<Wide>(1.);
constexpr Wide epsilon=std::bit_cast<Wide>(1e-30),minimum_condition=std::bit_cast<Wide>(1e-6);
constexpr Wide half=std::bit_cast<Wide>(.5),byte_scale=std::bit_cast<Wide>(127.5),byte_limit=std::bit_cast<Wide>(255.);
constexpr Wide tan_angle_limit=std::bit_cast<Wide>(.020002667093402423); // tan(0.02 radians)

DOME_UV_NORMAL_FN Wide absolute(Wide value){return value&~sign;}
DOME_UV_NORMAL_FN Wide max_positive(Wide a,Wide b){return a>b?a:b;}
DOME_UV_NORMAL_ARITH Wide add(Wide a,Wide b){return paint_grid::add(a,b);}
DOME_UV_NORMAL_ARITH Wide mul(Wide a,Wide b){return paint_grid::mul(a,b);}
DOME_UV_NORMAL_FN Wide sub(Wide a,Wide b){return add(a,b^sign);}
DOME_UV_NORMAL_ARITH Wide div(Wide a,Wide b){
  return paint_grid::div(absolute(a),absolute(b))|((a^b)&sign);
}
DOME_UV_NORMAL_FN Wide widen(float value){return paint_grid::widen_float_bits(paint_compute::bits(value));}
DOME_UV_NORMAL_FN float narrow(Wide value){
  const auto word=paint_grid::narrow_float_bits(value);
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
  return __uint_as_float(word);
#else
  return std::bit_cast<float>(word);
#endif
}
DOME_UV_NORMAL_ARITH Wide root(Wide value){
  if(!value)return 0;
  const int exponent=int(value>>52)-1023;
  const unsigned odd=unsigned(exponent)&1u;
  const Wide rounded_exponent=Wide((exponent-int(odd))/2+1023)<<52;
  const auto fraction=value&(hidden-1);
  if(!odd&&!fraction)return rounded_exponent;
  const auto significand=(hidden|fraction)<<odd;
  // Integer square root of significand * 2^52, one pair at a time.
  // The remainder and trial divisor both fit in 64 bits.
  Wide result=0,remainder=0;
  for(int pair=52;pair>=0;--pair){
    remainder=(remainder<<2)|(pair>=26?((significand>>(2*(pair-26)))&3):0);
    const auto trial=(result<<2)|1;
    result<<=1;
    if(remainder>=trial){
      remainder-=trial;
      ++result;
    }
  }
  // The squared midpoint has a fractional quarter, so a tie is impossible.
  return rounded_exponent-hidden+result+(remainder>result);
}

struct Vector { Wide x,y,z; };
DOME_UV_NORMAL_FN Vector widen(Vec3 v){return {widen(v.x),widen(v.y),widen(v.z)};}
DOME_UV_NORMAL_FN Vec3 narrow(Vector v){return {narrow(v.x),narrow(v.y),narrow(v.z)};}
DOME_UV_NORMAL_FN Vector add(Vector a,Vector b){return {add(a.x,b.x),add(a.y,b.y),add(a.z,b.z)};}
DOME_UV_NORMAL_FN Vector mul(Vector a,Wide b){return {mul(a.x,b),mul(a.y,b),mul(a.z,b)};}
DOME_UV_NORMAL_FN Vector div(Vector a,Wide b){return {div(a.x,b),div(a.y,b),div(a.z,b)};}
DOME_UV_NORMAL_FN Wide dot(Vector a,Vector b){return add(add(mul(a.x,b.x),mul(a.y,b.y)),mul(a.z,b.z));}
DOME_UV_NORMAL_FN Vector cross(Vector a,Vector b){
  return {sub(mul(a.y,b.z),mul(a.z,b.y)),sub(mul(a.z,b.x),mul(a.x,b.z)),sub(mul(a.x,b.y),mul(a.y,b.x))};
}
DOME_UV_NORMAL_FN bool unit(Vector &value){
  const auto magnitude=max_positive(max_positive(absolute(value.x),absolute(value.y)),absolute(value.z));
  if(magnitude<epsilon)return false;
  value=div(value,magnitude);
  value=div(value,root(dot(value,value)));
  return true;
}
DOME_UV_NORMAL_FN Vec3 blend(const NormalBakeVertex (&v)[3],Vec3 NormalBakeVertex::*member,
                             const Wide (&bary)[3]){
  return narrow(add(add(mul(widen(v[0].*member),bary[0]),mul(widen(v[1].*member),bary[1])),
                    mul(widen(v[2].*member),bary[2])));
}
DOME_UV_NORMAL_FN NormalBakeBasis basis(const NormalBakeVertex (&v)[3],const Wide (&bary)[3]){
  return {blend(v,&NormalBakeVertex::t,bary),blend(v,&NormalBakeVertex::b,bary),blend(v,&NormalBakeVertex::n,bary)};
}
DOME_UV_NORMAL_FN bool same_handedness(const NormalBakeVertex (&v)[3]){
  return v[0].w==v[1].w&&v[0].w==v[2].w;
}
DOME_UV_NORMAL_FN Vec3 transport(NormalBakeBasis old,const float (&q)[3]){
  using paint_compute::add;
  using paint_compute::mul;
  const auto component=[&](float t,float b,float n){return add(add(mul(t,q[0]),mul(b,q[1])),mul(n,q[2]));};
  const Vec3 desired{component(old.t.x,old.b.x,old.n.x),component(old.t.y,old.b.y,old.n.y),
                     component(old.t.z,old.b.z,old.n.z)};
  const auto squared=add(add(mul(desired.x,desired.x),mul(desired.y,desired.y)),mul(desired.z,desired.z));
  return paint_compute::normal_length(squared)>1e-8f?desired:old.n;
}
DOME_UV_NORMAL_FN uint8_t quantize(Wide value){
  if((value&sign)||value<half)return 0;
  if(value>=byte_limit)return 255;
  const unsigned shift=1075-unsigned(value>>52);
  const auto significand=(value&(hidden-1))|hidden;
  return uint8_t((significand+(Wide(1)<<(shift-1)))>>shift);
}
DOME_UV_NORMAL_FN Wide decode(uint8_t value){
  return sub(div(paint_grid::integer(2*value),byte_limit),one);
}
DOME_UV_NORMAL_FN bool encode(const Vector &t,const Vector &b,const Vector &n,const Vector &cofactor,
                              Wide determinant,Vec3 desired,uint8_t (&bytes)[4]){
  auto world=widen(desired);
  if(!unit(world))return false;
  Vector local{dot(world,cofactor),dot(t,cross(world,n)),dot(t,cross(b,world))};
  local=div(local,determinant);
  if(!unit(local)||!unit(local))return false;
  bytes[0]=quantize(mul(add(local.x,one),byte_scale));
  bytes[1]=quantize(mul(add(local.y,one),byte_scale));
  bytes[2]=quantize(mul(add(local.z,one),byte_scale));
  bytes[3]=255;
  auto rebuilt=add(add(mul(t,decode(bytes[0])),mul(b,decode(bytes[1]))),mul(n,decode(bytes[2])));
  if(!unit(rebuilt))return false;
  rebuilt=widen(narrow(rebuilt));
  const auto alignment=dot(world,rebuilt);
  if((alignment&sign)||!alignment)return false;
  const auto separation=cross(world,rebuilt);
  return root(dot(separation,separation))<=mul(alignment<one?alignment:one,tan_angle_limit);
}

DOME_UV_NORMAL_FN bool transport(const EmissionRasterDevice::BakeUvFrame &frame,
    const EmissionRasterDevice::BakeUvSample &sample,const float (&normals)[2][3],bool has_mtoon,
    uint8_t (&encoded)[2][4]){
  if(!same_handedness(frame.before)||!same_handedness(frame.after))return false;
  const Wide u=widen(sample.bary[0]),v=widen(sample.bary[1]),bary[]{u,v,sub(sub(one,u),v)};
  const auto before=basis(frame.before,bary),after=basis(frame.after,bary);
  const auto t=widen(after.t),b=widen(after.b),n=widen(after.n);
  const auto cofactor=cross(b,n);
  const auto determinant=dot(t,cofactor);
  const auto lengths=mul(mul(root(dot(t,t)),root(dot(b,b))),root(dot(n,n)));
  if(!lengths||!absolute(determinant)||div(absolute(determinant),lengths)<minimum_condition)return false;
  return encode(t,b,n,cofactor,determinant,transport(before,normals[0]),encoded[0])&&
         (!has_mtoon||encode(t,b,n,cofactor,determinant,transport(before,normals[1]),encoded[1]));
}
}

#undef DOME_UV_NORMAL_FN
#undef DOME_UV_NORMAL_ARITH
