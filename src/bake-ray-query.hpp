// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh-processing.hpp"
#include <span>

namespace dome::detail {
inline constexpr uint64_t bake_ray_limit = 1'000'000'000;
namespace bake_ambient { class Query; }

struct BakeRayQuery {
  // An exact hit takes precedence. A missing ray may touch an edge within the
  // float staging error, provided the edge lies within the same depth segment.
  static std::optional<SurfaceHit> cast(const SurfaceQuery &,Vec3 origin,Vec3 direction,
      float maximum,double rounding_radius,const SurfaceQueryOptions &);
  static std::unique_ptr<bake_ambient::Query> upload_ambient(const SurfaceQuery &,
      std::span<const Vec3> pattern,float maximum,const std::function<bool()> &cancelled);
};
} // namespace dome::detail
