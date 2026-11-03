// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "key-mirror-correspondence.hpp"
#include <memory>
namespace dome::detail {
// Detached geometry only: no frame/material/paint authority or editable seal.
// Triangle indices always refer to the real immutable input mesh. An interior
// point is not a new raw source vertex identity.
struct MirrorSurfacePoint {
  uint32_t triangle = 0;
  std::array<double, 3> barycentric{};
};
struct MirrorRefinementCorner {
  Vec3 position;
  MirrorSurfacePoint current, reflected;
};
struct KeyMirrorRefinement {
  std::shared_ptr<const Mesh> source;
  // Three consecutive entries form one triangle. Deindexing deliberately keeps
  // distinct corner/role origins; a later owner must prove any actual merging.
  std::vector<MirrorRefinementCorner> corners;
  std::vector<uint32_t> opposite;
  double maximum_rest_error = 0;
  size_t work_units = 0;
};
KeyMirrorRefinement refine_reflected_key(std::shared_ptr<const Mesh>, float tolerance,
                                         const KeyMirrorLimits & = {});
} // namespace dome::detail
