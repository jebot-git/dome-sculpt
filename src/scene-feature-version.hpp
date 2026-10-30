// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/deformation.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/material-bindings.hpp"
#include <nlohmann/json.hpp>
namespace dome::detail {
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
  // ColorPreparation is merged by its owning actor: include its retained
  // original_deformation and original_material even after active rig changes.
  return binding->uv_origin
             ? std::max(minimum_scene_version(binding->uv_origin->deformation.get()),minimum_scene_version(binding->uv_origin->appearance.get()))
             : 0u;
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
