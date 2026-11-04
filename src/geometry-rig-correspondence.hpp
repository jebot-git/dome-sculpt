// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "skin-affine.hpp"
#include "weight-surface-system.hpp"
namespace dome::detail {
namespace geometry_rig_correspondence {
inline bool same_float(float a, float b) {
  return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b);
}
inline bool same_matrix(const RigMatrix &a, const RigMatrix &b) {
  for (size_t k = 0; k < 16; ++k)
    if (!same_float(a[k], b[k]))
      return false;
  return true;
}
inline bool same_optional_matrix(const std::optional<RigMatrix> &a,
                                 const std::optional<RigMatrix> &b) {
  return bool(a) == bool(b) && (!a || same_matrix(*a, *b));
}
inline bool same_joint(const JointTransform &a, const JointTransform &b) {
  const std::array x{a.translation.x, a.translation.y, a.translation.z,
                     a.rotation.w,    a.rotation.x,    a.rotation.y,
                     a.rotation.z,    a.scale.x,       a.scale.y,
                     a.scale.z};
  const std::array y{b.translation.x, b.translation.y, b.translation.z,
                     b.rotation.w,    b.rotation.x,    b.rotation.y,
                     b.rotation.z,    b.scale.x,       b.scale.y,
                     b.scale.z};
  for (size_t k = 0; k < x.size(); ++k)
    if (!same_float(x[k], y[k]))
      return false;
  return same_optional_matrix(a.full_local_matrix, b.full_local_matrix);
}
inline bool same_rig(const Skeleton &a, const Skeleton &b) {
  if (a.bones.size() != b.bones.size() ||
      !same_optional_matrix(a.space_matrix, b.space_matrix) ||
      bool(a.inverse_bind_override) != bool(b.inverse_bind_override))
    return false;
  if (a.inverse_bind_override) {
    if (a.inverse_bind_override->size() != b.inverse_bind_override->size())
      return false;
    for (size_t i = 0; i < a.inverse_bind_override->size(); ++i)
      if (!same_matrix(a.inverse_bind_override->at(i),
                       b.inverse_bind_override->at(i)))
        return false;
  }
  for (size_t i = 0; i < a.bones.size(); ++i) {
    const auto &x = a.bones[i], &y = b.bones[i];
    if (x.name != y.name || x.parent != y.parent || x.deform != y.deform ||
        !same_float(x.length, y.length) || !same_joint(x.rest, y.rest) ||
        !same_optional_matrix(x.rest_matrix, y.rest_matrix))
      return false;
  }
  return true;
}
} // namespace geometry_rig_correspondence
// Worker-only exact canonical replay, not name-only/nearest-joint mapping.
// Complete rest/deform/bind data uses exact float bits and optional presence.
// This grants no reference identity from artist weight values or count
// equality.
inline bool geometry_reference_rig_matches(const Skeleton &source,
                                           const Skeleton &target,
                                           const AffineTransform &change,
                                           bool same_frame,
                                           weight_surface::Budget &work,
                                           size_t maximum_workspace,
                                           size_t *workspace_peak = nullptr) {
  if (source.bones.size() > 256 || target.bones.size() > 256 ||
      source.bones.size() != target.bones.size())
    return false;
  size_t names = 0;
  for (const auto &bone : source.bones) {
    if (bone.name.size() > maximum_workspace - names)
      throw std::length_error("Reference rig replay name bound");
    names += bone.name.size();
  }
  const size_t metadata =
      source.bones.size() * (sizeof(Bone) + sizeof(RigMatrix) * 2) + 8192;
  if (metadata > maximum_workspace || names > maximum_workspace - metadata)
    throw std::length_error(
        "Canonical reference rig replay exceeds workspace before copy");
  if (workspace_peak)
    *workspace_peak = std::max(*workspace_peak, metadata + names);
  work.step_items(names + 1, 2);
  work.step_items(source.bones.size() + target.bones.size() + 1, 8192);
  if (same_frame && geometry_rig_correspondence::same_rig(source, target))
    return true;
  SkinGeometryWork geometry(work.maximum - work.used, work.cancel);
  const auto before = geometry.remaining();
  const auto expected = skin_reframe(source, change, geometry);
  work.step(before - geometry.remaining());
  return geometry_rig_correspondence::same_rig(expected, target);
}
} // namespace dome::detail
