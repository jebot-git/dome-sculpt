// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "emission-raster-device.hpp"
#include "paint-composite-kernel.hpp"
#include "paint-source-coordinate-kernel.hpp"
#include "source-uv-sampler-kernel.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_EMISSION_FN __host__ __device__ inline
#else
#define DOME_EMISSION_FN inline
#endif
#if defined(__SYCL_DEVICE_ONLY__)
// Keep the integer binary64 implementation shared between the many blend
// and resolve operations in the Arc program instead of cloning its body.
#define DOME_EMISSION_MATH [[clang::noinline]] static inline
#else
#define DOME_EMISSION_MATH DOME_EMISSION_FN
#endif

namespace dome::detail::emission_raster {
struct DeviceInput {
  const int32_t *padding=nullptr;
  const Layer *layers=nullptr;
  const Entry *entries=nullptr;
  const CommonPixel *common=nullptr;
  const PackedField *physical=nullptr;
  const uint32_t *physical_values=nullptr;
  const paint_compute::Quantization *quantization=nullptr;
  Bake bake;
  Source source;
  uint32_t background[3]{},strength=0,resolution=0,layer_count=0,tiles_per_layer=0;
};
struct FiniteGrid {
  const SourceTextureAxis *axis=nullptr;
  uint32_t resolution=0;
};

inline constexpr uint32_t one32=0x3f800000, max32=0x7f7fffff, invalid=0x7fc00000, invalid_uv=0x7fc00001;
inline constexpr uint32_t radiance_overflow=0x7fc00002;
inline constexpr uint64_t one64=0x3ff0000000000000ull, sign64=1ull<<63;

DOME_EMISSION_FN uint32_t magnitude(uint32_t value){return value&0x7fffffffu;}
DOME_EMISSION_FN uint64_t widen(uint32_t value){return paint_grid::widen_float_bits(value);}
DOME_EMISSION_FN uint32_t narrow(uint64_t value){return paint_grid::narrow_float_bits(value);}
DOME_EMISSION_MATH uint64_t product64(uint64_t a,uint64_t b){return paint_grid::mul(a,b);}
DOME_EMISSION_MATH uint64_t sum64(uint64_t a,uint64_t b){return paint_grid::add(a,b);}
DOME_EMISSION_FN uint32_t product(uint32_t a,uint32_t b){return narrow(product64(widen(a),widen(b)));}
DOME_EMISSION_FN uint32_t sum(uint32_t a,uint32_t b){return narrow(sum64(widen(a),widen(b)));}
DOME_EMISSION_FN uint64_t complement(uint32_t value){return sum64(one64,widen(value)^sign64);}
DOME_EMISSION_MATH uint32_t finite_over(uint32_t below,uint32_t above,uint32_t coverage,uint32_t remaining){
  return sum(product(below,remaining),product(above,coverage));
}
DOME_EMISSION_FN bool bounded(uint32_t value,uint32_t limit){
  return magnitude(value)==0||value<=limit;
}
DOME_EMISSION_FN void unpack(PackedField field,const uint32_t *data,size_t index,uint32_t (&result)[7]){
  size_t next=field.offset+index*field.stride;
  for(unsigned k=0;k<7;++k)result[k]=field.mask&(1u<<k)?data[next++]:field.constants[k];
}
DOME_EMISSION_FN void base(const Bake &bake,const paint_compute::Quantization &q,size_t at,uint32_t (&value)[7]){
  if(bake.physical){
    if(bake.dense){
      for(unsigned k=0;k<7;++k)value[k]=bake.dense[at*7+k];
    }else{
      unpack(bake.packed,bake.varying,at,value);
      if(bake.coverage_bits)value[6]=((bake.coverage_bits[at/32]>>(at%32))&1u)?one32:0;
    }
  }else if(bake.coverage){
    value[6]=paint_compute::bits(q.unit[bake.coverage[at]]);
    for(unsigned k=0;k<3;++k)
      value[k]=product(paint_compute::bits(q.srgb[bake.legacy[at*4+k]]),value[6]);
  }
}
DOME_EMISSION_FN bool valid_coefficients(const uint32_t (&value)[7],bool physical){
  if(!bounded(value[6],one32))return false;
  const auto limit=product(physical?max32:std::bit_cast<uint32_t>(maximum_native_emission),value[6]);
  for(unsigned k=0;k<3;++k)
    if(!bounded(value[k],magnitude(value[6]))||!bounded(value[k+3],magnitude(limit)))return false;
  return true;
}
template<bool finite=false>
DOME_EMISSION_FN bool apply_layer(const DeviceInput &input,size_t layer,Entry entry,size_t within,uint32_t (&value)[7]){
  if(entry.common==absent&&entry.physical==absent)return true;
  const CommonPixel common=entry.common==absent?CommonPixel{{0,0,0},0,255}:
    input.common[size_t(entry.common)*tile_pixels+within];
  uint32_t paint[7]{};
  const auto &q=*input.quantization;
  if(entry.physical!=absent)unpack(input.physical[entry.physical],input.physical_values,within,paint);
  else{
    for(unsigned k=0;k<3;++k)paint[k]=paint_compute::bits(q.srgb[common.legacy[k]]);
    paint[6]=paint_compute::bits(q.unit[common.coverage]);
  }
  const auto opacity=product(input.layers[layer].opacity[common.mask],paint[6]);
  if(!magnitude(opacity))return true;
  if constexpr(!finite)if(!valid_coefficients(value,input.bake.physical)||!bounded(opacity,one32))return false;
  const auto remaining=complement(opacity);
  if constexpr(!finite){
    if(input.bake.physical){
      for(unsigned k=0;k<7;++k){
        const auto above=k==6?opacity:product(paint[k],opacity);
        value[k]=narrow(sum64(sum64(0,widen(above)),product64(widen(value[k]),remaining)));
      }
      return true;
    }
  }
  const auto residual=narrow(remaining);
  for(unsigned k=0;k<6;++k){
    if constexpr(finite)value[k]=finite_over(value[k],paint[k],opacity,residual);
    else value[k]=sum(product(value[k],residual),product(paint[k],opacity));
  }
  value[6]=sum(product(value[6],residual),opacity);
  return true;
}
DOME_EMISSION_FN bool typed_taps(const Source &source,uint32_t x,uint32_t y,paint_compute::SourcePixelTaps &taps){
  const auto &job=source.coordinates;
  const auto column=job.columns[x],row=job.rows[y];
  const auto u=sum64(sum64(column.u,row.u),job.transform.offset_u);
  const auto v=sum64(sum64(column.v,row.v),job.transform.offset_v);
  if((u&~sign64)>widen(max32)||(v&~sign64)>widen(max32))return false;
  // mtoon_uv returns float UVs before the independently retained image
  // sampler applies its affine transform in binary64.
  const auto uf=widen(narrow(u)),vf=widen(narrow(v));
  const auto *m=source.sampler_affine;
  const auto s=sum64(sum64(product64(widen(m[0]),uf),product64(widen(m[1]),vf)),widen(m[4]));
  const auto t=sum64(sum64(product64(widen(m[2]),uf),product64(widen(m[3]),vf)),widen(m[5]));
  taps={paint_compute::coordinates::axis(s,job.width,job.transform.wrap_s,job.nearest),
        paint_compute::coordinates::axis(t,job.height,job.transform.wrap_t,job.nearest)};
  return true;
}

template<bool bake_only=false,bool typed=false,bool mapped=false>
DOME_EMISSION_FN void render(const DeviceInput &input,size_t at,uint32_t *destination,
    MappedSourceImage image={},SourceUvSampler::Coordinate uv={}){
  const auto owner=input.padding[at];
  const auto source=owner<0?at:size_t(owner);
  if(source>=size_t(input.resolution)*input.resolution){destination[0]=invalid;return;}
  uint32_t value[7]{};
  base(input.bake,*input.quantization,source,value);
  uint32_t background[]{input.background[0],input.background[1],input.background[2]};
  if constexpr(!bake_only){
    const auto x=uint32_t(source%input.resolution),y=uint32_t(source/input.resolution);
    const auto tile=(y/32)*(input.resolution/32)+x/32,within=(y%32)*32+x%32;
    for(unsigned layer=0;layer<input.layer_count;++layer){
      if(!apply_layer(input,layer,input.entries[size_t(layer)*input.tiles_per_layer+tile],within,value)){
        destination[0]=invalid;
        return;
      }
    }
    if constexpr(mapped){
      uint32_t sampled[4]{};
      if(owner>=0)sample_mapped_source(image,uv,*input.quantization,sampled,0);
      for(unsigned k=0;k<3;++k)background[k]=product(background[k],sampled[k]);
    }else if(input.source.rgba){
      const auto &job=input.source.coordinates;
      paint_compute::SourcePixelTaps taps;
      if constexpr(typed){
        if(!typed_taps(input.source,x,y,taps)){destination[0]=invalid_uv;return;}
      }else taps=paint_compute::source_coordinates(job,x,y);
      float source_rgba[4];
      paint_compute::sample_source_taps(input.source.rgba,job.width,job.nearest,taps,true,*input.quantization,source_rgba);
      for(unsigned k=0;k<3;++k)background[k]=product(background[k],paint_compute::bits(source_rgba[k]));
    }
  }
  if(!valid_coefficients(value,input.bake.physical)){
    destination[0]=invalid;
    return;
  }
  const auto residual=input.bake.physical?complement(value[6]):widen(narrow(complement(value[6])));
  for(unsigned k=0;k<3;++k){
    const auto shade=product64(widen(background[k]),residual);
    const auto legacy=product64(widen(value[k]),widen(input.strength));
    const auto radiance=sum64(sum64(shade,legacy),widen(value[k+3]));
    if((radiance&~sign64)>widen(max32)&&input.bake.physical){destination[0]=invalid;return;}
    destination[k]=narrow(radiance);
  }
  for(unsigned k=0;k<7;++k)destination[3+k]=value[k];
}

// The finite view uses the editor's FLOAT over and bilinear filtering of
// separately premultiplied roles, before resolving them against the material.
DOME_EMISSION_FN void authoring_texel(const DeviceInput &input,size_t at,uint32_t (&value)[7]){
  const auto owner=input.padding[at];
  if(owner<0)return;
  base(input.bake,*input.quantization,size_t(owner),value);
  const auto x=uint32_t(owner)%input.resolution,y=uint32_t(owner)/input.resolution;
  const auto tile=(y/32)*(input.resolution/32)+x/32,within=(y%32)*32+x%32;
  for(unsigned layer=0;layer<input.layer_count;++layer)
    apply_layer<true>(input,layer,input.entries[size_t(layer)*input.tiles_per_layer+tile],within,value);
}

DOME_EMISSION_MATH uint32_t finite_lerp(uint32_t a,uint32_t b,uint32_t t){
  // All seven coefficients are nonnegative. Match the reference lerp's
  // zero endpoint formula and monotonic clamp for positive endpoints.
  if(!magnitude(a)||!magnitude(b))return sum(product(t,b),product(narrow(complement(t)),a));
  if(t==one32)return b;
  const auto x=sum(a,product(t,sum(b,a^0x80000000u)));
  return b>a?(x<b?x:b):(x>b?x:b);
}

DOME_EMISSION_FN void filter_finite(const DeviceInput &input,FiniteGrid grid,uint32_t x,uint32_t y,
    uint32_t (&value)[7]){
  const auto column=grid.axis[x],row=grid.axis[y];
  const auto fx=paint_compute::bits(column.fraction),fy=paint_compute::bits(row.fraction);
  const unsigned rows=fy&&row.first!=row.second?2:1,columns=fx&&column.first!=column.second?2:1;
  for(unsigned j=0;j<rows;++j){
    uint32_t filtered[7]{};
    for(unsigned i=0;i<columns;++i){
      uint32_t sample[7]{};
      const size_t texel=size_t(j?row.second:row.first)*input.resolution+(i?column.second:column.first);
      authoring_texel(input,texel,sample);
      for(unsigned k=0;k<7;++k)filtered[k]=i?finite_lerp(filtered[k],sample[k],fx):sample[k];
    }
    for(unsigned k=0;k<7;++k)value[k]=j?finite_lerp(value[k],filtered[k],fy):filtered[k];
  }

}

DOME_EMISSION_FN void resolve_finite(const DeviceInput &input,const uint32_t (&value)[7],
    const uint32_t *source,uint32_t *out){
  const auto residual=widen(narrow(complement(value[6])));
  constexpr auto limit=std::bit_cast<uint64_t>(30000.0);
  for(unsigned k=0;k<3;++k){
    const auto background=product64(widen(input.background[k]),widen(source[k]));
    const auto shade=product64(background,residual);
    const auto radiance=sum64(sum64(shade,widen(value[k])),widen(value[k+3]));
    if((radiance&~sign64)>limit||(radiance>sign64)){
      out[0]=invalid;
      return;
    }
    out[k]=narrow(radiance);
  }
}

DOME_EMISSION_FN void render_finite(const DeviceInput &input,FiniteGrid grid,size_t at,uint32_t *out){
  const auto x=uint32_t(at%grid.resolution),y=uint32_t(at/grid.resolution);
  uint32_t value[7]{},source[]{one32,one32,one32};
  filter_finite(input,grid,x,y,value);
  if(input.source.rgba){
    const auto &job=input.source.coordinates;
    float rgba[4];
    paint_compute::sample_source_taps(input.source.rgba,job.width,job.nearest,
        paint_compute::source_coordinates(job,x,y),true,*input.quantization,rgba);
    for(unsigned k=0;k<3;++k)source[k]=paint_compute::bits(rgba[k]);
  }
  resolve_finite(input,value,source,out);
}

DOME_EMISSION_FN void render_mapped(const DeviceInput &input,FiniteGrid grid,MappedSourceImage image,
    EmissionRasterDevice::MappedSample sample,uint32_t *out){
  uint32_t value[7]{},source[4]{one32,one32,one32,one32};
  filter_finite(input,grid,sample.texel%grid.resolution,sample.texel/grid.resolution,value);
  if(image.rgba)sample_mapped_source(image,{sample.u,sample.v},*input.quantization,source,0);
  resolve_finite(input,value,source,out);
}

template<bool mapped=false>
DOME_EMISSION_FN void render_texture_set(const DeviceInput &input,size_t at,uint32_t *out,bool coefficients){
  uint32_t value[7]{};
  authoring_texel(input,at,value);
  if(!valid_coefficients(value,true)){
    out[0]=invalid;
    return;
  }
  uint32_t background[]{input.background[0],input.background[1],input.background[2]};
  const auto &source=input.source;
  if constexpr(mapped){
    const auto x=uint32_t(at%input.resolution),y=uint32_t(at/input.resolution);
    const auto &job=source.coordinates;
    float rgba[4];
    paint_compute::sample_source_taps(source.rgba,job.width,job.nearest,
        paint_compute::source_coordinates(job,x,y),true,*input.quantization,rgba);
    for(unsigned k=0;k<3;++k)background[k]=product(background[k],paint_compute::bits(rgba[k]));
  }else if(source.aligned_mtoon_linear){
    const auto *pixel=source.rgba+4*at;
    for(unsigned k=0;k<3;++k)
      background[k]=product(background[k],paint_compute::bits(source.aligned_mtoon_linear[pixel[k]]));
  }
  // Texture sets resolve the authored binary32 coefficients in binary64,
  // with the backdrop sampled at the display texel, including in padding.
  const auto residual=complement(value[6]);
  for(unsigned k=0;k<3;++k){
    const auto shade=product64(widen(background[k]),residual);
    const auto legacy=product64(widen(value[k]),widen(input.strength));
    const auto radiance=sum64(sum64(shade,legacy),widen(value[k+3]));
    if((radiance&~sign64)>widen(max32)){
      out[0]=radiance_overflow;
      return;
    }
    out[k]=narrow(radiance);
  }
  if(coefficients)for(unsigned k=0;k<7;++k)out[3+k]=value[k];
  else out[3]=value[6];
}
}
#undef DOME_EMISSION_MATH
#undef DOME_EMISSION_FN
