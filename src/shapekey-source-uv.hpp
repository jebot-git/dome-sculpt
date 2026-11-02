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
 if(!object.painted||!object.painted->source_maps())return false;
 const auto &normal=object.painted->source_maps()->normal;
 if(!normal.image||normal.texcoord<2)return false;
 const auto &binding=object.material_bindings;
 return !(binding&&binding->normal_preparation&&object.painted->normal_projection()&&
   object.painted->normal_projection()==binding->normal_preparation->projection);
}
}
