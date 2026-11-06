// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh-processing.hpp"
#include "mtoon.hpp"
#include <optional>
#include <span>
namespace dome {
namespace detail {struct SculptSurfaceChunk; class SculptSceneChunks;}
struct CreatorAppearance;
struct SurfaceRange;
struct SourceNativeMaterial;
// Shared by GPU binding and worker-built picking; no metadata parsing on input.
std::optional<MToonMaterial> retained_mtoon(const CreatorAppearance *);
int mtoon_coordinate(const MToonTexture &);
SourceNativeMaterial native_source(const CreatorAppearance &);
float mtoon_factor(float typed, float original, float current);
struct PaintCornerCoordinates { std::span<const std::array<float,2>> values; };
struct PaintVertexCoordinates { std::span<const std::array<float,2>> values; };
// Distinguishes an actual packed triangle from a chunk-local triangle. A
// caller selecting an object must use object, never manufacture a global
// packed offset for a separate immutable geometry owner.
struct MaterialSurfaceHit {
  SurfaceHit surface;
  uint64_t object=0;
  std::shared_ptr<const detail::SculptSurfaceChunk> chunk;
  bool packed() const {return !chunk;}
};
class MaterialSurfaceQuery {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  MaterialSurfaceQuery(const Mesh &, std::span<const SurfaceRange>,
                       const std::function<bool()> &cancelled = {});
  // Explicit renderer stream. Empty suppresses raw tint for prepared surfaces;
  // the original three-argument constructor keeps legacy appearance fallback.
  MaterialSurfaceQuery(const Mesh &, std::span<const SurfaceRange>,
                       const std::function<bool()> &cancelled,
                       std::span<const std::array<float,4>> packed_colors);
  // Explicit coordinate alignment: logical input triangles or packed vertices.
  MaterialSurfaceQuery(const Mesh &,std::span<const SurfaceRange>,const std::function<bool()> &,
                       std::span<const std::array<float,4>>,PaintCornerCoordinates);
  MaterialSurfaceQuery(const Mesh &,std::span<const SurfaceRange>,const std::function<bool()> &,
                       std::span<const std::array<float,4>>,PaintVertexCoordinates);
  // A real mixed immutable scene, retaining chunk-local namespaces. This does
  // not flatten generated geometry into the packed paint/source namespace.
  MaterialSurfaceQuery(const Mesh &,std::span<const SurfaceRange>,const std::function<bool()> &,
      std::span<const std::array<float,4>>,PaintVertexCoordinates,
      std::shared_ptr<const detail::SculptSceneChunks>);
  ~MaterialSurfaceQuery();
  MaterialSurfaceQuery(const MaterialSurfaceQuery &) = delete;
  MaterialSurfaceQuery &operator=(const MaterialSurfaceQuery &) = delete;
  SurfaceHit closest(Vec3) const;
  SurfaceHit closest(Vec3,uint64_t only_object) const;
  std::optional<SurfaceHit> nearby(Vec3, float maximum, double seconds=0) const;
  // The closest visible face may be an unselected occluder. Base-level source
  // samplers, alpha/cutoff/cull, authored UV1 and MToon animation are retained.
  std::optional<SurfaceHit> raycast(Vec3 origin, Vec3 direction,
                                    float maximum = 100, double seconds = 0,uint64_t only_object = 0) const;
  // Excluded objects are a captured visibility/edit policy, never an alpha override.
  std::optional<SurfaceHit> raycast_excluding(Vec3 origin, Vec3 direction, float maximum,
                                    double seconds, std::span<const uint64_t> excluded) const;
  MaterialSurfaceHit closest_target(Vec3,uint64_t only_object=0) const;
  std::optional<MaterialSurfaceHit> nearby_target(Vec3,float maximum,double seconds=0) const;
  std::optional<MaterialSurfaceHit> raycast_target(Vec3,Vec3,float maximum=100,
                                                double seconds=0,uint64_t only_object=0) const;
  std::optional<MaterialSurfaceHit> raycast_excluding_target(Vec3,Vec3,float maximum,
                                                double seconds,std::span<const uint64_t>) const;
};
} // namespace dome
