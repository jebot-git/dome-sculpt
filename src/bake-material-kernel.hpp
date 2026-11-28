// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-material-device.hpp"
#include "bake-texture-filter-kernel.hpp"
#include "source-uv-sampler-kernel.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_BAKE_MATERIAL_INLINE __host__ __device__ inline
#else
#define DOME_BAKE_MATERIAL_INLINE inline
#endif

namespace dome::detail::bake_material {
struct ImageView {
  MappedSourceImage image;
  bool original;
};
struct View {
  ImageView images[RoleCount];
  Parameters parameters;
  CoordinateLayout coordinates;
};

struct SampleOutput {
  ResultLayout layout;
  float *rows;
};

// Copy each active channel as binary32. The host decodes the same field order.
DOME_BAKE_MATERIAL_INLINE void sample_channels(ResultLayout layout,auto &value,auto copy){
  copy(value.color,4);
  copy(&value.roughness,1);
  copy(&value.metallic,1);
  copy(&value.occlusion,1);
  if(layout.emission)copy(value.emission,3);
  if(layout.pbr)copy(value.pbr,4);
  if(layout.normal)copy(value.normal,4);
  if(layout.baked_normal)copy(value.baked_normal,4);
}

DOME_BAKE_MATERIAL_INLINE void write_sample(ResultLayout layout,float *row,const Values &value){
  *row++=float(value.invalid);
  sample_channels(layout,value,[&](const float *channel,unsigned count){
    for(unsigned c=0;c<count;++c)*row++=channel[c];
  });
}

DOME_BAKE_MATERIAL_INLINE Values read_sample(ResultLayout layout,const float *row){
  Values value{};
  value.invalid=*row++!=0;
  sample_channels(layout,value,[&](float *channel,unsigned count){
    for(unsigned c=0;c<count;++c)channel[c]=*row++;
  });
  return value;
}

DOME_BAKE_MATERIAL_INLINE bool sample_image(const ImageView &view,const Uv &uv,
    const paint_compute::Quantization &table,float (&color)[4]){
  using namespace paint_compute;
  const auto &image=view.image;
  if(!finite_uv(uv))return false;
  if(view.original){
    sample_mapped_source_channels(image,{uv[0],uv[1]},table,color);
    return true;
  }
  // Baked maps address the FLOAT texel centre before repeat or clamp.
  // Original glTF maps wrap in source coordinates before this boundary.
  const float x=sub(mul(uv[0],float(image.width)),.5f),y=sub(mul(uv[1],float(image.height)),.5f);
  if(!(x>=-1e9f&&x<=1e9f&&y>=-1e9f&&y<=1e9f))return false;
  const SourcePixelTaps taps{legacy_axis(x,image.width,image.wrap_s==10497),legacy_axis(y,image.height,image.wrap_t==10497)};
  sample_source_taps(image.rgba,image.width,false,taps,image.srgb,table,color);
  return true;
}

DOME_BAKE_MATERIAL_INLINE Values evaluate(const View &view,const Uv *coordinates,
    const paint_compute::Quantization &table){
  using namespace paint_compute;
  const auto &params=view.parameters;
  Values result{};
  for(unsigned c=0;c<4;++c)result.color[c]=params.color[c];
  for(unsigned c=0;c<3;++c)result.emission[c]=params.emission[c];
  result.roughness=params.roughness;
  result.metallic=params.metallic;
  result.occlusion=1.f;
  const bool factor=params.original||!params.composited;
  for(unsigned role=0;role<RoleCount;++role){
    const auto &image=view.images[role];
    if(!image.image.rgba)continue;
    float rgba[4];
    if(!sample_image(image,coordinates[view.coordinates.roles[role]],table,rgba)){result.invalid=1;return result;}
    switch(role){
    case Color:
      for(unsigned c=0;c<4;++c)result.color[c]=mul(rgba[c],factor?result.color[c]:1.f);
      break;
    case Orm:
      result.roughness=mul(rgba[1],factor?result.roughness:1.f);
      result.metallic=mul(rgba[2],factor?result.metallic:1.f);
      if(!params.original)result.occlusion=rgba[0];
      break;
    case Occlusion:
      result.occlusion=sub(1.f,mul(params.occlusion_strength,sub(1.f,rgba[0])));
      break;
    case Emission:
      for(unsigned c=0;c<3;++c)result.emission[c]=mul(rgba[c],factor?result.emission[c]:1.f);
      break;
    case PbrColor:
      for(unsigned c=0;c<4;++c)result.pbr[c]=rgba[c];
      break;
    case TangentNormal:
      for(unsigned c=0;c<4;++c)result.normal[c]=rgba[c];
      break;
    }
  }
  return result;
}

DOME_BAKE_MATERIAL_INLINE void evaluate_sample(const View &view,const Uv *coordinates,
    const paint_compute::Quantization &table,SampleOutput output,size_t sample){
  const auto value=evaluate(view,coordinates+sample*view.coordinates.count,table);
  write_sample(output.layout,output.rows+sample*output.layout.stride,value);
}
} // namespace dome::detail::bake_material
#undef DOME_BAKE_MATERIAL_INLINE
