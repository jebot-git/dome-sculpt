// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-paint-access.hpp"
#include "dome/scene.hpp"
#include "dome/material-bindings.hpp"
#include "dome/texture-sets.hpp"

namespace dome::detail {
// Raw source images and rig frames have independent immutable owners.
// Prepared fields need a separate grid adapter.
inline bool writable_bake_resolution_target(const SculptObject &o)noexcept{
  if(!o.mesh_active||!o.painted||o.texture_sets||o.polygon||o.retopology||
     !TextureSetPaintAccess::can_resize_raw(*o.painted))return false;
  const auto &b=o.material_bindings;
  return !b||(!b->normal_preparation&&!b->color_preparation&&!b->canonical_attributes&&
      !b->native_paint_sampling&&!b->canonical_paint_sampling&&!b->native_paint_frames&&
      !b->polygon_surface_transfer&&!b->sampling_reparameterized);
}
inline bool unpainted_bake_resolution_target(const SculptObject &o)noexcept{
  return writable_bake_resolution_target(o)&&TextureSetPaintAccess::can_resize_unpainted(*o.painted);
}
// At the conservative 20-work-per-cell charge, a 2048 grid alone exceeds
// the generic 64M limit. Smaller caller budgets and the 256 MiB cap still apply.
inline TextureSetLimits bake_target_resolution_limits(){
  TextureSetLimits limits;limits.maximum_work=128'000'000;return limits;
}
inline bool resize_unpainted_bake_target(Scene &scene,uint64_t object,uint32_t resolution,
    const TextureSetLimits &limits=bake_target_resolution_limits()){
  limits.validate();auto stop=[&]{if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Target paint resolution change cancelled");};stop();
  const auto &before=scene.object(object);
  if(!unpainted_bake_resolution_target(before)||resolution<64||resolution>2048||(resolution&(resolution-1)))
    throw std::invalid_argument("Target resizing requires an empty writable grid with no prepared field");
  if(before.painted->resolution()==resolution)return false;
  auto resized=TextureSetPaintAccess::resize_unpainted(*before.painted,resolution,limits);
  auto next=scene.snapshot();next.object(object).painted=std::move(resized);
  next.validate();stop();scene=std::move(next);return true;
}
} // namespace dome::detail
