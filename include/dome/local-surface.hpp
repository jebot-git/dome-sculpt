// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <map>
#include <memory>
#include <span>
namespace dome {
// Inclusive lattice SAMPLE bounds, before the mesher's support halo. Producers
// must include every changed value (including erased values), not just
// negatives.
using SurfaceSampleRegion = Volume::GeometryRegion;
struct LocalSurfaceLimits {
  size_t samples = 4'000'000, tiles = 65'536, dirty_tiles = 16'384;
  size_t evaluated_cells = 8'000'000, bytes = 512 * 1024 * 1024;
  size_t vertices = 8'000'000, indices = 48'000'000, regions = 4096;
};
struct LocalSurfaceStats {
  size_t samples_scanned = 0, cells_evaluated = 0, tiles_built = 0;
  size_t tiles_reused = 0, bytes = 0, source_bytes = 0;
};
// Minimal immutable sampler. It retains no displacement layers, masks,
// authoring history or controls. Capturing a strong sample owner makes future
// COW writes detach, while the context records the meaning of absent sample
// values.
class SurfaceGeometry {
public:
  explicit SurfaceGeometry(const Volume &);
  const Volume::GeometryOwner &geometry_owner() const { return samples_; }
  const Volume::Voxels &voxels() const { return *samples_; }
  float voxel_size() const { return voxel_; }
  float band() const { return band_; }
  float at(Int3) const;
  float sample(Vec3) const;
  Vec3 gradient(Vec3) const;

private:
  Volume::GeometryOwner samples_;
  float voxel_, band_;
};
// Immutable CPU extraction packets. An edge key is global lattice provenance,
// not an approximate position weld; exact-zero intersections use {p,p}.
struct SurfaceEdge {
  Int3 a, b;
  auto operator<=>(const SurfaceEdge &) const = default;
};
struct SurfaceEdgeVertex {
  SurfaceEdge edge;
  Vec3 position, normal;
};
struct SurfaceCell {
  Int3 coordinate;
  std::vector<SurfaceEdgeVertex> vertices;
  // Ordered triangle requests; global edge resolution precedes degeneracy and
  // winding.
  std::vector<uint32_t> indices;
  std::vector<Vec3> outward;
};
struct SurfaceTile {
  Int3 coordinate;
  std::vector<SurfaceCell> cells;
  size_t bytes = 0;
  int support_halo = 1;
};
struct LocalSurfaceUpdateReport {
  enum class Kind { Reused, Local, Rebuild };
  Kind kind = Kind::Rebuild;
  size_t ancestry_events = 0;
  bool capacity_fallback = false;
};
class LocalSurfaceCache
    : public std::enable_shared_from_this<LocalSurfaceCache> {
public:
  static constexpr int tile_width = 8;
  using Ptr = std::shared_ptr<const LocalSurfaceCache>;
  using Owner = std::shared_ptr<const SurfaceGeometry>;
  using Cancel = std::function<bool()>;
  using Tiles = std::map<Int3, std::shared_ptr<const SurfaceTile>>;
  static Ptr build(const Volume &, LocalSurfaceLimits = {}, Cancel = {});
  // Authenticated Volume ancestry is queried internally. Unknown/expired or
  // replaced sources rebuild; callers cannot declare incomplete dirty boxes.
  Ptr update(const Volume &, Cancel = {},
             LocalSurfaceUpdateReport * = nullptr) const;
  const Owner &source_owner() const { return source_; }
  const Tiles &tiles() const { return tiles_; }
  const LocalSurfaceStats &stats() const { return stats_; }
  // Explicit export/legacy adapter. Updates and no-op view/mask changes never
  // invoke this O(surface) operation. Output ordering matches surface_mesh().
  Mesh materialize(Cancel = {}) const;
  static Int3 tile_coordinate(Int3 cell);

private:
  LocalSurfaceCache() = default;
  Ptr update_regions(const Volume &, std::span<const SurfaceSampleRegion>,
                     Cancel) const;
  Owner source_;
  LocalSurfaceLimits limits_;
  Tiles tiles_;
  LocalSurfaceStats stats_;
  int support_halo_ = 1;
};
} // namespace dome
