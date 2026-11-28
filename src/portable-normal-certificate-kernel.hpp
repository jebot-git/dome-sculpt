// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "portable-normal-raster-kernel.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_CERT_FN __host__ __device__ inline
#else
#define DOME_CERT_FN inline
#endif
#if defined(__SYCL_DEVICE_ONLY__)
// Share the rounded interval operations in the Arc program rather than
// cloning them into every column of each source and emitted frame.
#define DOME_CERT_ARITH [[clang::noinline]] inline
#else
#define DOME_CERT_ARITH DOME_CERT_FN
#endif

namespace dome::detail::portable_normal {
namespace interval {
// Enclose the reference's FLOAT and double operations using FLOAT endpoints.
// A device decision is made only when the whole interval is on one side.
struct Range { float lo,hi; };
struct Vector { Range x,y,z; };
struct Basis { Vector t,b,n; };
DOME_CERT_FN float min(float a,float b){return a<b?a:b;}
DOME_CERT_FN float max(float a,float b){return a>b?a:b;}
DOME_CERT_FN Range point(float a){return {a,a};}
DOME_CERT_FN Vector point(Vec3 a){return {point(a.x),point(a.y),point(a.z)};}
DOME_CERT_FN Basis point(NormalBakeBasis b){return {point(b.t),point(b.b),point(b.n)};}
DOME_CERT_FN float as_float(uint32_t word){
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
 return __uint_as_float(word);
#else
 return std::bit_cast<float>(word);
#endif
}
DOME_CERT_FN float outward(float x,bool up){
 const uint32_t bits=paint_compute::bits(x);
 // Keep interval endpoints outside the denormal range on devices that flush.
 if((bits&0x7fffffffu)<=0x00800000u)return up?0x1p-125f:-0x1p-125f;
 if((bits&0x7fffffffu)>=0x7f800000u)return x;
 return as_float((x>0)==up?bits+1:bits-1);
}
DOME_CERT_FN Range around(float x){return {outward(x,false),outward(x,true)};}
DOME_CERT_FN Range negate(Range a){return {-a.hi,-a.lo};}
DOME_CERT_FN Range abs(Range a){
 if(a.lo>=0)return a;
 if(a.hi<=0)return negate(a);
 return {0,max(-a.lo,a.hi)};
}
DOME_CERT_FN Range add(Range a,Range b){
 return {outward(paint_compute::add(a.lo,b.lo),false),
         outward(paint_compute::add(a.hi,b.hi),true)};
}
DOME_CERT_FN Range subtract(Range a,Range b){return add(a,negate(b));}
DOME_CERT_ARITH Range multiply(Range a,Range b){
 const auto ll=paint_compute::mul(a.lo,b.lo),lh=paint_compute::mul(a.lo,b.hi);
 const auto hl=paint_compute::mul(a.hi,b.lo),hh=paint_compute::mul(a.hi,b.hi);
 return {outward(min(min(ll,lh),min(hl,hh)),false),
         outward(max(max(ll,lh),max(hl,hh)),true)};
}
DOME_CERT_FN Range square(Range a){
 a=abs(a);
 return {max(0,outward(paint_compute::mul(a.lo,a.lo),false)),
         outward(paint_compute::mul(a.hi,a.hi),true)};
}
DOME_CERT_FN float divide_rn(float a,float b){
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
DOME_CERT_ARITH Range divide_positive(Range a,Range b){
 return {outward(divide_rn(a.lo,a.lo<0?b.lo:b.hi),false),
         outward(divide_rn(a.hi,a.hi<0?b.hi:b.lo),true)};
}
DOME_CERT_FN Vector divide_positive(Vector v,Range b){
 const auto reciprocal=divide_positive(point(1),b);
 return {multiply(v.x,reciprocal),multiply(v.y,reciprocal),multiply(v.z,reciprocal)};
}
DOME_CERT_ARITH Range sqrt(Range a){
 return {max(0,outward(paint_compute::normal_length(max(0,a.lo)),false)),
         outward(paint_compute::normal_length(max(0,a.hi)),true)};
}
DOME_CERT_FN Vector add(Vector a,Vector b){return {add(a.x,b.x),add(a.y,b.y),add(a.z,b.z)};}
DOME_CERT_FN Vector subtract(Vector a,Vector b){return {subtract(a.x,b.x),subtract(a.y,b.y),subtract(a.z,b.z)};}
DOME_CERT_FN Vector scale(Vector a,Range b){return {multiply(a.x,b),multiply(a.y,b),multiply(a.z,b)};}
DOME_CERT_FN Range dot(Vector a,Vector b){
 return add(add(multiply(a.x,b.x),multiply(a.y,b.y)),multiply(a.z,b.z));
}
DOME_CERT_FN Vector cross(Vector a,Vector b){
 return {subtract(multiply(a.y,b.z),multiply(a.z,b.y)),
         subtract(multiply(a.z,b.x),multiply(a.x,b.z)),
         subtract(multiply(a.x,b.y),multiply(a.y,b.x))};
}
DOME_CERT_FN Range length(Vector a){return sqrt(add(add(square(a.x),square(a.y)),square(a.z)));}
DOME_CERT_FN Range max_abs(Vector a){
 const auto x=abs(a.x),y=abs(a.y),z=abs(a.z);
 return {max(x.lo,max(y.lo,z.lo)),max(x.hi,max(y.hi,z.hi))};
}
DOME_CERT_FN bool unit(Vector &a){
 const auto magnitude=max_abs(a);
 if(!(magnitude.lo>1e-12f))return false;
 a=divide_positive(a,magnitude);
 const auto norm=length(a);
 if(!(norm.lo>1e-12f))return false;
 a=divide_positive(a,norm);
 return true;
}
DOME_CERT_FN Vector apply(Basis b,Vector p){
 return add(add(scale(b.t,p.x),scale(b.b,p.y)),scale(b.n,p.z));
}
DOME_CERT_FN bool inverse_apply(Basis b,Vector p,Vector &out){
 const auto cofactor=cross(b.b,b.n);
 const auto det=dot(b.t,cofactor);
 const auto magnitude=abs(det);
 if(!(magnitude.lo>.5f))return false;
 Vector q{dot(p,cofactor),dot(b.t,cross(p,b.n)),dot(b.t,cross(b.b,p))};
 if(det.hi<0)q={negate(q.x),negate(q.y),negate(q.z)};
 out=divide_positive(q,magnitude);
 return true;
}
DOME_CERT_FN Vector blend(const NormalBakeVertex (&v)[3],const Range (&weights)[3],Vec3 NormalBakeVertex::*column){
 return add(add(scale(point(v[0].*column),weights[0]),scale(point(v[1].*column),weights[1])),
             scale(point(v[2].*column),weights[2]));
}
DOME_CERT_FN bool blend_basis(const NormalBakeVertex (&v)[3],const Range (&weights)[3],Basis &b){
 if(v[0].w!=v[1].w||v[0].w!=v[2].w)return false;
 b={blend(v,weights,&NormalBakeVertex::t),blend(v,weights,&NormalBakeVertex::b),
    blend(v,weights,&NormalBakeVertex::n)};
 return unit(b.t)&&unit(b.b)&&unit(b.n);
}
DOME_CERT_FN bool original_basis(const SourceFrame &f,const Range (&weights)[3],Basis &b){
 if(!f.native)return blend_basis(f.vertices,weights,b);
 b.n=blend(f.vertices,weights,&NormalBakeVertex::n);
 if(!unit(b.n))return false;
 const auto before_t=point(f.before.t);
 b.t=subtract(before_t,scale(b.n,dot(before_t,b.n)));
 if(!unit(b.t))return false;
 b.b=scale(cross(b.n,b.t),point(f.handedness));
 return true;
}
// Both portable normal grids are power of two. Their UV-to-texel scale is
// exact at FLOAT precision. Enclose either fused or separate accumulation
// of the reference's four FLOAT tap weights.
DOME_CERT_FN void sample(ImageView image,const float (&uv)[2],const paint_compute::Quantization &q,Range (&rgba)[4]){
 const auto s=device_math::clamp_axis(uv[0],image.width),t=device_math::clamp_axis(uv[1],image.height);
 for(auto &channel:rgba)channel={0,0};
 for(unsigned i=0;i<4;++i){
  const bool right=i&1,bottom=i&2;
  const float weight=paint_compute::mul(right?s.fraction:paint_compute::sub(1,s.fraction),
                                        bottom?t.fraction:paint_compute::sub(1,t.fraction));
  if(weight==0)continue;
  const auto *pixel=image.rgba+(size_t(bottom?t.second:t.first)*image.width+(right?s.second:s.first))*4;
  for(unsigned c=0;c<4;++c){
   if(pixel[c]==0)continue;
   rgba[c]=add(rgba[c],multiply(point(weight),point(q.unit[pixel[c]])));
  }
 }
}
DOME_CERT_FN Vector decode(const Range (&rgba)[4]){
 const auto channel=[&](unsigned c){return subtract(multiply(rgba[c],point(2)),point(1));};
 return {channel(0),channel(1),channel(2)};
}
}

DOME_CERT_FN Verdict certify_texel(const Chart &chart,const Probe &probe,ImageView original,ImageView emitted,
                                   const paint_compute::Quantization &q){
 using namespace interval;
 if((original.width&(original.width-1))||(original.height&(original.height-1))||
    (emitted.width&(emitted.width-1))||(emitted.height&(emitted.height-1)))
  return Verdict::Reference;
 const Range weights[]{around(probe.source.barycentric[0]),around(probe.source.barycentric[1]),
                        around(probe.source.barycentric[2])};
 Basis source,target;
 if(!original_basis(chart.source,weights,source)||!blend_basis(chart.destination,weights,target))
  return Verdict::Reference;
 Range e[4];
 sample(original,probe.source.uv,q,e);
 const auto detail=decode(e);
 const bool native=chart.source.native;
 const auto strength=native?length(detail):max_abs(detail);
 const float minimum=native?1e-8f:1e-20f;
 const bool inherited=(native&&e[3].hi<=0)||strength.hi<=minimum;
 if(!inherited&&((native&&e[3].lo<=0)||strength.lo<=minimum))return Verdict::Reference;
 auto direction=inherited?source.n:apply(source,detail);
 Vector local;
 if(!unit(direction)||!inverse_apply(point(chart.source.before),direction,local))return Verdict::Reference;
 auto wanted=apply(point(chart.source.current),local);
 if(!unit(wanted))return Verdict::Reference;
 sample(emitted,probe.emitted_uv,q,e);
 auto actual=apply(target,decode(e));
 const auto actual_length=length(actual);
 if(actual_length.hi<=1e-8f)return Verdict::Fail;
 if(actual_length.lo<=1e-8f||!unit(actual))return Verdict::Reference;
 const auto separation=length(cross(wanted,actual));
 const auto raw=dot(wanted,actual);
 const Range alignment{max(-1,min(1,raw.lo)),max(-1,min(1,raw.hi))};
 if(alignment.hi<=0)return separation.lo>0||alignment.hi<0?Verdict::Fail:Verdict::Reference;
 // Brackets tan(0.02). The strict interval comparisons also keep equality
 // and the reference atan2's last-bit rounding on the precision path.
 const auto bound=multiply(alignment,{.02000266f,.02000268f});
 if(alignment.lo>0&&separation.hi<bound.lo)return Verdict::Pass;
 if(separation.lo>bound.hi)return Verdict::Fail;
 return Verdict::Reference;
}
}
#undef DOME_CERT_FN
#undef DOME_CERT_ARITH
