// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "key-mirror-refinement.hpp"
namespace dome::detail {
// Detached samples only. These are raw interpolated columns, deliberately NOT
// a NormalFrames DTO or authority to alter a material, paint, mesh or Scene.
// Original source IDs remain in before; derived corners have barycentric
// provenance in the separately supplied refinement, never fabricated raw IDs.
struct RefinedRoleField {
  std::shared_ptr<const NormalFrames> before;
  std::vector<Vec3> normals;
  std::vector<std::array<float,4>> tangents;
  std::vector<NormalFrameTarget> targets;
  size_t working_bytes=0,work_units=0;
};
RefinedRoleField sample_refined_role_field(const KeyMirrorRefinement &,
    std::shared_ptr<const NormalFrames>,const KeyMirrorLimits & = {});
}
