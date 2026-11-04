// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/humanoid-creator.hpp"
#include "dome/scene.hpp"
namespace dome::detail {
// Bounded cheap UI eligibility, not a geometry/field certificate. The worker
// validates exact current rest/placement/owners and source correspondence.
inline bool native_weight_reference(const SculptObject &o) {
  if (!o.mesh_active || !o.painted || !o.deformation ||
      !o.deformation->skeleton || o.deformation->skeleton->bones.empty() ||
      o.deformation->skeleton->bones.size() > 256 ||
      o.painted->mesh().indices.empty())
    return false;
  if (o.appearance) {
    const auto &a = *o.appearance;
    if (!a.source_joint_nodes.empty() || !a.asset || a.source_node < 0 ||
        size_t(a.source_node) >= a.asset->nodes.size() ||
        a.asset->nodes[size_t(a.source_node)].skin >= 0)
      return false;
  }
  return std::any_of(o.deformation->skeleton->bones.begin(),
                     o.deformation->skeleton->bones.end(),
                     [](const auto &b) { return b.deform; });
}
} // namespace dome::detail
