// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "key-mirror-correspondence.hpp"
namespace dome::detail {
// Converted, base-relative CHANNEL interpretation, not the exact absolute
// reflection in record3. Nothing here authenticates an editable material owner.
struct MirrorRelativeReport {
  double maximum_normal_base_difference = 0, maximum_tangent_base_difference = 0;
  double maximum_chart_cross_ratio = 0, maximum_basis_scale_difference = 0;
};
struct MirrorRelativeAttributes {
  NormalFrameTarget target;
  MirrorRelativeReport report;
};
MirrorRelativeAttributes relative_mirror_attributes(
    const Mesh &basis, const NormalFrames &before, const NormalFrameTarget &source, uint64_t id,
    const KeyMirrorCorrespondence &, std::span<const std::array<float, 2>> role_corner_coordinates,
    const KeyMirrorLimits & = {});
} // namespace dome::detail
