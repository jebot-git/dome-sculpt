// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/deformation.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/material-bindings.hpp"
#include "dome/color-preparation.hpp"
#include "dome/polygon-scene.hpp"
#include <set>
#include <nlohmann/json.hpp>
namespace dome { struct PolygonAuthority; }
namespace dome::detail {
// Scene12 is the first format owning complete polygon source histories. Its
// bounded graph may retain all older frame/color/bind/source receipt features.
inline unsigned minimum_scene_version(const PolygonAuthority *p);
inline unsigned minimum_scene_version(const DeformationState *d) {
  if (!d || !d->skeleton)
    return 0;
  const auto &s = *d->skeleton;
  return s.inverse_bind_override || s.space_matrix ||
                 std::any_of(s.bones.begin(), s.bones.end(),
                             [](const auto &b) { return bool(b.rest_matrix); })
             ? 11u
             : 0u;
}
inline unsigned minimum_scene_version(const CreatorAppearance *a) {return a && a->source_receipt ? 11u : 0u;}
inline unsigned minimum_scene_version(const SourceMaterialBindings *binding) {
  if (!binding)
    return 0;
  unsigned result=binding->uv_origin
             ? std::max(minimum_scene_version(binding->uv_origin->deformation.get()),minimum_scene_version(binding->uv_origin->appearance.get()))
             : 0u;
  if(const auto &preparation=binding->color_preparation)
    result=std::max({result,10u,minimum_scene_version(preparation->original_deformation.get()),
      minimum_scene_version(preparation->original_appearance.get()),minimum_scene_version(preparation->original_material.get())});
  return result;
}
inline unsigned minimum_scene_version(const PolygonAuthority *p) {
  unsigned result=0;std::set<const PolygonAuthority*> seen;
  while(p) {
    if(seen.size()>=512 || !seen.insert(p).second)throw std::invalid_argument("Invalid retained polygon feature-version graph");
    result=std::max(result,p->colors?14u:12u);const auto&o=p->origin;if(!o)break;
    result=std::max({result,o->placement?13u:0u,minimum_scene_version(o->deformation.get()),
      minimum_scene_version(o->appearance.get()),minimum_scene_version(o->material_bindings.get())});
    p=o->polygon.get();
  }
  return result;
}
inline unsigned minimum_deformation_scene_version(const nlohmann::json &d) {
  if (d.is_null() || d.at("skeleton").is_null())
    return 0;
  const auto &s = d.at("skeleton");
  return s.contains("inverse_bind_override") ||
                 (s.contains("version") && s.at("version") == 2)
             ? 11u
             : 0u;
}
} // namespace dome::detail
