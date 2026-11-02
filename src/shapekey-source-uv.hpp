// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/material-bindings.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/source-maps.hpp"
namespace dome::detail {
inline constexpr const char *source_uv_key_notice =
 "Prepare an editable texture atlas before changing geometry keys with original UV2+ normals";
// Geometric reauthoring currently derives its change of frame from UV0.
// Existing attribute copies/blends need no geometric frame, and original maps
// retained only in before-state capsules are not current sampling authority.
inline bool source_uv_geometry_keys_require_atlas(const SculptObject &object) noexcept {
 if(!object.painted)return false;
 const auto &binding=object.material_bindings;
 // An authenticated prepared route reauthors its own declared target basis;
 // raw maps in that recipe's before-state are not the current normal sampler.
 if(binding&&binding->normal_preparation&&object.painted->normal_projection()&&
    object.painted->normal_projection()==binding->normal_preparation->projection)return false;
 if(const auto &maps=object.painted->source_maps();maps&&maps->normal.image&&maps->normal.texcoord>=2)return true;
 if(binding&&binding->has_mtoon){
  const auto role=size_t(MToonSlot::Normal);const auto &slot=binding->material.textures[role];
  const auto coordinate=slot.transform_texcoord>=0?uint32_t(slot.transform_texcoord):slot.texcoord;
  if(slot.index>=0&&binding->roles[role].image&&(coordinate>=2||binding->roles[role].texcoord>=2))return true;
 }
 return false;
}
}
