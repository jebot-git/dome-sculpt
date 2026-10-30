// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh-processing.hpp"
#include "mtoon.hpp"
#include <optional>
#include <span>
namespace dome {
struct CreatorAppearance;
struct SurfaceRange;
struct SourceNativeMaterial;
// Shared by GPU binding and worker-built picking; no metadata parsing on input.
std::optional<MToonMaterial> retained_mtoon(const CreatorAppearance *);
int mtoon_coordinate(const MToonTexture &);
SourceNativeMaterial native_source(const CreatorAppearance &);
float mtoon_factor(float typed, float original, float current);
class MaterialSurfaceQuery {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  MaterialSurfaceQuery(const Mesh &, std::span<const SurfaceRange>,
                       const std::function<bool()> &cancelled = {});
  ~MaterialSurfaceQuery();
  MaterialSurfaceQuery(const MaterialSurfaceQuery &) = delete;
  MaterialSurfaceQuery &operator=(const MaterialSurfaceQuery &) = delete;
  SurfaceHit closest(Vec3) const;
  // The closest visible face may be an unselected occluder. Base-level source
  // samplers, alpha/cutoff/cull, authored UV1 and MToon animation are retained.
  std::optional<SurfaceHit> raycast(Vec3 origin, Vec3 direction,
                                    float maximum = 100, double seconds = 0,uint64_t only_object = 0) const;
};
} // namespace dome
