// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "portable-normal-raster.hpp"
#include "paint-composite-kernel.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_NORMAL_FN __host__ __device__ inline
#else
#define DOME_NORMAL_FN inline
#endif

namespace dome::detail::portable_normal {
struct ImageView { const uint8_t *rgba; uint32_t width,height; };
namespace device_math {
DOME_NORMAL_FN Vec3 plus(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
DOME_NORMAL_FN Vec3 minus(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
DOME_NORMAL_FN Vec3 scale(Vec3 a,float b){return {a.x*b,a.y*b,a.z*b};}
DOME_NORMAL_FN float inner(Vec3 a,Vec3 b){return (a.x*b.x+a.y*b.y)+a.z*b.z;}
DOME_NORMAL_FN Vec3 outer(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
DOME_NORMAL_FN Vec3 unit(Vec3 a){
 const float squared=inner(a,a);
 return squared>1e-24f?scale(a,1.f/std::sqrt(squared)):Vec3{};
}
DOME_NORMAL_FN Vec3 apply(NormalBakeBasis b,Vec3 n){
 return plus(plus(scale(b.t,n.x),scale(b.b,n.y)),scale(b.n,n.z));
}
DOME_NORMAL_FN float determinant(NormalBakeBasis b){return inner(b.t,outer(b.b,b.n));}
DOME_NORMAL_FN float conditioning(NormalBakeBasis b,float determinant){
 return std::abs(determinant)/std::sqrt(inner(b.t,b.t)*inner(b.b,b.b)*inner(b.n,b.n));
}
DOME_NORMAL_FN Vec3 unapply(NormalBakeBasis b,Vec3 v,float det){
 return {inner(v,outer(b.b,b.n))/det,inner(b.t,outer(v,b.n))/det,inner(b.t,outer(b.b,v))/det};
}
DOME_NORMAL_FN Vec3 interpolate(const NormalBakeVertex (&v)[3],const Sample &p,Vec3 NormalBakeVertex::*member){
 return plus(plus(scale(v[0].*member,p.barycentric[0]),scale(v[1].*member,p.barycentric[1])),
             scale(v[2].*member,p.barycentric[2]));
}
DOME_NORMAL_FN bool interpolate_basis(const NormalBakeVertex (&v)[3],const Sample &p,NormalBakeBasis &basis){
 const auto t=interpolate(v,p,&NormalBakeVertex::t);
 const auto b=interpolate(v,p,&NormalBakeVertex::b);
 const auto n=interpolate(v,p,&NormalBakeVertex::n);
 // Conditioning of the normalized frame alone does not detect loss of
 // precision when the interpolated columns nearly cancel.
 if(!(inner(t,t)>.0625f&&inner(b,b)>.0625f&&inner(n,n)>.0625f))return false;
 basis={unit(t),unit(b),unit(n)};
 return true;
}
DOME_NORMAL_FN SourceTextureAxis clamp_axis(float coordinate,uint32_t size){
 const float uv=coordinate<0?0:coordinate>1?1:coordinate;
 const float x=paint_compute::sub(paint_compute::mul(uv,float(size)),.5f);
 const int first=x<0?-1:int(x);
 return {first<0?0:first,first+1<int(size)?first+1:int(size)-1,x-first};
}
}

DOME_NORMAL_FN uint32_t encode_texel(Chart chart,Sample sample,ImageView source,
                                    const paint_compute::Quantization &q){
 using namespace device_math;
 const auto &f=chart.source;
 NormalBakeBasis original;
 if(f.native){
  const auto n=unit(interpolate(f.vertices,sample,&NormalBakeVertex::n));
  const auto projected=minus(f.before.t,scale(n,inner(f.before.t,n)));
  if(!(inner(projected,projected)>.0625f))return needs_reference;
  const auto t=unit(projected);
  original={t,scale(outer(n,t),f.handedness),n};
 }else if(!interpolate_basis(f.vertices,sample,original))return needs_reference;
 NormalBakeBasis target;
 if(!interpolate_basis(chart.destination,sample,target))return needs_reference;
 const auto original_quality=conditioning(original,determinant(original));
 const float det=determinant(target),geometric=determinant(f.before);
 const auto target_quality=conditioning(target,det);
 // Route poorly conditioned cases through the existing full-precision
 // evaluator. The CPU certificate still checks the completed image.
 if(!(original_quality>.04f&&target_quality>.04f&&std::abs(geometric)>.9f))return needs_reference;

 const paint_compute::SourcePixelTaps taps{clamp_axis(sample.uv[0],source.width),
                                          clamp_axis(sample.uv[1],source.height)};
 float e[4];
 paint_compute::sample_source_taps(source.rgba,source.width,false,taps,false,q,e);
 const Vec3 detail{e[0]*2-1,e[1]*2-1,e[2]*2-1};
 const float strength=inner(detail,detail);
 const bool available=!f.native||e[3]>0;
 if(available&&strength<1e-10f)return needs_reference;
 const auto before=available?apply(original,detail):original.n;
 const auto wanted=unit(apply(f.current,unapply(f.before,unit(before),geometric)));
 const auto local=unit(unapply(target,wanted,det));

 const float rounding_guard=.002f/(original_quality*target_quality);
 const float components[]{local.x,local.y,local.z};
 uint32_t rgba=0xff000000u;
 float decoded[3]{};
 for(unsigned k=0;k<3;++k){
  const float scaled=(components[k]+1)*127.5f;
  if(!(scaled>=0&&scaled<=255))return needs_reference;
  const auto lower=uint32_t(scaled);
  if(std::abs((scaled-float(lower))-.5f)<=rounding_guard)return needs_reference;
  const uint32_t byte=lower+(scaled-float(lower)>.5f);
  if(k==2&&byte<128)return needs_reference;
  decoded[k]=float(byte)*(2.f/255.f)-1;
  rgba|=byte<<(8*k);
 }
 const auto reconstructed=unit(apply(target,{decoded[0],decoded[1],decoded[2]}));
 const auto error=outer(wanted,reconstructed);
 const auto alignment=inner(wanted,reconstructed);
 // A buffer inside the portable 0.02-radian bound avoids making a
 // preflight decision on device rounding at the acceptance threshold.
 const auto limit=.01975f*alignment;
 return alignment>0&&inner(error,error)<limit*limit?rgba:needs_reference;
}
}
#undef DOME_NORMAL_FN
