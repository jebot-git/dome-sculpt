// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "emission-raster-kernel.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_MATERIAL_FN __host__ __device__ inline
#else
#define DOME_MATERIAL_FN inline
#endif

namespace dome::detail::emission_raster {
DOME_MATERIAL_FN float material_float(uint32_t word){
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
  return __uint_as_float(word);
#else
  return std::bit_cast<float>(word);
#endif
}

struct MaterialView {
  const MaterialEntry *entries=nullptr;
  const paint_compute::Pixel *common=nullptr;
  const paint_compute::ColorPixel *pbr=nullptr;
  const uint8_t *bake_color=nullptr,*bake_orm=nullptr,*bake_coverage=nullptr,*bake_pbr=nullptr;
  paint_compute::Parameters factors;
  float shade_factor[3]{};
  float occlusion_strength=1;
  bool shared_shade=false;
};

DOME_MATERIAL_FN float resolved_shared_shade(float source,float uncovered,float paint,float base,float shade){
  using namespace paint_compute;
  const float contribution=base!=0?paint/base:paint;
  return shade==0?0:mul(shade,add(mul(source,uncovered),contribution));
}
struct MaterialSources {
  uint32_t *values[paint_compute::SourceRoleCount]{};
  DOME_MATERIAL_FN float channel(unsigned role,size_t index,unsigned component) const {
    return values[role]?material_float(values[role][4*index+component]):1.f;
  }
};
struct MaterialCoefficients {
  float color[4]{},orm[3]{},coverage[3]{},ao_coverage=0;
};

DOME_MATERIAL_FN MaterialCoefficients material_texel(const DeviceInput &input,const MaterialView &view,size_t at,bool pbr){
  using namespace paint_compute;
  MaterialCoefficients value;
  const auto owner=input.padding[at];
  if(owner<0)return value;
  const auto &q=*input.quantization;
  if(view.bake_coverage){
    const float coverage=q.unit[view.bake_coverage[owner]];
    const auto *bake_color=pbr&&view.bake_pbr?view.bake_pbr:view.bake_color;
    for(unsigned c=0;c<4;++c){
      const auto channel=bake_color[4*size_t(owner)+c];
      value.color[c]=mul(c==3?q.unit[channel]:q.srgb[channel],coverage);
    }
    for(unsigned c=0;c<3;++c){
      value.orm[c]=mul(q.unit[view.bake_orm[4*size_t(owner)+c]],coverage);
      value.coverage[c]=coverage;
    }
    value.ao_coverage=coverage;
  }
  const auto x=uint32_t(owner)%input.resolution,y=uint32_t(owner)/input.resolution;
  const auto tile=(y/32)*(input.resolution/32)+x/32,within=(y%32)*32+x%32;
  for(unsigned layer=0;layer<input.layer_count;++layer){
    const auto entry=view.entries[size_t(layer)*input.tiles_per_layer+tile];
    if(entry.common==absent)continue;
    const auto &pixel=view.common[size_t(entry.common)*tile_pixels+within].data;
    const float mask=material_float(input.layers[layer].opacity[pixel[13]]);
    const float color=mul(q.unit[pixel[9]],mask);
    if(color!=0){
      const auto *rgba=pbr&&entry.pbr!=absent?view.pbr[size_t(entry.pbr)*tile_pixels+within].rgba:pixel;
      for(unsigned c=0;c<4;++c)
        value.color[c]=blend(value.color[c],c==3?q.unit[rgba[c]]:q.srgb[rgba[c]],color);
      value.coverage[0]=add(mul(value.coverage[0],sub(1,color)),color);
    }
    for(unsigned c=1;c<=2;++c){
      const float coverage=mul(q.unit[pixel[9+c]],mask);
      if(coverage==0)continue;
      value.orm[3-c]=blend(value.orm[3-c],q.unit[pixel[3+c]],coverage);
      value.coverage[c]=add(mul(value.coverage[c],sub(1,coverage)),coverage);
    }
  }
  return value;
}

// All coefficients are nonnegative. Match std::lerp's arithmetic and its
// endpoint clamp while keeping FMA contraction out of byte decisions.
DOME_MATERIAL_FN float material_lerp(float a,float b,float t){
  using namespace paint_compute;
  if(a==0||b==0)return add(mul(t,b),mul(sub(1,t),a));
  if(t==1)return b;
  const float x=add(a,mul(t,sub(b,a)));
  return b>a?(x<b?x:b):(x>b?x:b);
}
DOME_MATERIAL_FN MaterialCoefficients mix_material(MaterialCoefficients a,const MaterialCoefficients &b,float t){
  for(unsigned c=0;c<4;++c)a.color[c]=material_lerp(a.color[c],b.color[c],t);
  for(unsigned c=0;c<3;++c){
    a.orm[c]=material_lerp(a.orm[c],b.orm[c],t);
    a.coverage[c]=material_lerp(a.coverage[c],b.coverage[c],t);
  }
  a.ao_coverage=material_lerp(a.ao_coverage,b.ao_coverage,t);
  return a;
}
DOME_MATERIAL_FN MaterialCoefficients filter_material(const DeviceInput &input,const MaterialView &view,
    SourceTextureAxis column,SourceTextureAxis row,bool pbr=true){
  const auto r=input.resolution;
  const auto filter_row=[&](uint32_t y){
    auto value=material_texel(input,view,size_t(y)*r+column.first,pbr);
    if(column.fraction!=0&&column.first!=column.second)
      value=mix_material(value,material_texel(input,view,size_t(y)*r+column.second,pbr),column.fraction);
    return value;
  };
  auto value=filter_row(row.first);
  if(row.fraction!=0&&row.first!=row.second)value=mix_material(value,filter_row(row.second),row.fraction);
  return value;
}
DOME_MATERIAL_FN paint_compute::Output render_material(const DeviceInput &input,const MaterialView &view,
    FiniteGrid grid,const MaterialSources &source,uint32_t at,size_t source_index){
  using namespace paint_compute;
  const auto x=at%grid.resolution,y=at/grid.resolution;
  const auto value=filter_material(input,view,grid.axis[x],grid.axis[y]);
  const auto &m=view.factors;
  const auto &q=*input.quantization;
  const auto sampled=[&](unsigned role,unsigned component){return source.channel(role,source_index,component);};
  Output output;
  for(unsigned c=0;c<4;++c){
    const float color=add(mul(mul(m.color[c],sampled(SourceColor,c)),sub(1,value.coverage[0])),value.color[c]);
    output.color[c]=c==3?linear_byte(color):srgb_byte(color,q);
    if(c==3&&color<.999f)output.transparent=common_transparency;
  }
  const auto ao=sub(1,mul(view.occlusion_strength,sub(1,sampled(SourceOcclusion,0))));
  output.orm[0]=linear_byte(add(mul(ao,sub(1,value.ao_coverage)),value.orm[0]));
  output.orm[1]=linear_byte(add(mul(mul(m.roughness,sampled(SourceOrm,1)),sub(1,value.coverage[2])),value.orm[1]));
  output.orm[2]=linear_byte(add(mul(mul(m.metallic,sampled(SourceOrm,2)),sub(1,value.coverage[1])),value.orm[2]));
  output.orm[3]=output.emission[3]=255;
  output.color_coverage=linear_byte(value.coverage[0]);
  if(view.shared_shade){
    // HDR supplies a separate emission atlas, so reuse the output for shade.
    for(unsigned c=0;c<3;++c){
      const float shade=resolved_shared_shade(sampled(SourceColor,c),sub(1,value.coverage[0]),
          value.color[c],m.color[c],view.shade_factor[c]);
      if(shade>1.000001f)output.reserved=1;
      output.emission[c]=srgb_byte(shade,q);
    }
    return output;
  }
  uint32_t emission[7]{};
  filter_finite(input,grid,x,y,emission);
  const auto coverage=material_float(emission[6]);
  for(unsigned c=0;c<3;++c){
    const float glow=add(add(mul(mul(m.emission[c],sampled(SourceEmission,c)),sub(1,coverage)),
        material_float(emission[c])),material_float(emission[c+3]));
    if(glow>1)output.reserved=1;
    output.emission[c]=srgb_byte(glow,q);
  }
  output.emission_coverage=linear_byte(coverage);
  return output;
}
}
#undef DOME_MATERIAL_FN
