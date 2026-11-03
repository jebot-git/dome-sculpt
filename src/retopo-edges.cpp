// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/retopo.hpp"
#include <algorithm>
#include <set>
#include <type_traits>
namespace dome {
namespace {
void stop(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled()) throw std::runtime_error("Topology operation cancelled");
}
struct Boundary {
  unsigned count = 0;
  int direction = 0;
  uint32_t material = 0;
};
Boundary boundary(const RetopoMesh &mesh, const RetopoEdge &edge,
                  const std::function<bool()> &cancelled) {
  Boundary use;
  for (const auto &[id, face] : mesh.faces()) {
    (void)id; stop(cancelled);
    for (size_t i = 0; i < face.vertices.size(); ++i) {
      auto a = face.vertices[i], b = face.vertices[(i + 1) % face.vertices.size()];
      if ((a == edge.a && b == edge.b) || (a == edge.b && b == edge.a)) {
        ++use.count; use.direction = a == edge.a ? 1 : -1; use.material = face.material;
      }
    }
  }
  if (use.count > 1) throw std::runtime_error("Choose a wire or boundary edge");
  return use;
}
}
TopologyID RetopoMesh::bridge_edges(std::array<TopologyID, 4> ids,
                                    const std::function<bool()> &cancelled) {
  stop(cancelled);
  if (std::set<TopologyID>(ids.begin(), ids.end()).size() != 4)
    throw std::invalid_argument("Choose two disjoint boundary edges");
  auto first = edge_id(ids[0], ids[1]), second = edge_id(ids[2], ids[3]);
  if (!first || !second) throw std::invalid_argument("Connect both endpoint pairs before bridging");
  auto use = boundary(*this, edges_.at(first), cancelled);
  boundary(*this, edges_.at(second), cancelled);
  RetopoMesh draft = *this;
  auto face = draft.add_face(ids, use.material);
  draft.validate(cancelled); stop(cancelled);
  static_assert(std::is_nothrow_move_assignable_v<RetopoMesh>);
  *this = std::move(draft);
  return face;
}
RetopoExtrusion RetopoMesh::extrude_edge(TopologyID id, std::array<Vec3, 2> positions,
                                         const std::function<bool()> &cancelled) {
  stop(cancelled);
  const auto edge = edges_.at(id);
  const auto use = boundary(*this, edge, cancelled);
  RetopoMesh draft = *this;
  auto a = vertices_.at(edge.a), b = vertices_.at(edge.b);
  a.position = positions[0]; b.position = positions[1]; a.pinned = b.pinned = false;
  RetopoExtrusion result;
  result.outer = {draft.add_vertex(a), draft.add_vertex(b)};
  std::array<TopologyID, 4> face{edge.a, edge.b, result.outer[1], result.outer[0]};
  if (use.direction > 0) std::reverse(face.begin(), face.end());
  result.face = draft.add_face(face, use.material);
  result.edge = draft.edge_id(result.outer[0], result.outer[1]);
  draft.set_seam(result.edge, edge.seam);
  draft.validate(cancelled); stop(cancelled);
  *this = std::move(draft);
  return result;
}
} // namespace dome
