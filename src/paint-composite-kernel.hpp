// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <cstddef>
#include <bit>
#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_PC_FN __host__ __device__ inline
#else
#define DOME_PC_FN inline
#endif
namespace dome::paint_compute {
struct Pixel { uint8_t data[14]{}; };
struct Output { uint8_t color[4]{},orm[4]{},emission[4]{};uint8_t color_coverage=0,emission_coverage=0,transparent=0,reserved=0; };
struct Layer {float opacity=1;uint32_t first_tile=0;};
struct Parameters {float color[4]{},emission[3]{},metallic=0,roughness=0;uint32_t resolution=0,layers=0,tiles=0;};
struct Quantization {float unit[256]{},srgb[256]{};uint32_t threshold[255]{};uint32_t reserved=0;};
inline constexpr uint32_t absent=UINT32_MAX;
// Volatile float expression boundaries prohibit contraction without changing
// brush compilation flags or relying on the device's transcendental library.
DOME_PC_FN float mul(float a,float b){volatile float r=a*b;return r;}
DOME_PC_FN float add(float a,float b){volatile float r=a+b;return r;}
DOME_PC_FN float sub(float a,float b){volatile float r=a-b;return r;}
DOME_PC_FN uint32_t bits(float value){
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
 return __float_as_uint(value);
#else
 return std::bit_cast<uint32_t>(value);
#endif
}
DOME_PC_FN uint8_t linear_byte(float v){
 if(v<=0)return 0;if(v>=1)return 255;
 float scaled=mul(v,255);uint32_t integer=uint32_t(scaled);
 return uint8_t(integer+(sub(scaled,float(integer))>=.5f));
}
DOME_PC_FN uint8_t srgb_byte(float v,const Quantization &q){
 if(v<=0)return 0;if(v>=1)return 255;
 const auto word=bits(v);uint32_t lo=0,hi=255;
 while(lo<hi){const auto mid=(lo+hi)/2;if(word>=q.threshold[mid])lo=mid+1;else hi=mid;}
 return uint8_t(lo);
}
DOME_PC_FN float blend(float base,float paint,float a){return add(mul(base,sub(1,a)),mul(paint,a));}
DOME_PC_FN Output compose(uint32_t at,const Parameters &p,const Layer *layers,const uint32_t *tiles,
                         const Pixel *pixels,const int32_t *padding,const Quantization &q){
 float color[]{p.color[0],p.color[1],p.color[2],p.color[3]};
 float emission[]{p.emission[0],p.emission[1],p.emission[2]};
 float rough=p.roughness,metal=p.metallic,cc=0,ec=0;const auto owner=padding[at];
 if(owner>=0){
  const auto x=uint32_t(owner)%p.resolution,y=uint32_t(owner)/p.resolution;
  const auto tile=(y/32)*((p.resolution+31)/32)+x/32,offset=(y%32)*32+x%32;
  for(uint32_t li=0;li<p.layers;++li){
   const auto id=tiles[layers[li].first_tile+tile];if(id==absent)continue;
   const auto &v=pixels[size_t(id)*1024+offset];const auto mask=mul(q.unit[v.data[13]],layers[li].opacity);
   const auto ca=mul(mask,q.unit[v.data[9]]),ea=mul(mask,q.unit[v.data[12]]);
   cc=add(mul(cc,sub(1,ca)),ca);ec=add(mul(ec,sub(1,ea)),ea);
   for(uint32_t c=0;c<4;++c)color[c]=blend(color[c],c<3?q.srgb[v.data[c]]:q.unit[v.data[c]],ca);
   rough=blend(rough,q.unit[v.data[5]],mul(mask,q.unit[v.data[11]]));
   metal=blend(metal,q.unit[v.data[4]],mul(mask,q.unit[v.data[10]]));
   for(uint32_t c=0;c<3;++c)emission[c]=blend(emission[c],q.srgb[v.data[6+c]],ea);
  }
 }
 Output out;for(uint32_t c=0;c<3;++c){out.color[c]=srgb_byte(color[c],q);out.emission[c]=srgb_byte(emission[c],q);}
 out.color[3]=linear_byte(color[3]);out.orm[0]=255;out.orm[1]=linear_byte(rough);out.orm[2]=linear_byte(metal);out.orm[3]=out.emission[3]=255;
 out.color_coverage=linear_byte(cc);out.emission_coverage=linear_byte(ec);out.transparent=color[3]<.999f;return out;
}
static_assert(sizeof(Pixel)==14&&sizeof(Output)==16&&sizeof(Layer)==8&&sizeof(Parameters)==48&&sizeof(Quantization)==3072);
}
#undef DOME_PC_FN
