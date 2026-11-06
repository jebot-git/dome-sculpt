// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "canonical-paint-sampling.hpp"
#include "polygon-surface-owner.hpp"
#include "polygon-scene-archive.hpp"
namespace dome::detail {
// Close the real material/origin DAG before counting or encoding either pool.
// Both archives retain their own typed identity/count/depth checks; no values
// are merged and no before origin is replaced by the current object.
inline void collect_canonical_paint_controls(SourceMaterialArchive &materials,
    PolygonSceneArchive &origins,bool deep,const std::function<bool()> &cancelled={}){
 size_t material=0,origin=0;
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Canonical paint history collection cancelled");};
 while(material<materials.bindings().size()||origin<origins.origins().size()){
  while(material<materials.bindings().size()){
   stop();const auto binding=materials.bindings()[material++];
   if(binding->canonical_paint_sampling)origins.add_origin(binding->canonical_paint_sampling->before(),deep);
   if(binding->polygon_surface_transfer)origins.add_origin(binding->polygon_surface_transfer->before(),deep);
  }
  while(origin<origins.origins().size()){
   stop();const auto before=origins.origins()[origin++];if(before->material_bindings)materials.add(before->material_bindings,deep);
  }
 }
 stop();
}
}
