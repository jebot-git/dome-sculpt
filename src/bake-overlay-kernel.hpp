// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-material-kernel.hpp"
#include "source-material-raster-kernel.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_BAKE_OVERLAY_FN __host__ __device__ inline
#else
#define DOME_BAKE_OVERLAY_FN inline
#endif

namespace dome::detail::bake_material {
struct OverlayView {
  emission_raster::DeviceInput input;
  emission_raster::MaterialView material;
  CoveredNormalMap normal;
  bool separate_pbr=false,has_emission=false;
};

DOME_BAKE_OVERLAY_FN void apply_overlay(const OverlayView &view,const Coordinates &coordinates,Values &value){
  using namespace paint_compute;
  using namespace emission_raster;
  if(value.invalid)return;
  if(!finite_uv(coordinates.paint_uv)||(view.normal.rgba&&!finite_uv(coordinates.high_uv))){
    value.invalid=1;
    return;
  }
  const auto column=clamped_axis(coordinates.paint_uv[0],view.input.resolution);
  const auto row=clamped_axis(coordinates.paint_uv[1],view.input.resolution);
  const auto composite=[](float base,float coverage,float premultiplied){
    return add(mul(base,sub(1,coverage)),premultiplied);
  };
  const auto common=filter_material(view.input,view.material,column,row,false);
  if(view.separate_pbr){
    const auto pbr=view.material.pbr||view.material.bake_pbr?filter_material(view.input,view.material,column,row,true):common;
    for(unsigned c=0;c<4;++c)value.pbr[c]=composite(value.color[c],pbr.coverage[0],pbr.color[c]);
  }
  for(unsigned c=0;c<4;++c)value.color[c]=composite(value.color[c],common.coverage[0],common.color[c]);
  value.occlusion=composite(value.occlusion,common.ao_coverage,common.orm[0]);
  value.roughness=composite(value.roughness,common.coverage[2],common.orm[1]);
  value.metallic=composite(value.metallic,common.coverage[1],common.orm[2]);
  if(view.has_emission){
    uint32_t glow[7]{};
    filter_finite(view.input,column,row,glow);
    for(unsigned c=0;c<3;++c)
      value.emission[c]=composite(value.emission[c],material_float(glow[6]),material_float(glow[c]));
  }
  if(view.normal.rgba)sample_covered_normal(view.normal,coordinates.high_uv,*view.input.quantization,value.baked_normal);
}
} // namespace dome::detail::bake_material
#undef DOME_BAKE_OVERLAY_FN
