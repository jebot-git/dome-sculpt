// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mixed-normal-raster.hpp"
#include "portable-normal-raster-kernel.hpp"
#include "dome/paint.hpp"
#include <algorithm>
#include <limits>

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_MIXED_FN __host__ __device__ inline
#else
#define DOME_MIXED_FN inline
#endif

namespace dome::detail::mixed_normal {
using portable_normal::ImageView;
using paint_compute::add;
using paint_compute::mul;
using paint_compute::sub;

struct View {
 ImageView original,detail;
 const uint8_t *coverage,*visible;
 uint32_t width,height;
 float scale;
};
struct Tap {
 Vec3 original,baked;
 float original_length,baked_length,coverage,quantization;
};
struct Stripe {
 uint32_t y,rows,halo,width;
 size_t texels() const {return size_t(rows)*width;}
 size_t scratch() const {return size_t(rows+halo)*width;}
 size_t groups() const {return (texels()+group_size-1)/group_size;}
};
inline auto view(const Image &image,const uint8_t *rgba) {
 return ImageView{rgba,image.width,image.height};
}
inline Stripe stripe(const Input &in,uint32_t y) {
 // Recompute one previous row so each batch can certify its top cells
 // with no dependency on device scratch overwritten by the next batch.
 const auto halo=uint32_t(y!=0);
 return {y,std::min(batch_texels/in.width-halo,in.height-y),halo,in.width};
}
inline size_t capacity(const Input &in) {
 return std::min(size_t(batch_texels),size_t(in.width)*in.height);
}
void validate(const Input &,std::span<uint8_t>);
template<class Run>
float visit_stripes(const Input &in,std::span<uint8_t> rgba,const std::function<bool()> &cancelled,Run run) {
 float bound=0;
 for(uint32_t y=0;y<in.height;){
  if(cancelled&&cancelled())throw std::runtime_error("Mixed normal export cancelled");
  const auto batch=stripe(in,y);
  bound=std::max(bound,run(batch,rgba.subspan(4*size_t(y)*in.width,4*batch.texels())));
  if(bound>.02f)break;
  y+=batch.rows;
 }
 if(cancelled&&cancelled())throw std::runtime_error("Mixed normal export cancelled");
 return bound;
}

DOME_MIXED_FN float norm(Vec3 v) {
 return std::sqrt(add(add(mul(v.x,v.x),mul(v.y,v.y)),mul(v.z,v.z)));
}
DOME_MIXED_FN float low_value(float a,float b) {return b<a?b:a;}
DOME_MIXED_FN float high_value(float a,float b) {return a<b?b:a;}
DOME_MIXED_FN Vec3 unit(Vec3 v,float length,Vec3 fallback) {
 return length>1e-12f?Vec3{v.x/length,v.y/length,v.z/length}:fallback;
}
DOME_MIXED_FN void sample(ImageView source,float u,float v,const paint_compute::Quantization &q,
    float (&rgba)[4],const uint8_t *coverage=nullptr) {
 using portable_normal::device_math::clamp_axis;
 const paint_compute::SourcePixelTaps taps{clamp_axis(u,source.width),clamp_axis(v,source.height)};
 paint_compute::sample_source_taps(source.rgba,source.width,false,taps,false,q,rgba,
                                   coverage!=nullptr,coverage);
}
DOME_MIXED_FN Tap encode(View in,Stripe stripe,const paint_compute::Quantization &q,size_t i,uint8_t *pixel) {
 const float u=float(i%stripe.width)/in.width;
 const float v=float(stripe.y-stripe.halo+i/stripe.width)/in.height;
 float a[4],b[4];
 sample(in.original,u,v,q,a);
 sample(in.detail,u,v,q,b,in.coverage);
 const float coverage=b[3]<0?0:b[3]>1?1:b[3];
 const Vec3 raw_source{mul(sub(mul(2,a[0]),1),in.scale),mul(sub(mul(2,a[1]),1),in.scale),sub(mul(2,a[2]),1)};
 const Vec3 raw_detail=coverage>1e-8f?
     Vec3{sub(mul(2,b[0]),coverage)/coverage,sub(mul(2,b[1]),coverage)/coverage,
          sub(mul(2,b[2]),coverage)/coverage}:Vec3{0,0,1};
 const float source_length=norm(raw_source),detail_length=norm(raw_detail);
 const auto src=unit(raw_source,source_length,{0,1,0});
 const auto dst=unit(raw_detail,detail_length,{0,1,0});
 const float value[]{paint_compute::blend(src.x,dst.x,coverage),
                     paint_compute::blend(src.y,dst.y,coverage),
                     paint_compute::blend(src.z,dst.z,coverage)};
 float error[3];
 for(unsigned k=0;k<3;++k){
  const float encoded=add(mul(value[k],.5f),.5f);
  pixel[k]=paint_compute::linear_byte(encoded);
  error[k]=sub(sub(mul(2,float(pixel[k]))/255.f,1),value[k]);
 }
 pixel[3]=255;
 return {src,dst,source_length,detail_length,coverage,norm({error[0],error[1],error[2]})};
}

// Compare the normalized interpolation of four raw normals with the
// interpolation of their unit directions for any convex weights.
struct Directions {
 Vec3 first{},low{},high{};
 float shortest=std::numeric_limits<float>::infinity(),longest=0,min_dot=1,min_z=1;
 DOME_MIXED_FN void include(Vec3 direction,float length){
  if(longest==0)first=low=high=direction;
  low={low_value(low.x,direction.x),low_value(low.y,direction.y),low_value(low.z,direction.z)};
  high={high_value(high.x,direction.x),high_value(high.y,direction.y),high_value(high.z,direction.z)};
  shortest=low_value(shortest,length);
  longest=high_value(longest,length);
  min_dot=low_value(min_dot,portable_normal::device_math::inner(first,direction));
  min_z=low_value(min_z,direction.z);
 }
 DOME_MIXED_FN float span() const {
  return norm({high.x-low.x,high.y-low.y,high.z-low.z});
 }
 DOME_MIXED_FN float error() const {
  // Normalizing the length-weighted mean costs at most the direction
  // mean's length deficit and twice their covariance over mean length.
  return 1-min_dot+.5f*(longest-shortest)*span()/shortest;
 }
};

DOME_MIXED_FN float cell_bound(const Tap *const (&taps)[4]){
 float min_coverage=1,max_coverage=0,quantization=0;
 for(const auto *tap:taps){
  min_coverage=low_value(min_coverage,tap->coverage);
  max_coverage=high_value(max_coverage,tap->coverage);
  quantization=high_value(quantization,tap->quantization);
 }
 // Allow for frame, sampler and binary32 arithmetic error.
 float delta=quantization+1.01e-4f,minimum_z=1;
 if(min_coverage<1){
  Directions original;
  for(const auto *tap:taps)original.include(tap->original,tap->original_length);
  if(!(original.min_z>0&&original.shortest>1e-12f))return 1;
  minimum_z=original.min_z;
  delta+=(1-min_coverage)*original.error()+.25f*(max_coverage-min_coverage)*original.span();
 }
 if(max_coverage>0){
  Directions detail;
  for(const auto *tap:taps)if(tap->coverage>0)detail.include(tap->baked,tap->baked_length);
  if(!(detail.min_z>0&&detail.shortest>1e-12f))return 1;
  minimum_z=low_value(minimum_z,detail.min_z);
  // The covered contribution already uses coverage-weighted interpolation
  // weights for both the raw vectors and their directions.
  delta+=max_coverage*detail.error();
 }
 return minimum_z>delta?delta/minimum_z:1;
}

DOME_MIXED_FN float certify(View in,Stripe stripe,const Tap *taps,size_t i) {
 const auto x=uint32_t(i%in.width),y=stripe.y+uint32_t(i/in.width);
 if(!x||!y)return 0;
 if(in.visible){
  const size_t stride=size_t(in.width)+1,at=size_t(y)*stride+x;
  // Final clamp cells have the same boundary values as their neighbors.
  if(!(in.visible[at]||(x+1==in.width&&in.visible[at+1])||
       (y+1==in.height&&in.visible[at+stride])||
       (x+1==in.width&&y+1==in.height&&in.visible[at+stride+1])))return 0;
 }
 const size_t at=i+size_t(stripe.halo)*in.width,above=at-in.width;
 const Tap *corners[]{taps+above-1,taps+above,taps+at-1,taps+at};
 return cell_bound(corners);
}
}
#undef DOME_MIXED_FN
