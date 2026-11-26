// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-paint-access.hpp"
#include "dome/scene.hpp"
#include "dome/material-bindings.hpp"
#include "dome/texture-sets.hpp"

namespace dome::detail {
// Raw source images and rig frames have their own immutable owners. Only the
// empty writable grid changes here. Prepared fields need their own adapter.
inline bool unpainted_bake_resolution_target(const SculptObject &o)noexcept{
  if(!o.mesh_active||!o.painted||o.texture_sets||o.polygon||o.retopology||
     !TextureSetPaintAccess::can_resize_unpainted(*o.painted))return false;
  const auto &b=o.material_bindings;
  return !b||(!b->normal_preparation&&!b->color_preparation&&!b->canonical_attributes&&
      !b->native_paint_sampling&&!b->canonical_paint_sampling&&!b->native_paint_frames&&
      !b->polygon_surface_transfer&&!b->sampling_reparameterized);
}
// 2048² cells at the unchanged conservative20-work/cell charge need more
// than the generic64M limit. Only this ordinary empty-target workflow opts in;
// caller-supplied smaller budgets remain binding and byte limits stay256MiB.
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
