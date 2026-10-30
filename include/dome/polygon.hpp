// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <optional>
#include <span>
namespace dome {
using PolygonId = uint64_t;
struct PolygonLimits {
  size_t maximum_vertices = 500'000, maximum_faces = 500'000;
  size_t maximum_corners = 2'000'000, maximum_face_corners = 256;
  size_t maximum_work = 64'000'000, maximum_provenance = 128;
  std::function<bool()> cancelled;
};
struct PolygonVertex {
  PolygonId id = 0;
  Vec3 position;
  // Convex interpolation of initial vertex identities, for later attribute
  // transfer. Moving/extruding a vertex does not change its source identity.
  std::vector<std::pair<PolygonId, double>> provenance;
};
struct PolygonCorner {
  PolygonId id = 0, vertex = 0;
  std::array<float, 2> uv{};
  bool generated_uv = false;
};
struct PolygonFace {
  PolygonId id = 0;
  std::vector<PolygonCorner> corners;
  uint32_t material = 0;
};
struct PolygonEdge {
  PolygonId id = 0, a = 0, b = 0;
  bool hard = false;
};
enum class PolygonSelectionMode { Vertex, Edge, Face };
struct PolygonSelection {
  PolygonSelectionMode mode = PolygonSelectionMode::Face;
  std::vector<PolygonId> ids;
};
enum class PolygonOperation {
  Move,
  Extrude,
  Inset,
  Bridge,
  Dissolve,
  Weld,
  CutEdges,
  ConnectVertices,
  DeleteFaces,
  FlipFaces
};
struct PolygonEdit {
  PolygonOperation operation = PolygonOperation::Move;
  PolygonSelection selection;
  Vec3 delta{};
  float amount = 0; // Extrude/inset in object-local metres.
  unsigned cuts = 1;
  // Workspace high-water ticket: retain this across local Undo/branching so
  // a stale component selection can never acquire a discarded draft's IDs.
  PolygonId first_id = 0;
};
struct PolygonRender {
  Mesh mesh;
  std::vector<PolygonId> vertices, corners, triangle_faces;
};
struct PolygonEditResult;
// Polygon authority is separate from its triangulated display derivative.
// Every selectable component and every face corner has a never-reused ID.
class PolygonMesh {
public:
  static PolygonMesh from_faces(std::span<const Vec3>,
                                const std::vector<std::vector<uint32_t>> &,
                                const PolygonLimits & = {});
  static PolygonMesh from_triangles(const Mesh &, const PolygonLimits & = {});
  static PolygonMesh cube(float size = 1, const PolygonLimits & = {});
  static PolygonMesh grid(unsigned rows = 1, unsigned columns = 1,
                          float size = 1, const PolygonLimits & = {});
  const std::vector<PolygonVertex> &vertices() const { return vertices_; }
  const std::vector<PolygonFace> &faces() const { return faces_; }
  const std::vector<PolygonEdge> &edges() const { return edges_; }
  PolygonId next_id() const { return next_; }
  void validate(const PolygonLimits & = {}) const;
  PolygonRender triangulate(const PolygonLimits & = {}) const;
  nlohmann::json encode() const;
  static PolygonMesh decode(const nlohmann::json &, const PolygonLimits & = {});

private:
  std::vector<PolygonVertex> vertices_;
  std::vector<PolygonFace> faces_;
  std::vector<PolygonEdge> edges_;
  PolygonId next_ = 1;
  friend struct PolygonKernel;
  friend PolygonEditResult
  edit_polygon(const PolygonMesh &, const PolygonEdit &, const PolygonLimits &);
};
struct PolygonEditResult {
  std::optional<PolygonMesh> replacement;
  std::vector<PolygonId> created_vertices, created_faces;
};
// Detached/transactional. Cancellation, invalid topology and resource failures
// leave the source and its next-ID counter untouched. A genuine no-op has no
// replacement, so callers must not create a history entry.
PolygonEditResult edit_polygon(const PolygonMesh &, const PolygonEdit &,
                               const PolygonLimits & = {});
} // namespace dome
