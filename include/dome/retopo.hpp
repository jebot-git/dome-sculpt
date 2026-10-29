// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh-processing.hpp"
#include <map>
#include <span>
namespace dome {
using TopologyID = uint64_t;
struct RetopoVertex {
  Vec3 position{};
  std::array<float, 2> uv{};
  bool pinned = false;
};
struct RetopoEdge {
  TopologyID a = 0, b = 0;
  bool seam = false;
};
struct RetopoFace {
  std::vector<TopologyID> vertices;
  uint32_t material = 0;
};
struct RetopoSurface {
  Mesh mesh;
  std::vector<TopologyID> source_vertices, source_faces;
  std::vector<uint32_t> materials;
};
struct RetopoRelaxOptions {
  unsigned iterations = 1;
  float strength = .5f, maximum_snap_distance = .1f;
  std::function<bool()> cancelled;
};
// Value snapshots preserve all stable IDs, attributes and the allocation
// cursor. Mutations either complete or leave the original value untouched. IDs
// are never recycled inside a history branch. Wire edges and isolated vertices
// are allowed.
class RetopoMesh {
  TopologyID next_ = 1;
  std::map<TopologyID, RetopoVertex> vertices_;
  std::map<TopologyID, RetopoEdge> edges_;
  std::map<TopologyID, RetopoFace> faces_;
  TopologyID edge_id(TopologyID, TopologyID) const;

public:
  const auto &vertices() const { return vertices_; }
  const auto &edges() const { return edges_; }
  const auto &faces() const { return faces_; }
  TopologyID next_id() const { return next_; }
  TopologyID add_vertex(RetopoVertex);
  TopologyID add_edge(TopologyID a, TopologyID b, bool seam = false);
  TopologyID add_face(std::span<const TopologyID>, uint32_t material = 0);
  void remove_face(TopologyID);
  void remove_edge(TopologyID);   // Must have no incident face.
  void remove_vertex(TopologyID); // Must have no incident edge/face.
  void move_vertex(TopologyID, Vec3);
  void set_pinned(TopologyID, bool);
  void set_seam(TopologyID, bool);
  void project(std::span<const TopologyID>, const SurfaceQuery &,
               float maximum_distance,
               const std::function<bool()> &cancelled = {});
  void relax(const SurfaceQuery &, const RetopoRelaxOptions & = {});
  void validate(const std::function<bool()> &cancelled = {}) const;
  RetopoSurface triangulate(const std::function<bool()> &cancelled = {}) const;
  std::string encode() const;
  static RetopoMesh decode(std::string_view);
};
struct PlaneCutOptions {
  Vec3 normal{0, 1, 0};
  float offset = 0; // keep dot(normalized(normal),position) >= offset
  bool cap = true;
  uint32_t cap_material = 0;
  float epsilon = 1e-6f; // Metres; ambiguous vertices on the plane reject.
  size_t max_triangles = 2'000'000, max_contour_vertices = 4096;
  std::function<bool()> cancelled;
};
struct PlaneCutResult {
  Mesh mesh;
  // One entry per result triangle. UINT32_MAX denotes newly generated cap.
  std::vector<uint32_t> source_triangles, materials;
  // Per output index, relative to that triangle's source. Zero for cap corners.
  std::vector<std::array<float, 3>> source_corner_barycentrics;
  size_t cap_triangles = 0;
};
// Keeps the positive half-space. Input face materials are optional, one per
// triangle. Capping requires closed consistently oriented nonintersecting
// shells. Existing UVs/pins/seams interpolate; new cut/cap boundaries become
// seams. Cap contours with holes, plane-through-vertex cases or ambiguous
// branches reject explicitly; painted assets require separate
// transfer/rebaking.
PlaneCutResult plane_cut(const Mesh &, const PlaneCutOptions & = {},
                         std::span<const uint32_t> materials = {});
} // namespace dome
