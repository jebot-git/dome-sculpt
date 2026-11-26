// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh-processing.hpp"

namespace dome::detail {
inline constexpr uint64_t bake_ray_limit = 1'000'000'000;

struct BakeRayQuery {
  // An exact hit takes precedence. A missing ray may touch an edge within the
  // float staging error, provided the edge lies within the same depth segment.
  static std::optional<SurfaceHit> cast(const SurfaceQuery &,Vec3 origin,Vec3 direction,
      float maximum,double rounding_radius,const SurfaceQueryOptions &);
};
} // namespace dome::detail
