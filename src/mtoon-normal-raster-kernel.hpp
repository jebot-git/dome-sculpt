// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mtoon-normal-raster.hpp"
#include "portable-normal-raster-kernel.hpp"
#include "dome/paint.hpp"
#include <algorithm>
#include <limits>

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_MTOON_NORMAL_FN __host__ __device__ inline
#else
#define DOME_MTOON_NORMAL_FN inline
#endif

namespace dome::detail::mtoon_normal_raster {
using portable_normal::ImageView;
using paint_compute::add;
using paint_compute::mul;
using paint_compute::sub;

struct View {
 ImageView original,detail;
 const uint8_t *coverage,*visible;
 uint32_t width,height;
 float scale;
 Grid grid;
 Vec3 constant;
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
inline View view(const Input &in,const uint8_t *original,const uint8_t *detail,
                 const uint8_t *coverage,const uint8_t *visible) {
 return {in.original?ImageView{original,in.original->width,in.original->height}:ImageView{},
         {detail,in.detail.width,in.detail.height},coverage,visible,in.width,in.height,in.scale,in.grid,in.constant};
}
inline size_t original_bytes(const Input &in) {return in.original?in.original->rgba.size():0;}
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
  if(cancelled&&cancelled())throw std::runtime_error("MToon normal export cancelled");
  const auto batch=stripe(in,y);
  bound=std::max(bound,run(batch,rgba.subspan(4*size_t(y)*in.width,4*batch.texels())));
  if(bound>.02f)break;
  y+=batch.rows;
 }
 if(cancelled&&cancelled())throw std::runtime_error("MToon normal export cancelled");
 return bound;
}

DOME_MTOON_NORMAL_FN float norm(Vec3 v) {
 return std::sqrt(add(add(mul(v.x,v.x),mul(v.y,v.y)),mul(v.z,v.z)));
}
DOME_MTOON_NORMAL_FN float low_value(float a,float b) {return b<a?b:a;}
DOME_MTOON_NORMAL_FN float high_value(float a,float b) {return a<b?b:a;}
DOME_MTOON_NORMAL_FN Vec3 unit(Vec3 v,float length,Vec3 fallback) {
 return length>1e-12f?Vec3{v.x/length,v.y/length,v.z/length}:fallback;
}
DOME_MTOON_NORMAL_FN Vec3 rgb(const uint8_t *pixel) {
 return {2.f*pixel[0]/255.f-1,2.f*pixel[1]/255.f-1,2.f*pixel[2]/255.f-1};
}
DOME_MTOON_NORMAL_FN float blend_normal(Vec3 source,Vec3 detail,float cover,uint8_t *pixel) {
 const float value[]{paint_compute::blend(source.x,detail.x,cover),
                     paint_compute::blend(source.y,detail.y,cover),
                     paint_compute::blend(source.z,detail.z,cover)};
 for(unsigned k=0;k<3;++k)
  pixel[k]=paint_compute::linear_byte(add(mul(value[k],.5f),.5f));
 const auto decoded=rgb(pixel);
 return norm({decoded.x-value[0],decoded.y-value[1],decoded.z-value[2]});
}
DOME_MTOON_NORMAL_FN Tap encode_native(View in,const paint_compute::Quantization &q,size_t at,uint8_t *pixel) {
 Vec3 original=in.constant;
 if(in.original.rgba){
  original=rgb(in.original.rgba+4*at);
  original.x*=in.scale;
  original.y*=in.scale;
 }
 const auto *detail=in.detail.rgba+4*at;
 const auto baked=rgb(detail);
 const auto cover=q.unit[in.coverage[4*at]];
 for(unsigned k=0;k<4;++k)pixel[k]=detail[k];
 const float error=cover==1?0:blend_normal(original,baked,cover,pixel);
 return {original,baked,norm(original),norm(baked),cover,error};
}
DOME_MTOON_NORMAL_FN void sample(ImageView source,float u,float v,const paint_compute::Quantization &q,
    float (&rgba)[4],const uint8_t *coverage=nullptr) {
 using portable_normal::device_math::clamp_axis;
 const paint_compute::SourcePixelTaps taps{clamp_axis(u,source.width),clamp_axis(v,source.height)};
 paint_compute::sample_source_taps(source.rgba,source.width,false,taps,false,q,rgba,
                                   coverage!=nullptr,coverage);
}
DOME_MTOON_NORMAL_FN Tap encode(View in,Stripe stripe,const paint_compute::Quantization &q,size_t i,uint8_t *pixel) {
 if(in.grid==Grid::Native)return encode_native(in,q,size_t(stripe.y-stripe.halo)*in.width+i,pixel);
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
 pixel[3]=255;
 return {src,dst,source_length,detail_length,coverage,blend_normal(src,dst,coverage,pixel)};
}

