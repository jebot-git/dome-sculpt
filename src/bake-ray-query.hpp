// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh-processing.hpp"
#include <span>

namespace dome::detail {
inline constexpr uint64_t bake_ray_limit = 1'000'000'000;
namespace bake_rays { class Query; struct ProjectionRay; struct ProjectionHit; }

struct BakeRayQuery {
  // An exact hit takes precedence. A missing ray may touch an edge within the
  // float staging error, provided the edge lies within the same depth segment.
  static std::optional<SurfaceHit> cast(const SurfaceQuery &,Vec3 origin,Vec3 direction,
      float maximum,double rounding_radius,const SurfaceQueryOptions &);
  static bake_rays::ProjectionRay projection_ray(Vec3 origin,Vec3 direction,float maximum,double radius);
  static std::optional<SurfaceHit> resolve(const SurfaceQuery &,const bake_rays::ProjectionHit &);
  static std::unique_ptr<bake_rays::Query> upload(const SurfaceQuery &,
      std::span<const Vec3> pattern,float ao_maximum,bool projection,const std::function<bool()> &cancelled);
};
} // namespace dome::detail
