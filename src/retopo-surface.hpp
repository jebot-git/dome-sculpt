// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh-processing.hpp"
#include "retopo-surface-device.hpp"

namespace dome::detail {
// Worker-owned device workspace for unfiltered native Retopology projection.
// The immutable surface must outlive this workspace.
class RetopoSurface {
  const SurfaceQuery &source_;
  std::unique_ptr<retopo_surface::Query> device_;
public:
  explicit RetopoSurface(const SurfaceQuery &source):source_(source){}
  SurfaceHit closest(Vec3,const std::function<bool()> &cancelled={});
  void closest(std::span<const Vec3>,std::span<SurfaceHit>,const std::function<bool()> &cancelled={},
               uint32_t triangle_budget=2'000'000);
};
}
