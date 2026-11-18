// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-paint-access.hpp"
#include "dome/scene.hpp"
#include "dome/material-bindings.hpp"
#include "dome/texture-sets.hpp"

namespace dome::detail {
// Existing authored/source/prepared field resizing keeps its separate adapter.
// This narrow recovery route owns an ordinary, unpainted target with no image,
// normal, rig or original-asset authority to resample or discard.
inline bool unpainted_bake_resolution_target(const SculptObject &o)noexcept{
  if(!o.mesh_active||!o.painted||o.texture_sets||o.appearance||o.deformation||o.polygon||o.retopology||
      o.painted->in_stroke()||o.painted->has_paint_data()||o.painted->has_physical_emission()||o.painted->normal_projection()||o.painted->color_projection())return false;
  if(const auto &b=o.material_bindings){
    if(b->normal_preparation||b->normal_frames||b->canonical_attributes||b->canonical_uv||b->native_paint_sampling)return false;
    for(size_t role=0;role<b->roles.size();++role)if(b->roles[role].image||b->material.textures[role].index>=0)return false;
  }
  return true;
}
inline bool resize_unpainted_bake_target(Scene &scene,uint64_t object,uint32_t resolution,
    const TextureSetLimits &limits={}){
  limits.validate();auto stop=[&]{if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Target paint resolution change cancelled");};stop();
  const auto &before=scene.object(object);
  if(!unpainted_bake_resolution_target(before)||resolution<64||resolution>2048||(resolution&(resolution-1)))
    throw std::invalid_argument("Resize an unpainted Bake target; preserve authored or prepared fields through their owning resolution adapter");
  if(before.painted->resolution()==resolution)return false;
  auto resized=TextureSetPaintAccess::resize_unpainted(*before.painted,resolution,limits);
  auto next=scene.snapshot();next.object(object).painted=std::move(resized);
  next.validate();stop();scene=std::move(next);return true;
}
} // namespace dome::detail
