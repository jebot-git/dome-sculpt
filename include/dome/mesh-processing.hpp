// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <memory>
namespace dome {
struct TopologyReport {
  size_t vertices = 0, triangles = 0, geometric_vertices = 0,
         unused_vertices = 0;
  size_t degenerate_triangles = 0, duplicate_triangles = 0, boundary_edges = 0;
  size_t nonmanifold_edges = 0, inconsistent_edges = 0, components = 0;
  bool closed_manifold() const {
    return triangles && !degenerate_triangles && !duplicate_triangles &&
           !boundary_edges && !nonmanifold_edges && !inconsistent_edges;
  }
};
// Topology joins exact geometric positions across attribute seams; it never
// welds nearby vertices merely because they happen to share a spatial bucket.
TopologyReport inspect_mesh(const Mesh &,
                            const std::function<bool()> &cancelled = {});
struct CleanupOptions {
  bool remove_degenerate = true, remove_duplicates = true,
       merge_identical_vertices = true;
  std::function<bool()> cancelled;
};
struct CleanupResult {
  Mesh mesh;
  std::vector<uint32_t> source_vertices, source_triangles;
  size_t removed_degenerate = 0, removed_duplicates = 0, removed_vertices = 0;
};
// Full attributes and explicit UV seam/pin flags are preserved. Applying this
// to an already-painted asset requires a separate explicit paint-transfer job.
CleanupResult cleanup_mesh(const Mesh &, const CleanupOptions & = {});
struct SurfaceHit {
  uint32_t triangle = 0;
  Vec3 position{}, normal{};
  std::array<float, 3> barycentric{};
  float distance = 0;
};
// Owns an immutable mesh/BVH copy; concurrent read-only queries are safe.
// Distances are metres and ray direction is normalized internally.
class SurfaceQuery {
  struct Impl;
  std::unique_ptr<Impl> impl_;

public:
  explicit SurfaceQuery(const Mesh &,
                        const std::function<bool()> &cancelled = {});
  ~SurfaceQuery();
  SurfaceQuery(SurfaceQuery &&) noexcept;
  SurfaceQuery &operator=(SurfaceQuery &&) noexcept;
  SurfaceQuery(const SurfaceQuery &) = delete;
  SurfaceQuery &operator=(const SurfaceQuery &) = delete;
  SurfaceHit closest(Vec3) const;
  std::optional<SurfaceHit> raycast(Vec3 origin, Vec3 direction,
                                    float maximum = 100) const;
};
struct VoxelizeOptions {
  float voxel_size = .03f;
  size_t max_grid_points = 4'000'000;
  std::function<bool()> cancelled;
};
// BVH accelerated closest-distance/odd-even signed distance for closed,
// consistently oriented non-self-intersecting triangle shells. Nested shells
// use odd-even fill (inner cavities); arbitrary self-intersections are not
// fixed. Fails before publishing if the dense interior plus narrow band exceeds
// limits.
Volume mesh_to_volume(const Mesh &, const VoxelizeOptions & = {});
} // namespace dome
