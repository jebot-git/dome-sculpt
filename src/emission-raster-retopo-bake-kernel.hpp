// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "emission-raster-kernel.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_RETOPO_BAKE_FN __host__ __device__ inline
#else
#define DOME_RETOPO_BAKE_FN inline
#endif

namespace dome::detail::emission_raster {
DOME_RETOPO_BAKE_FN EmissionRasterDevice::RetopoBakeTexel retopo_bake_texel(uint32_t texel,
    const BakeUvMaps &maps,const Bake &bake,RetopoBakeFormat format){
  EmissionRasterDevice::RetopoBakeTexel result;
  const auto rgba=size_t(texel)*4;
  result.coverage=maps.coverage[texel];
  for(unsigned c=0;c<4;++c){
    result.color[c]=maps.color[rgba+c];
    if(format.resolved)result.pbr[c]=maps.pbr[rgba+c];
  }
  for(unsigned c=0;c<3;++c){
    result.orm[c]=maps.orm[rgba+c];
    if(!format.physical)result.legacy[c]=maps.legacy[rgba+c];
    if(format.normals)result.normal[c]=maps.normals[0][rgba+c];
  }
  if(format.physical){
    uint32_t coefficient[7];
    physical_base(bake,texel,coefficient);
    for(unsigned c=0;c<3;++c){
      result.emission.legacy[c]=std::bit_cast<float>(coefficient[c]);
      result.emission.absolute[c]=std::bit_cast<float>(coefficient[c+3]);
    }
    result.emission.coverage=std::bit_cast<float>(coefficient[6]);
  }
  return result;
}

// Keep color and normal values in linear FLOAT through source compositing
// and transport between two different tangent frames. Bake UV editing can
// quantize sooner because it works on a single retained mesh.
DOME_RETOPO_BAKE_FN EmissionRasterDevice::RetopoBakePixel filter_retopo_bake(
    const EmissionRasterDevice::BakeUvSample &sample,const BakeUvMaps &maps,const Bake &bake,
    const EmissionRasterDevice::RetopoBakeTexel *staged,RetopoBakeFormat format){
  using paint_compute::add;
  using paint_compute::mul;
  using paint_compute::sub;
  const auto &q=*maps.quantization;
  EmissionRasterDevice::RetopoBakePixel result;
  uint32_t emission[7]{};
  for(unsigned i=0;i<4;++i){
    const auto weight=sample.weights[i];
    if(!weight)continue;
    const auto texel=sample.texels[i];
    const auto p=staged?staged[texel]:retopo_bake_texel(texel,maps,bake,format);
    const auto alpha=mul(weight,q.unit[p.coverage]);
    if(alpha>0){
      result.coverage=add(result.coverage,alpha);
      result.material[3]=add(result.material[3],mul(q.unit[p.color[3]],alpha));
      if(format.resolved)result.pbr[3]=add(result.pbr[3],mul(q.unit[p.pbr[3]],alpha));
      for(unsigned c=0;c<3;++c){
        result.material[c]=add(result.material[c],mul(q.srgb[p.color[c]],alpha));
        result.material[4+c]=add(result.material[4+c],mul(q.unit[p.orm[c]],alpha));
        if(!format.physical)result.material[7+c]=add(result.material[7+c],mul(q.srgb[p.legacy[c]],alpha));
        const auto normal=format.normals?sub(mul(q.unit[p.normal[c]],2),1):float(c==2);
        result.material[10+c]=add(result.material[10+c],mul(normal,alpha));
        if(format.resolved)result.pbr[c]=add(result.pbr[c],mul(q.srgb[p.pbr[c]],alpha));
      }
    }
    if(format.physical||format.imported_overlay){
      // Preserve subnormal coefficients on devices that flush native f32.
      const auto w=paint_compute::bits(weight);
      const auto coverage=format.physical?p.emission.coverage:q.unit[p.coverage];
      emission[6]=sum(emission[6],product(paint_compute::bits(coverage),w));
      for(unsigned c=0;c<3;++c){
        const auto legacy=format.physical?p.emission.legacy[c]:mul(q.srgb[p.legacy[c]],coverage);
        emission[c]=sum(emission[c],product(paint_compute::bits(legacy),w));
        if(format.physical)
          emission[c+3]=sum(emission[c+3],product(paint_compute::bits(p.emission.absolute[c]),w));
      }
    }
  }
  const auto coverage=emission[6]<one32?emission[6]:one32;
  const auto limit=product(coverage,max32);
  result.emission.coverage=std::bit_cast<float>(coverage);
  for(unsigned c=0;c<3;++c){
    result.emission.legacy[c]=std::bit_cast<float>(emission[c]<coverage?emission[c]:coverage);
    // A rounded-to-zero coverage must be rejected by the field validator
    // if it can no longer represent the sampled nonzero absolute energy.
    result.emission.absolute[c]=std::bit_cast<float>(coverage&&emission[c+3]>limit?limit:emission[c+3]);
  }
  return result;
}
}

#undef DOME_RETOPO_BAKE_FN
