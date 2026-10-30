// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon-source.hpp"
#include "dome/humanoid-creator.hpp"
#include "polygon-source-metadata.hpp"
#include <map>
#include <set>
namespace dome {
namespace {
struct Budget {
  const PolygonLimits &limits;
  size_t work = 0;
  void visit(size_t count = 1) {
    if (count > limits.maximum_work - std::min(work, limits.maximum_work))
      throw std::length_error(
          "Polygon source correspondence exceeds its work budget");
    work += count;
    if (limits.cancelled && limits.cancelled())
      throw std::runtime_error("Polygon source preparation cancelled");
  }
};
bool same(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
struct SourceFace {
  std::vector<uint32_t> native;
  size_t first = 0, count = 0;
};
} // namespace
struct PolygonSourceAccess {
  static void attributes(PolygonMesh &mesh,
                         const std::vector<SourceFace> &faces, const Mesh &rest,
                         Budget &budget) {
    std::map<std::pair<PolygonId, PolygonId>, std::array<Vec3, 2>> edges;
    std::set<std::pair<PolygonId, PolygonId>> hard;
    for (size_t f = 0; f < faces.size(); ++f)
      for (size_t c = 0; c < faces[f].native.size(); ++c) {
        budget.visit();
        auto &corner = mesh.faces_[f].corners[c];
        corner.uv = rest.uv.empty() ? std::array<float, 2>{}
                                    : rest.uv[faces[f].native[c]];
        corner.generated_uv = rest.uv.empty();
        if (!rest.normals.empty()) {
          auto next = (c + 1) % faces[f].native.size();
          auto a = corner.vertex, b = mesh.faces_[f].corners[next].vertex;
          std::array<Vec3, 2> normals{rest.normals[faces[f].native[c]],
                                      rest.normals[faces[f].native[next]]};
          if (a > b)
            std::swap(normals[0], normals[1]);
          auto key = std::minmax(a, b);
          auto [old, inserted] = edges.emplace(key, normals);
          if (!inserted && (!same(old->second[0], normals[0]) ||
                            !same(old->second[1], normals[1])))
            hard.emplace(key);
        }
      }
    for (auto &edge : mesh.edges_) {
      budget.visit();
      edge.hard = hard.contains(std::minmax(edge.a, edge.b));
    }
  }
};
PolygonSourceTopology
prepare_polygon_topology(const Mesh &rest, const CreatorAppearance *appearance,
                         const PolygonLimits &limits) {
  auto stage_limits = limits;
  stage_limits.maximum_work /= 4;
  Budget budget{stage_limits};
  budget.visit();
  if (rest.positions.size() > limits.maximum_vertices ||
      rest.indices.size() / 3 > limits.maximum_faces ||
      rest.indices.size() > limits.maximum_corners || rest.indices.size() % 3 ||
      (!rest.uv.empty() && rest.uv.size() != rest.positions.size()) ||
      (!rest.normals.empty() && rest.normals.size() != rest.positions.size()))
    throw std::length_error(
        "Polygon source exceeds its supported attribute domain");
  const ImportedPrimitive *primitive = nullptr;
  if (appearance && appearance->asset)
    for (auto &candidate : appearance->asset->primitives) {
      budget.visit();
      if (candidate.source_mesh == appearance->source_mesh &&
          candidate.source_primitive == appearance->source_primitive &&
          candidate.source_node == appearance->source_node) {
        if (primitive)
          throw std::invalid_argument("Ambiguous original polygon primitive");
        primitive = &candidate;
      }
    }
  polygon_source_detail::Metadata extras;
  if (primitive && !primitive->extras_json.empty()) {
    if (primitive->extras_json.size() > 64 * 1024 * 1024)
      throw std::length_error(
          "Original polygon metadata exceeds its byte budget");
    extras = polygon_source_detail::parse(primitive->extras_json,
                                          primitive->mesh.positions.size(),
                                          limits, [&] { budget.visit(); });
  }
  if (!extras.version_present) {
    if (extras.polygons_present)
      throw std::invalid_argument(
          "Original polygon metadata has no supported version");
    PolygonSourceTopology out{PolygonMesh::from_triangles(rest, stage_limits)};
    auto render = out.topology.triangulate(stage_limits);
    std::map<PolygonId, uint32_t> native;
    for (size_t i = 0; i < out.topology.vertices().size(); ++i) {
      budget.visit();
      native.emplace(out.topology.vertices()[i].id, uint32_t(i));
    }
    for (auto vertex : render.vertices) {
      budget.visit();
      out.source_vertices.push_back(native.at(vertex));
    }
    return out;
  }
  if (extras.version != 1 || !extras.vertices_present ||
      !extras.corners_present || !extras.polygons_present)
    throw std::invalid_argument("Unsupported original polygon witness");
  const auto &source_vertices = extras.vertices,
             &source_corners = extras.corners;
  const auto &polygons = extras.polygons;
  const size_t canonical_size = primitive->mesh.positions.size(),
               triangles = rest.indices.size() / 3;
  if (source_vertices.size() != canonical_size ||
      source_corners.size() != canonical_size ||
      appearance->source_vertices.size() != rest.positions.size() ||
      primitive->mesh.indices.size() != rest.indices.size() ||
      polygons.size() > limits.maximum_faces)
    throw std::invalid_argument(
        "Original polygon witness no longer matches its rest surface");
  std::map<uint32_t, uint32_t> canonical_to_native;
  for (size_t i = 0; i < rest.positions.size(); ++i) {
    budget.visit();
    auto canonical = appearance->source_vertices[i];
    if (canonical >= canonical_size)
      throw std::invalid_argument("Original polygon vertex correspondence is "
                                  "outside its source domain");
    auto [entry, inserted] =
        canonical_to_native.emplace(canonical, uint32_t(i));
    auto n = entry->second;
    if (!inserted &&
        (!same(rest.positions[n], rest.positions[i]) ||
         (!rest.uv.empty() && rest.uv[n] != rest.uv[i]) ||
         (!rest.normals.empty() && !same(rest.normals[n], rest.normals[i]))))
      throw std::invalid_argument("Repeated original accessor vertex has "
                                  "incompatible native attributes");
  }
  for (size_t i = 0; i < rest.indices.size(); ++i) {
    budget.visit();
    auto n = rest.indices[i];
    if (n >= rest.positions.size() ||
        appearance->source_vertices[n] != primitive->mesh.indices[i])
      throw std::invalid_argument("Original polygon triangle order changed");
  }
  std::vector<SourceFace> faces;
  std::vector<bool> covered(triangles, false);
  std::set<uint64_t> face_ids, global_corners;
  std::map<uint64_t, uint32_t> vertex_map;
  std::vector<Vec3> positions;
  std::vector<std::vector<uint32_t>> loops;
  for (auto &face : polygons) {
    budget.visit();
    if (!face_ids.insert(face.source_face).second)
      throw std::invalid_argument("Original source face identity repeats");
    auto first = face.first_triangle, count = face.triangle_count;
    const auto &corners = face.vertices;
    if (corners.size() < 3 || corners.size() > limits.maximum_face_corners ||
        count != corners.size() - 2 || first > triangles ||
        count > triangles - first)
      throw std::invalid_argument("Invalid original polygon triangle range");
    SourceFace f{{}, size_t(first), size_t(count)};
    std::map<uint64_t, uint32_t> corner_native;
    std::vector<uint64_t> boundary;
    std::vector<uint32_t> loop;
    for (auto &value : corners) {
      budget.visit();
      auto canonical = value;
      if (canonical >= canonical_size ||
          !canonical_to_native.contains(uint32_t(canonical)))
        throw std::invalid_argument(
            "Original N-gon corner is absent from its rest surface");
      auto n = canonical_to_native.at(uint32_t(canonical));
      auto corner = source_corners[canonical],
           vertex = source_vertices[canonical];
      if (!corner_native.emplace(corner, n).second ||
          !global_corners.insert(corner).second)
        throw std::invalid_argument("Original polygon corner identity repeats");
      auto [entry, inserted] =
          vertex_map.emplace(vertex, uint32_t(positions.size()));
      if (inserted)
        positions.push_back(rest.positions[n]);
      else if (!same(positions.at(entry->second), rest.positions[n]))
        throw std::invalid_argument(
            "Original shared vertex has incompatible rest positions");
      f.native.push_back(n);
      boundary.push_back(corner);
      loop.push_back(entry->second);
    }
    std::map<std::pair<uint64_t, uint64_t>, int> edges;
    for (size_t t = f.first; t < f.first + f.count; ++t) {
      budget.visit();
      if (covered[t])
        throw std::invalid_argument("Original polygon triangle ranges overlap");
      covered[t] = true;
      uint64_t ids[3];
      for (size_t c = 0; c < 3; ++c) {
        auto n = rest.indices[t * 3 + c],
             canonical = appearance->source_vertices[n];
        auto corner = source_corners[canonical];
        auto it = corner_native.find(corner);
        if (it == corner_native.end())
          throw std::invalid_argument(
              "Original triangle uses a corner outside its N-gon");
        auto representative = it->second;
        if (!same(rest.positions[n], rest.positions[representative]) ||
            (!rest.uv.empty() && rest.uv[n] != rest.uv[representative]) ||
            (!rest.normals.empty() &&
             !same(rest.normals[n], rest.normals[representative])) ||
            source_vertices[canonical] !=
                source_vertices[appearance->source_vertices[representative]])
          throw std::invalid_argument(
              "Repeated original corner has incompatible attributes");
        ids[c] = corner;
      }
      if (ids[0] == ids[1] || ids[1] == ids[2] || ids[2] == ids[0])
        throw std::invalid_argument(
            "Original polygon has a collapsed source triangle");
      for (size_t c = 0; c < 3; ++c) {
        auto a = ids[c], b = ids[(c + 1) % 3];
        auto key = std::minmax(a, b);
        edges[key] += (a < b ? 1 : -1);
      }
    }
    for (size_t c = 0; c < boundary.size(); ++c) {
      auto a = boundary[c], b = boundary[(c + 1) % boundary.size()];
      edges[std::minmax(a, b)] -= (a < b ? 1 : -1);
    }
    for (auto [edge, balance] : edges) {
      budget.visit();
      if (balance)
        throw std::invalid_argument(
            "Original polygon boundary does not match its triangles");
    }
    faces.push_back(std::move(f));
    loops.push_back(std::move(loop));
  }
  for (bool present : covered) {
    budget.visit();
    if (!present)
      throw std::invalid_argument("Original polygons omit source triangles");
  }
  PolygonSourceTopology out{
      PolygonMesh::from_faces(positions, loops, stage_limits), {}, true};
  PolygonSourceAccess::attributes(out.topology, faces, rest, budget);
  out.topology.validate(stage_limits);
  std::map<PolygonId, uint32_t> corners;
  for (size_t f = 0; f < faces.size(); ++f)
    for (size_t c = 0; c < faces[f].native.size(); ++c) {
      budget.visit();
      corners.emplace(out.topology.faces()[f].corners[c].id,
                      faces[f].native[c]);
    }
  auto render = out.topology.triangulate(stage_limits);
  for (auto corner : render.corners) {
    budget.visit();
    out.source_vertices.push_back(corners.at(corner));
  }
  return out;
}
} // namespace dome
