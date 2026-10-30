// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/deformation.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/material-bindings.hpp"
#include "dome/color-preparation.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/polygon-scene.hpp"
#include <nlohmann/json.hpp>
namespace dome { struct PolygonAuthority; }
namespace dome::detail {
// Scene12 is the first format owning complete polygon source histories. Its
// bounded graph may retain all older frame/color/bind/source receipt features.
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
  if(const auto &preparation=binding->normal_preparation)
    result=std::max({result,15u,minimum_scene_version(preparation->original_deformation.get()),
      minimum_scene_version(preparation->original_appearance.get()),minimum_scene_version(preparation->original_material.get())});
  return result;
}
inline unsigned minimum_scene_version(const PolygonAuthority *p) {
  unsigned result=p?12u:0u;size_t depth=0;
  while(p&&p->origin){if(++depth>256)throw std::invalid_argument("Polygon feature graph exceeds its bounded depth");const auto &origin=*p->origin;
    result=std::max({result,minimum_scene_version(origin.deformation.get()),minimum_scene_version(origin.appearance.get()),minimum_scene_version(origin.material_bindings.get())});p=origin.polygon.get();
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
