// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh-processing.hpp"
#include "retopo-surface-device.hpp"

namespace dome::detail {
struct RetopoMatch {
  uint32_t triangle=UINT32_MAX;
  std::array<float,3> barycentric{};
  explicit operator bool()const noexcept{return triangle!=UINT32_MAX;}
};
struct RetopoMatchPoint {
  Vec3 position,normal;
  bool restrict_material=false;
};
// Worker-owned device workspace for native Retopology projection and transfer.
// The immutable surface must outlive this workspace.
class RetopoSurface {
  const SurfaceQuery &source_;
  std::unique_ptr<retopo_surface::Query> device_;
  struct MaterialState;
  std::unique_ptr<MaterialState> material_;
public:
  explicit RetopoSurface(const SurfaceQuery &);
  ~RetopoSurface();
  SurfaceHit closest(Vec3,const std::function<bool()> &cancelled={});
  void closest(std::span<const Vec3>,std::span<SurfaceHit>,const std::function<bool()> &cancelled={},
               uint32_t triangle_budget=2'000'000);
  uint32_t match(std::span<const RetopoMatchPoint>,std::span<RetopoMatch>,const AffineTransform &,
                 std::span<const Vec3> normals,std::optional<std::span<const uint32_t>> members,
                 uint32_t work,const std::function<bool()> &cancelled);
};
}
