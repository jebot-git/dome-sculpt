// SPDX-License-Identifier: GPL-3.0-or-later
// Boundary incidence informed by Wings wings_face.erl outer_edges:
// Copyright (c) 2001-2011 Bjorn Gustavsson; third_party/wings/license.terms.
// Relax blend informed by VCGLib smooth.h, VertexCoordLaplacianBlend:
// Copyright(C) 2004-2016 Visual Computing Lab, ISTI-CNR, GPL-2.0-or-later.
#include "dome/retopo.hpp"
#include <bit>
#include <limits>
#include <nlohmann/json.hpp>
#include <numeric>
#include <set>
namespace dome {
namespace {
constexpr size_t budget = 100'000;
void stop(const std::function<bool()> &fn) {
  if (fn && fn())
    throw std::runtime_error("Topology operation cancelled");
}
void position(Vec3 p) {
  if (!finite(p) ||
      std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) > 100)
    throw std::invalid_argument(
        "Topology vertex outside finite 100m workspace");
}
void vertex(const RetopoVertex &v) {
  position(v.position);
  if (!std::isfinite(v.uv[0]) || !std::isfinite(v.uv[1]))
    throw std::invalid_argument("Nonfinite topology UV");
}
using Edge = std::array<TopologyID, 2>;
Edge edge(TopologyID a, TopologyID b) {
  return {std::min(a, b), std::max(a, b)};
}
double dp(Vec3 a, Vec3 b) {
  return double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z;
}
using XY = std::array<double, 2>;
double area(XY a, XY b, XY c) {
  return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}
bool on_segment(XY a, XY b, XY p) {
  return std::abs(area(a, b, p)) <= 1e-14 &&
         p[0] >= std::min(a[0], b[0]) - 1e-14 &&
         p[0] <= std::max(a[0], b[0]) + 1e-14 &&
         p[1] >= std::min(a[1], b[1]) - 1e-14 &&
         p[1] <= std::max(a[1], b[1]) + 1e-14;
}
bool crosses(XY a, XY b, XY c, XY d) {
  double x = area(a, b, c), y = area(a, b, d), z = area(c, d, a),
         w = area(c, d, b);
  return (((x > 0 && y < 0) || (x < 0 && y > 0)) &&
          ((z > 0 && w < 0) || (z < 0 && w > 0))) ||
         on_segment(a, b, c) || on_segment(a, b, d) || on_segment(c, d, a) ||
         on_segment(c, d, b);
}
// Ear clipping keeps every boundary vertex, including collinear cut vertices,
// so cap triangulation cannot introduce a T junction on an original cut edge.
std::vector<std::array<size_t, 3>> ears(const std::vector<XY> &xy,
                                        const std::function<bool()> &fn = {}) {
  if (xy.size() < 3 || xy.size() > 4096)
    throw std::length_error("Contour size outside limit");
  size_t work = 0;
  for (size_t i = 0; i < xy.size(); i++)
    for (size_t j = i + 1; j < xy.size(); j++) {
      if (++work % 256 == 0)
        stop(fn);
      size_t in = (i + 1) % xy.size(), jn = (j + 1) % xy.size();
      if (in == j || jn == i)
        continue;
      if (crosses(xy[i], xy[in], xy[j], xy[jn]))
        throw std::runtime_error("Crossing or touching contour edges");
    }
  double total = 0;
  for (size_t i = 0; i < xy.size(); i++)
    total += xy[i][0] * xy[(i + 1) % xy.size()][1] -
             xy[i][1] * xy[(i + 1) % xy.size()][0];
  if (std::abs(total) < 1e-14)
    throw std::runtime_error("Degenerate contour");
  double sign = total > 0 ? 1 : -1;
  std::vector<size_t> p(xy.size());
  std::iota(p.begin(), p.end(), 0);
  std::vector<std::array<size_t, 3>> out;
  while (p.size() > 3) {
    bool found = false;
    for (size_t i = 0; i < p.size(); i++) {
      auto a = p[(i + p.size() - 1) % p.size()], b = p[i],
           c = p[(i + 1) % p.size()];
      if (area(xy[a], xy[b], xy[c]) * sign <= 1e-14)
        continue;
      bool contains = false;
      for (auto q : p) {
        if (++work % 256 == 0)
          stop(fn);
        if (work > 32'000'000)
          throw std::length_error("Contour triangulation work budget exceeded");
        if (q != a && q != b && q != c &&
            area(xy[a], xy[b], xy[q]) * sign >= -1e-14 &&
            area(xy[b], xy[c], xy[q]) * sign >= -1e-14 &&
            area(xy[c], xy[a], xy[q]) * sign >= -1e-14) {
          contains = true;
          break;
        }
      }
      if (contains)
        continue;
      out.push_back({a, b, c});
      p.erase(p.begin() + i);
      found = true;
      break;
    }
    if (!found)
      throw std::runtime_error(
          "Ambiguous contour cannot be triangulated safely");
  }
  if (area(xy[p[0]], xy[p[1]], xy[p[2]]) * sign <= 1e-14)
    throw std::runtime_error("Degenerate final contour triangle");
  out.push_back({p[0], p[1], p[2]});
  stop(fn);
  return out;
}
std::vector<std::array<size_t, 3>>
face_triangles(const RetopoFace &f,
               const std::map<TopologyID, RetopoVertex> &vertices) {
  if (f.vertices.size() != 3 && f.vertices.size() != 4)
    throw std::runtime_error("Retopology faces must be triangles or quads");
  Vec3 n{};
  for (size_t i = 0; i < f.vertices.size(); i++) {
    auto a = vertices.at(f.vertices[i]).position,
         b = vertices.at(f.vertices[(i + 1) % f.vertices.size()]).position;
    n += cross(a, b);
  }
  if (length(n) < 1e-10f)
    throw std::runtime_error("Degenerate or crossing retopology face");
  int axis = std::abs(n.y) > std::abs(n.x) ? 1 : 0;
  if (std::abs(n.z) > (axis == 0 ? std::abs(n.x) : std::abs(n.y)))
    axis = 2;
  std::vector<XY> xy;
  for (auto id : f.vertices) {
    auto p = vertices.at(id).position;
    xy.push_back(axis == 0   ? XY{p.y, p.z}
                 : axis == 1 ? XY{p.x, p.z}
                             : XY{p.x, p.y});
  }
  auto triangles = ears(xy);
  for (auto t : triangles) {
    auto a = vertices.at(f.vertices[t[0]]).position,
         b = vertices.at(f.vertices[t[1]]).position,
         c = vertices.at(f.vertices[t[2]]).position;
    if (dp(cross(b - a, c - a), n) <= 1e-20)
      throw std::runtime_error("Folded retopology face");
  }
  return triangles;
}
void preserve_orientation(const std::map<TopologyID, RetopoFace> &faces,
                          const std::map<TopologyID, RetopoVertex> &before,
                          const std::map<TopologyID, RetopoVertex> &after) {
  for (auto &[id, face] : faces)
    for (auto triangle : face_triangles(face, before)) {
      auto a = before.at(face.vertices[triangle[0]]).position;
      auto b = before.at(face.vertices[triangle[1]]).position;
      auto c = before.at(face.vertices[triangle[2]]).position;
      auto x = after.at(face.vertices[triangle[0]]).position;
      auto y = after.at(face.vertices[triangle[1]]).position;
      auto z = after.at(face.vertices[triangle[2]]).position;
      if (dp(cross(b - a, c - a), cross(y - x, z - x)) <= 0)
        throw std::runtime_error("Topology change would invert a face");
    }
}
TopologyID json_id(const nlohmann::json &j) {
  if (!j.is_number_unsigned() || j.get<uint64_t>() == 0)
    throw std::runtime_error("Invalid stable topology ID");
  return j.get<uint64_t>();
}
} // namespace
TopologyID RetopoMesh::edge_id(TopologyID a, TopologyID b) const {
  auto target = edge(a, b);
  for (auto &[id, e] : edges_)
    if (edge(e.a, e.b) == target)
      return id;
  return 0;
}
TopologyID RetopoMesh::add_vertex(RetopoVertex v) {
  vertex(v);
  if (vertices_.size() >= budget || next_ == UINT64_MAX)
    throw std::length_error("Topology vertex/ID budget exceeded");
  auto id = next_;
  vertices_.emplace(id, v);
  ++next_;
  return id;
}
TopologyID RetopoMesh::add_edge(TopologyID a, TopologyID b, bool seam) {
  if (a == b || !vertices_.contains(a) || !vertices_.contains(b))
    throw std::invalid_argument("Invalid edge endpoints");
  if (auto id = edge_id(a, b)) {
    if (seam)
      edges_.at(id).seam = true;
    return id;
  }
  if (edges_.size() >= budget * 3 || next_ == UINT64_MAX)
    throw std::length_error("Topology edge/ID budget exceeded");
  auto id = next_;
  edges_.emplace(id, RetopoEdge{a, b, seam});
  ++next_;
  return id;
}
TopologyID RetopoMesh::add_face(std::span<const TopologyID> ids,
                                uint32_t material) {
  if (faces_.size() >= budget || ids.size() < 3 || ids.size() > 4)
    throw std::length_error("Topology face budget/size exceeded");
  RetopoMesh draft = *this;
  for (auto id : ids)
    if (!vertices_.contains(id))
      throw std::invalid_argument("Face refers to missing vertex");
  if (std::set<TopologyID>(ids.begin(), ids.end()).size() != ids.size())
    throw std::invalid_argument("Repeated face vertex");
  for (size_t i = 0; i < ids.size(); i++)
    draft.add_edge(ids[i], ids[(i + 1) % ids.size()]);
  if (draft.next_ == UINT64_MAX)
    throw std::length_error("Topology IDs exhausted");
  auto id = draft.next_++;
  draft.faces_.emplace(id, RetopoFace{{ids.begin(), ids.end()}, material});
  draft.validate();
  *this = std::move(draft);
  return id;
}
void RetopoMesh::remove_face(TopologyID id) {
  RetopoMesh draft = *this;
  if (!draft.faces_.erase(id))
    throw std::out_of_range("Missing face");
  draft.validate();
  *this = std::move(draft);
}
void RetopoMesh::remove_edge(TopologyID id) {
  auto e = edges_.at(id);
  for (auto &[unused, f] : faces_)
    for (size_t i = 0; i < f.vertices.size(); i++)
      if (edge(f.vertices[i], f.vertices[(i + 1) % f.vertices.size()]) ==
          edge(e.a, e.b))
        throw std::runtime_error("Edge still has incident faces");
  edges_.erase(id);
}
void RetopoMesh::remove_vertex(TopologyID id) {
  if (!vertices_.contains(id))
    throw std::out_of_range("Missing vertex");
  for (auto &[unused, e] : edges_)
    if (e.a == id || e.b == id)
      throw std::runtime_error("Vertex still has incident edges");
  vertices_.erase(id);
}
void RetopoMesh::move_vertex(TopologyID id, Vec3 p) {
  position(p);
  RetopoMesh draft = *this;
  draft.vertices_.at(id).position = p;
  draft.validate();
  preserve_orientation(faces_, vertices_, draft.vertices_);
  *this = std::move(draft);
}
void RetopoMesh::set_pinned(TopologyID id, bool value) {
  vertices_.at(id).pinned = value;
}
void RetopoMesh::set_seam(TopologyID id, bool value) {
  edges_.at(id).seam = value;
}
void RetopoMesh::validate(const std::function<bool()> &fn) const {
  stop(fn);
  if (!next_ || vertices_.size() > budget || edges_.size() > budget * 3 ||
      faces_.size() > budget)
    throw std::length_error("Topology budget exceeded");
  std::set<TopologyID> used;
  auto identity = [&](TopologyID id) {
    if (!id || id >= next_ || !used.insert(id).second)
      throw std::runtime_error("Invalid or reused topology ID");
  };
  for (auto &[id, v] : vertices_) {
    if (used.size() % 256 == 0)
      stop(fn);
    identity(id);
    vertex(v);
  }
  std::map<Edge, TopologyID> indexed;
  for (auto &[id, e] : edges_) {
    identity(id);
    if (e.a == e.b || !vertices_.contains(e.a) || !vertices_.contains(e.b) ||
        !indexed.emplace(edge(e.a, e.b), id).second)
      throw std::runtime_error("Invalid or duplicate topology edge");
  }
  struct Use {
    unsigned count = 0;
    int orientation = 0;
  };
  std::map<Edge, Use> incidence;
  std::set<std::vector<TopologyID>> face_sets;
  for (auto &[id, f] : faces_) {
    stop(fn);
    identity(id);
    auto sorted = f.vertices;
    std::sort(sorted.begin(), sorted.end());
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end() ||
        !face_sets.insert(sorted).second)
      throw std::runtime_error("Repeated or duplicate face");
    for (size_t i = 0; i < f.vertices.size(); i++) {
      auto a = f.vertices[i], b = f.vertices[(i + 1) % f.vertices.size()];
      if (!vertices_.contains(a) || !indexed.contains(edge(a, b)))
        throw std::runtime_error("Missing topology face edge");
      auto &use = incidence[edge(a, b)];
      ++use.count;
      use.orientation += a < b ? 1 : -1;
      if (use.count > 2 || (use.count == 2 && use.orientation != 0))
        throw std::runtime_error(
            "Nonmanifold or inconsistently oriented topology edge");
    }
    face_triangles(f, vertices_);
  }
  // A manifold edge check alone does not catch two face fans touching only at
  // one vertex. Require a connected link, with zero or two boundary ends.
  std::map<TopologyID, std::map<TopologyID, std::vector<TopologyID>>> links;
  for (auto &[id, f] : faces_)
    for (size_t i = 0; i < f.vertices.size(); i++) {
      auto v = f.vertices[i],
           a = f.vertices[(i + f.vertices.size() - 1) % f.vertices.size()],
           b = f.vertices[(i + 1) % f.vertices.size()];
      links[v][a].push_back(b);
      links[v][b].push_back(a);
    }
  for (auto &[vertex, link] : links) {
    stop(fn);
    size_t ends = 0;
    for (auto &[v, neighbors] : link) {
      if (neighbors.size() > 2)
        throw std::runtime_error("Nonmanifold topology vertex link");
      ends += neighbors.size() == 1;
    }
    if (ends != 0 && ends != 2)
      throw std::runtime_error("Nonmanifold topology vertex boundary");
    std::set<TopologyID> reached;
    std::vector<TopologyID> pending = {link.begin()->first};
    while (!pending.empty()) {
      auto id = pending.back();
      pending.pop_back();
      if (!reached.insert(id).second)
        continue;
      for (auto next : link.at(id))
        pending.push_back(next);
    }
    if (reached.size() != link.size())
      throw std::runtime_error("Disconnected face fans at topology vertex");
  }
  stop(fn);
}
RetopoSurface RetopoMesh::triangulate(const std::function<bool()> &fn) const {
  validate(fn);
  if (faces_.empty())
    throw std::runtime_error("Retopology has no faces");
  RetopoSurface out;
  std::map<TopologyID, uint32_t> indices;
  std::map<Edge, bool> seams;
  for (auto &[id, e] : edges_)
    seams[edge(e.a, e.b)] = e.seam;
  for (auto &[id, f] : faces_) {
    stop(fn);
    for (auto tri : face_triangles(f, vertices_)) {
      out.source_faces.push_back(id);
      out.materials.push_back(f.material);
      for (size_t k = 0; k < 3; k++) {
        auto v = f.vertices[tri[k]], next = f.vertices[tri[(k + 1) % 3]];
        auto [it, added] =
            indices.emplace(v, uint32_t(out.mesh.positions.size()));
        if (added) {
          auto &vert = vertices_.at(v);
          out.source_vertices.push_back(v);
          out.mesh.positions.push_back(vert.position);
          out.mesh.uv.push_back(vert.uv);
          out.mesh.uv_pins.push_back(vert.pinned);
          out.mesh.normals.push_back({});
        }
        out.mesh.indices.push_back(it->second);
        out.mesh.uv_seams.push_back(seams[edge(v, next)]);
      }
    }
  }
  for (size_t t = 0; t < out.mesh.indices.size(); t += 3) {
    auto a = out.mesh.indices[t], b = out.mesh.indices[t + 1],
         c = out.mesh.indices[t + 2];
    auto n = cross(out.mesh.positions[b] - out.mesh.positions[a],
                   out.mesh.positions[c] - out.mesh.positions[a]);
    out.mesh.normals[a] += n;
    out.mesh.normals[b] += n;
    out.mesh.normals[c] += n;
  }
  for (auto &n : out.mesh.normals) {
    if (length(n) < 1e-12f)
      throw std::runtime_error("Cancelled retopology normals");
    n = normalized(n);
  }
  out.mesh.validate();
  stop(fn);
  return out;
}
void RetopoMesh::project(std::span<const TopologyID> ids,
                         const SurfaceQuery &source, float maximum,
                         const std::function<bool()> &fn) {
  if (!std::isfinite(maximum) || maximum < 0 || maximum > 100)
    throw std::invalid_argument("Invalid snap distance");
  if (ids.size() > vertices_.size() ||
      std::set<TopologyID>(ids.begin(), ids.end()).size() != ids.size())
    throw std::invalid_argument("Invalid or duplicate projection selection");
  RetopoMesh draft = *this;
  for (auto id : ids) {
    stop(fn);
    auto &v = draft.vertices_.at(id);
    auto hit = source.closest(v.position);
    if (hit.distance > maximum)
      throw std::runtime_error("Surface is outside snap distance");
    v.position = hit.position;
  }
  draft.validate(fn);
  preserve_orientation(faces_, vertices_, draft.vertices_);
  stop(fn);
  *this = std::move(draft);
}
void RetopoMesh::relax(const SurfaceQuery &source,
                       const RetopoRelaxOptions &o) {
  if (o.iterations > 100 || !std::isfinite(o.strength) || o.strength < 0 ||
      o.strength > 1 || !std::isfinite(o.maximum_snap_distance) ||
      o.maximum_snap_distance < 0 || o.maximum_snap_distance > 100)
    throw std::invalid_argument("Invalid surface relax options");
  validate(o.cancelled);
  RetopoMesh draft = *this;
  std::map<TopologyID, std::set<TopologyID>> neighbors;
  std::map<Edge, unsigned> uses;
  for (auto &[id, f] : faces_)
    for (size_t i = 0; i < f.vertices.size(); i++)
      ++uses[edge(f.vertices[i], f.vertices[(i + 1) % f.vertices.size()])];
  std::set<TopologyID> fixed;
  // Wings outer_edges' paired-edge cancellation becomes explicit incidence;
  // unlike that manifold-only helper we also retain wires as fixed boundaries.
  for (auto &[id, e] : edges_) {
    neighbors[e.a].insert(e.b);
    neighbors[e.b].insert(e.a);
    if (uses[edge(e.a, e.b)] != 2 || e.seam) {
      fixed.insert(e.a);
      fixed.insert(e.b);
    }
  }
  for (unsigned iteration = 0; iteration < o.iterations; iteration++) {
    auto previous = draft.vertices_;
    for (auto &[id, v] : draft.vertices_) {
      stop(o.cancelled);
      if (v.pinned || fixed.contains(id) || neighbors[id].empty())
        continue;
      Vec3 sum{};
      for (auto other : neighbors[id])
        sum += previous.at(other).position;
      // VCGLib's blended Laplacian, with Jacobi snapshots and hard boundaries.
      auto p = previous.at(id).position +
               (sum / float(neighbors[id].size()) - previous.at(id).position) *
                   o.strength;
      auto hit = source.closest(p);
      if (hit.distance > o.maximum_snap_distance)
        throw std::runtime_error("Relax projection exceeds snap distance");
      v.position = hit.position;
    }
    draft.validate(o.cancelled);
    for (auto &[id, f] : faces_) {
      auto tris = face_triangles(f, vertices_);
      for (auto t : tris) {
        auto a = vertices_.at(f.vertices[t[0]]).position,
             b = vertices_.at(f.vertices[t[1]]).position,
             c = vertices_.at(f.vertices[t[2]]).position;
        auto x = draft.vertices_.at(f.vertices[t[0]]).position,
             y = draft.vertices_.at(f.vertices[t[1]]).position,
             z = draft.vertices_.at(f.vertices[t[2]]).position;
        if (dp(cross(b - a, c - a), cross(y - x, z - x)) <= 0)
          throw std::runtime_error("Relax would invert a face");
      }
    }
  }
  stop(o.cancelled);
  *this = std::move(draft);
}
std::string RetopoMesh::encode() const {
  validate();
  nlohmann::json j = {{"version", 1},
                      {"next", next_},
                      {"vertices", nlohmann::json::array()},
                      {"edges", nlohmann::json::array()},
                      {"faces", nlohmann::json::array()}};
  for (auto &[id, v] : vertices_)
    j["vertices"].push_back(
        {id, {v.position.x, v.position.y, v.position.z}, v.uv, v.pinned});
  for (auto &[id, e] : edges_)
    j["edges"].push_back({id, e.a, e.b, e.seam});
  for (auto &[id, f] : faces_)
    j["faces"].push_back({id, f.vertices, f.material});
  return j.dump();
}
RetopoMesh RetopoMesh::decode(std::string_view text) {
  if (text.size() > 64 * 1024 * 1024)
    throw std::length_error("Topology file exceeds byte budget");
  // Bound nesting before recursive JSON parse, respecting escaped strings.
  bool quoted = false, escaped = false;
  int depth = 0;
  for (char c : text) {
    if (quoted) {
      if (escaped)
        escaped = false;
      else if (c == '\\')
        escaped = true;
      else if (c == '"')
        quoted = false;
    } else if (c == '"')
      quoted = true;
    else if (c == '{' || c == '[') {
      if (++depth > 32)
        throw std::length_error("Topology JSON nesting exceeds limit");
    } else if (c == '}' || c == ']')
      --depth;
  }
  auto j = nlohmann::json::parse(text);
  if (!j.is_object() || j.size() != 5 || j.at("version") != 1)
    throw std::runtime_error("Unsupported topology format");
  RetopoMesh out;
  out.next_ = json_id(j.at("next"));
  for (auto name : {"vertices", "edges", "faces"})
    if (!j.at(name).is_array() ||
        j.at(name).size() >
            (std::string_view(name) == "edges" ? budget * 3 : budget))
      throw std::length_error("Topology record budget exceeded");
  for (auto &v : j.at("vertices")) {
    if (!v.is_array() || v.size() != 4 || !v[1].is_array() ||
        v[1].size() != 3 || !v[2].is_array() || v[2].size() != 2 ||
        !v[3].is_boolean())
      throw std::runtime_error("Invalid topology vertex record");
    if (!out.vertices_
             .emplace(json_id(v[0]),
                      RetopoVertex{{v[1][0].get<float>(), v[1][1].get<float>(),
                                    v[1][2].get<float>()},
                                   v[2].get<std::array<float, 2>>(),
                                   v[3].get<bool>()})
             .second)
      throw std::runtime_error("Duplicate topology vertex ID");
  }
  for (auto &e : j.at("edges")) {
    if (!e.is_array() || e.size() != 4 || !e[3].is_boolean())
      throw std::runtime_error("Invalid topology edge record");
    if (!out.edges_
             .emplace(json_id(e[0]), RetopoEdge{json_id(e[1]), json_id(e[2]),
                                                e[3].get<bool>()})
             .second)
      throw std::runtime_error("Duplicate edge ID");
  }
  for (auto &f : j.at("faces")) {
    if (!f.is_array() || f.size() != 3 || !f[1].is_array() || f[1].size() < 3 ||
        f[1].size() > 4 || !f[2].is_number_unsigned() ||
        f[2].get<uint64_t>() > UINT32_MAX)
      throw std::runtime_error("Invalid topology face record");
    std::vector<TopologyID> ids;
    for (auto &v : f[1])
      ids.push_back(json_id(v));
    if (!out.faces_
             .emplace(json_id(f[0]), RetopoFace{ids, f[2].get<uint32_t>()})
             .second)
      throw std::runtime_error("Duplicate face ID");
  }
  out.validate();
  return out;
}
namespace {
using Point = std::array<uint32_t, 3>;
uint32_t float_bits(float x) { return x == 0 ? 0 : std::bit_cast<uint32_t>(x); }
Point point(Vec3 p) {
  return {float_bits(p.x), float_bits(p.y), float_bits(p.z)};
}
struct ClipVertex {
  Vec3 p{}, n{};
  std::array<float, 2> uv{};
  std::array<double, 3> bary{};
  bool cut = false, pinned = false;
};
bool inside_polygon(XY p, const std::vector<XY> &poly) {
  bool in = false;
  for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
    if (on_segment(poly[j], poly[i], p))
      throw std::runtime_error("Touching cap contours are ambiguous");
    if ((poly[i][1] > p[1]) != (poly[j][1] > p[1]) &&
        p[0] < (poly[j][0] - poly[i][0]) * (p[1] - poly[i][1]) /
                       (poly[j][1] - poly[i][1]) +
                   poly[i][0])
      in = !in;
  }
  return in;
}
} // namespace
PlaneCutResult plane_cut(const Mesh &m, const PlaneCutOptions &o,
                         std::span<const uint32_t> materials) {
  stop(o.cancelled);
  if (!finite(o.normal) || !std::isfinite(length(o.normal)) ||
      length(o.normal) < 1e-10f || !std::isfinite(o.offset) ||
      std::abs(o.offset) > 200 || !std::isfinite(o.epsilon) || o.epsilon <= 0 ||
      o.epsilon > .001f || !o.max_triangles || o.max_triangles > 2'000'000 ||
      o.max_contour_vertices < 3 || o.max_contour_vertices > 4096)
    throw std::invalid_argument("Invalid plane cut options");
  m.validate();
  auto topology = inspect_mesh(m, o.cancelled);
  if (o.cap && !topology.closed_manifold())
    throw std::runtime_error(
        "Capped cutting requires a closed consistently oriented manifold");
  if (!materials.empty() && materials.size() != m.indices.size() / 3)
    throw std::invalid_argument("Cut face material count mismatch");
  Vec3 normal = normalized(o.normal);
  auto distance = [&](Vec3 p) { return dp(normal, p) - o.offset; };
  size_t positive = 0, negative = 0;
  for (auto p : m.positions) {
    stop(o.cancelled);
    double d = distance(p);
    if (std::abs(d) <= o.epsilon)
      throw std::runtime_error(
          "Cut plane touches source vertices; move plane or reduce epsilon");
    positive += d > 0;
    negative += d < 0;
  }
  if (!positive)
    throw std::runtime_error("Plane cut removes the entire surface");
  PlaneCutResult out;
  if (!negative) {
    if (m.indices.size() / 3 > o.max_triangles)
      throw std::length_error("Cut output triangle budget exceeded");
    out.mesh = m;
    for (uint32_t t = 0; t < m.indices.size() / 3; t++) {
      out.source_triangles.push_back(t);
      out.materials.push_back(materials.empty() ? 0 : materials[t]);
      out.source_corner_barycentrics.insert(
          out.source_corner_barycentrics.end(),
          {{{1, 0, 0}}, {{0, 1, 0}}, {{0, 0, 1}}});
    }
    stop(o.cancelled);
    return out;
  }
  std::map<Point, Vec3> cut_positions;
  std::vector<std::array<Point, 2>> segments;
  auto append = [&](const std::array<ClipVertex, 3> &v, uint32_t source,
                    uint32_t material, std::array<uint8_t, 3> seams) {
    if (out.mesh.positions.size() > 2'000'000 - 3)
      throw std::length_error("Cut output vertex budget exceeded");
    if (out.source_triangles.size() >= o.max_triangles)
      throw std::length_error("Cut output triangle budget exceeded");
    for (size_t k = 0; k < 3; k++) {
      out.mesh.indices.push_back(uint32_t(out.mesh.positions.size()));
      out.mesh.positions.push_back(v[k].p);
      out.mesh.normals.push_back(v[k].n);
      if (!m.uv.empty())
        out.mesh.uv.push_back(v[k].uv);
      out.mesh.uv_pins.push_back(v[k].pinned);
      out.mesh.uv_seams.push_back(seams[k]);
      out.source_corner_barycentrics.push_back(
          {float(v[k].bary[0]), float(v[k].bary[1]), float(v[k].bary[2])});
    }
    out.source_triangles.push_back(source);
    out.materials.push_back(material);
  };
  for (size_t t = 0; t < m.indices.size(); t += 3) {
    if (t % 384 == 0)
      stop(o.cancelled);
    std::array<ClipVertex, 3> tri;
    for (size_t k = 0; k < 3; k++) {
      auto i = m.indices[t + k];
      tri[k].p = m.positions[i];
      tri[k].n = m.normals[i];
      tri[k].bary[k] = 1;
      if (!m.uv.empty())
        tri[k].uv = m.uv[i];
      if (!m.uv_pins.empty())
        tri[k].pinned = m.uv_pins[i];
    }
    std::vector<ClipVertex> clipped;
    for (size_t k = 0; k < 3; k++) {
      auto a = tri[k], b = tri[(k + 1) % 3];
      double da = distance(a.p), db = distance(b.p);
      if (da > 0)
        clipped.push_back(a);
      if ((da > 0) != (db > 0)) {
        double f = da / (da - db);
        ClipVertex v;
        v.cut = true;
        // Geometric intersections use canonical endpoint order. Duplicated UV
        // and normal vertices therefore share bit-identical cut positions.
        auto x = a.p, y = b.p;
        if (point(y) < point(x))
          std::swap(x, y);
        double dx = distance(x), dy = distance(y), q = dx / (dx - dy);
        v.p = {float(double(x.x) + (double(y.x) - x.x) * q),
               float(double(x.y) + (double(y.y) - x.y) * q),
               float(double(x.z) + (double(y.z) - x.z) * q)};
        v.n = a.n * float(1 - f) + b.n * float(f);
        if (!std::isfinite(length(v.n)) || length(v.n) < 1e-12f)
          v.n = normalized(cross(tri[1].p - tri[0].p, tri[2].p - tri[0].p));
        else
          v.n = normalized(v.n);
        for (size_t j = 0; j < 2; j++)
          v.uv[j] = float(double(a.uv[j]) * (1 - f) + double(b.uv[j]) * f);
        for (size_t j = 0; j < 3; j++)
          v.bary[j] = a.bary[j] * (1 - f) + b.bary[j] * f;
        if (o.cap) {
          cut_positions.emplace(point(v.p), v.p);
          if (cut_positions.size() > o.max_contour_vertices)
            throw std::length_error("Cut contour vertex budget exceeded");
        }
        clipped.push_back(v);
      }
    }
    if (clipped.empty())
      continue;
    for (size_t k = 0; k < clipped.size(); k++)
      if (o.cap && clipped[k].cut && clipped[(k + 1) % clipped.size()].cut)
        segments.push_back(
            {point(clipped[k].p), point(clipped[(k + 1) % clipped.size()].p)});
    for (size_t k = 1; k + 1 < clipped.size(); k++) {
      std::array<ClipVertex, 3> v = {clipped[0], clipped[k], clipped[k + 1]};
      std::array<uint8_t, 3> seams{};
      for (size_t j = 0; j < 3; j++) {
        auto &a = v[j], &b = v[(j + 1) % 3];
        if (a.cut && b.cut)
          seams[j] = 1;
        for (size_t zero = 0; zero < 3; zero++)
          if (a.bary[zero] == 0 && b.bary[zero] == 0 && !m.uv_seams.empty())
            seams[j] |= m.uv_seams[t + (zero + 1) % 3];
      }
      append(v, uint32_t(t / 3), materials.empty() ? 0 : materials[t / 3],
             seams);
    }
  }
  if (o.cap) {
    if (cut_positions.size() > o.max_contour_vertices)
      throw std::length_error("Cut contour vertex budget exceeded");
    std::map<Point, std::set<Point>> adjacency;
    std::set<std::array<Point, 2>> links;
    for (auto s : segments) {
      if (s[1] < s[0])
        std::swap(s[0], s[1]);
      if (s[0] == s[1] || !links.insert(s).second)
        throw std::runtime_error("Duplicate or degenerate cut segment");
      adjacency[s[0]].insert(s[1]);
      adjacency[s[1]].insert(s[0]);
    }
    for (auto &[p, neighbors] : adjacency)
      if (neighbors.size() != 2)
        throw std::runtime_error("Cut contour is open or branches");
    Vec3 cap_normal = normal * -1;
    Vec3 u = normalized(cross(
        std::abs(normal.x) < .8f ? Vec3{1, 0, 0} : Vec3{0, 1, 0}, cap_normal));
    Vec3 v = cross(cap_normal, u);
    auto project = [&](Point p) {
      auto x = cut_positions.at(p);
      return XY{dp(x, u), dp(x, v)};
    };
    std::set<Point> visited;
    std::vector<std::vector<Point>> loops;
    std::vector<std::vector<XY>> polygons;
    for (auto &[start, neighbors] : adjacency) {
      stop(o.cancelled);
      if (visited.contains(start))
        continue;
      std::vector<Point> loop;
      Point current = start, previous{};
      bool have_previous = false;
      do {
        if (!visited.insert(current).second)
          throw std::runtime_error("Cut contour revisits a vertex");
        loop.push_back(current);
        auto &nexts = adjacency.at(current);
        auto next = *nexts.begin();
        if (have_previous && next == previous)
          next = *std::next(nexts.begin());
        previous = current;
        current = next;
        have_previous = true;
        if (loop.size() > o.max_contour_vertices)
          throw std::length_error("Cut contour length exceeds limit");
      } while (current != start);
      std::vector<XY> xy;
      for (auto p : loop)
        xy.push_back(project(p));
      double total = 0;
      for (size_t i = 0; i < xy.size(); i++)
        total += xy[i][0] * xy[(i + 1) % xy.size()][1] -
                 xy[i][1] * xy[(i + 1) % xy.size()][0];
      if (total < 0) {
        std::reverse(loop.begin(), loop.end());
        std::reverse(xy.begin(), xy.end());
      }
      ears(xy, o.cancelled);
      loops.push_back(std::move(loop));
      polygons.push_back(std::move(xy));
    }
    for (size_t a = 0; a < polygons.size(); a++)
      for (size_t b = a + 1; b < polygons.size(); b++) {
        stop(o.cancelled);
        if (inside_polygon(polygons[a][0], polygons[b]) ||
            inside_polygon(polygons[b][0], polygons[a]))
          throw std::runtime_error(
              "Cap contours with holes or overlapping shells need a dedicated "
              "polygon-with-holes triangulator");
        for (size_t i = 0; i < polygons[a].size(); i++)
          for (size_t j = 0; j < polygons[b].size(); j++)
            if (crosses(
                    polygons[a][i], polygons[a][(i + 1) % polygons[a].size()],
                    polygons[b][j], polygons[b][(j + 1) % polygons[b].size()]))
              throw std::runtime_error("Intersecting cap contours");
      }
    for (size_t i = 0; i < loops.size(); i++)
      for (auto tri : ears(polygons[i], o.cancelled)) {
        std::array<ClipVertex, 3> vertices;
        std::array<uint8_t, 3> seams{};
        for (size_t k = 0; k < 3; k++) {
          auto p = loops[i][tri[k]], q = loops[i][tri[(k + 1) % 3]];
          vertices[k].p = cut_positions.at(p);
          vertices[k].n = cap_normal;
          vertices[k].cut = true;
          vertices[k].uv = {float(polygons[i][tri[k]][0]),
                            float(polygons[i][tri[k]][1])};
          std::array<Point, 2> link = {std::min(p, q), std::max(p, q)};
          seams[k] = links.contains(link);
        }
        append(vertices, UINT32_MAX, o.cap_material, seams);
        ++out.cap_triangles;
      }
  }
  if (out.mesh.indices.empty())
    throw std::runtime_error("Plane cut leaves no surface");
  out.mesh.validate();
  if (o.cap && !inspect_mesh(out.mesh, o.cancelled).closed_manifold())
    throw std::runtime_error("Cut result failed closed manifold verification");
  stop(o.cancelled);
  return out;
}
} // namespace dome