// Compare the normalized interpolation of four raw normals with the
// interpolation of their unit directions for any convex weights.
struct Directions {
 Vec3 first{},low{},high{};
 float shortest=std::numeric_limits<float>::infinity(),longest=0,min_dot=1,min_z=1;
 DOME_MTOON_NORMAL_FN void include(Vec3 direction,float length){
  if(longest==0)first=low=high=direction;
  low={low_value(low.x,direction.x),low_value(low.y,direction.y),low_value(low.z,direction.z)};
  high={high_value(high.x,direction.x),high_value(high.y,direction.y),high_value(high.z,direction.z)};
  shortest=low_value(shortest,length);
  longest=high_value(longest,length);
  min_dot=low_value(min_dot,portable_normal::device_math::inner(first,direction));
  min_z=low_value(min_z,direction.z);
 }
 DOME_MTOON_NORMAL_FN float span() const {
  return norm({high.x-low.x,high.y-low.y,high.z-low.z});
 }
 DOME_MTOON_NORMAL_FN float error() const {
  // Normalizing the length-weighted mean costs at most the direction
  // mean's length deficit and twice their covariance over mean length.
  return 1-min_dot+.5f*(longest-shortest)*span()/shortest;
 }
};

struct CoverageBounds {
 float low=1,high=0,quantization=0;
};
DOME_MTOON_NORMAL_FN CoverageBounds coverage_bounds(const Tap *const (&taps)[4]){
 CoverageBounds result;
 for(const auto *tap:taps){
  result.low=low_value(result.low,tap->coverage);
  result.high=high_value(result.high,tap->coverage);
  result.quantization=high_value(result.quantization,tap->quantization);
 }
 return result;
}

DOME_MTOON_NORMAL_FN float independent_cell_bound(const Tap *const (&taps)[4]){
 const auto [min_coverage,max_coverage,quantization]=coverage_bounds(taps);
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

// The native-size output filters raw directions. Bound the cost of
// normalizing each input before blending by the length range of its taps.
struct NormalCone {
 Vec3 reference{};
 float low=1,high=0,min_z=1;
 DOME_MTOON_NORMAL_FN void include(Vec3 raw,float length){
  if(high==0&&length>0)reference=unit(raw,length,{});
  low=low_value(low,portable_normal::device_math::inner(reference,raw));
  high=high_value(high,length);
  min_z=low_value(min_z,raw.z);
 }
 DOME_MTOON_NORMAL_FN float error() const {return high_value(1-low,high-1);}
};

DOME_MTOON_NORMAL_FN float native_cell_bound(const Tap *const (&taps)[4]){
 const auto [min_coverage,max_coverage,quantization]=coverage_bounds(taps);
 // With full coverage both texture reads normalize identical filtered RGB.
 if(min_coverage==1)return 1e-4f;
 NormalCone original,baked;
 Vec3 low=taps[0]->original,high=low;
 for(const auto *tap:taps){
  const auto raw=tap->original;
  original.include(raw,tap->original_length);
  if(tap->coverage>0)baked.include(tap->baked,tap->baked_length);
  low={low_value(low.x,raw.x),low_value(low.y,raw.y),low_value(low.z,raw.z)};
  high={high_value(high.x,raw.x),high_value(high.y,raw.y),high_value(high.z,raw.z)};
 }
 const float covariance=.25f*(max_coverage-min_coverage)*norm({high.x-low.x,high.y-low.y,high.z-low.z});
 const auto original_z=original.min_z/original.high;
 const float min_z=baked.high==0?original_z:low_value(original_z,baked.min_z/baked.high);
 const float delta=quantization+covariance+(1-min_coverage)*original.error()+max_coverage*baked.error()+1.01e-4f;
 return min_z>delta?delta/min_z:1;
}

DOME_MTOON_NORMAL_FN float certify(View in,Stripe stripe,const Tap *taps,size_t i) {
 const auto x=uint32_t(i%in.width),y=stripe.y+uint32_t(i/in.width);
 const bool native=in.grid==Grid::Native;
 const size_t at=i+size_t(stripe.halo)*in.width;
 const auto &tap=taps[at];
 if(native){
  if((tap.coverage<1&&tap.original.z<=0)||(tap.coverage>0&&tap.baked.z<=0))return 1;
 }else if(!x||!y)return 0;
 if(in.visible){
  const auto margin=uint32_t(native);
  const size_t stride=size_t(in.width)+1+margin,cell=size_t(y+margin)*stride+x+margin;
  // Final clamp cells have the same boundary values as their neighbors.
  if(!(in.visible[cell]||(x+1==in.width&&in.visible[cell+1])||
       (y+1==in.height&&in.visible[cell+stride])||
       (x+1==in.width&&y+1==in.height&&in.visible[cell+stride+1])))return 0;
 }
 const size_t above=y?at-in.width:at,left=x?1:0;
 const Tap *corners[]{taps+above-left,taps+above,taps+at-left,&tap};
 return native?native_cell_bound(corners):independent_cell_bound(corners);
}
}
#undef DOME_MTOON_NORMAL_FN
