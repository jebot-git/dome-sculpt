// SPDX-License-Identifier: GPL-3.0-or-later
// Native stable-ID polygon topology. Wings 3D's never-reused identities,
// per-side attributes, region boundaries and validate-before-accept discipline
// inform this implementation; no Erlang runtime/data structure is embedded.
// Reference copyright (c) 2001-2011 Bjorn Gustavsson and other parties;
// their permissive notice is retained in third_party/wings/license.terms.
#include "dome/polygon.hpp"
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <numeric>
#include <set>
namespace dome {
namespace {
using Id = PolygonId;
using Key = std::pair<Id, Id>;
Id archive_id(const nlohmann::json &v) {
  if (!v.is_number_unsigned() &&
      (!v.is_number_integer() || v.get<int64_t>() < 0))
    throw std::invalid_argument("Invalid unsigned polygon archive identity");
  return v.get<Id>();
}
Key key(Id a, Id b) { return std::minmax(a, b); }
struct Budget {
  const PolygonLimits &limits;
  size_t work = 0;
  void step(size_t n = 1) {
    if (n > limits.maximum_work - work)
      throw std::length_error("Polygon operation exceeds its work budget");
    work += n;
    if (limits.cancelled && limits.cancelled())
      throw std::runtime_error("Polygon operation cancelled");
  }
};
struct D3 {
  double x = 0, y = 0, z = 0;
  D3 operator+(D3 b) const { return {x + b.x, y + b.y, z + b.z}; }
  D3 operator-(D3 b) const { return {x - b.x, y - b.y, z - b.z}; }
  D3 operator*(double s) const { return {x * s, y * s, z * s}; }
};
D3 d3(Vec3 p) { return {p.x, p.y, p.z}; }
double dot3(D3 a, D3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
D3 cross3(D3 a, D3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double norm(D3 p) { return std::sqrt(dot3(p, p)); }
Vec3 f3(D3 p) { return {float(p.x), float(p.y), float(p.z)}; }
struct P2 {
  double x = 0, y = 0;
  P2 operator+(P2 b) const { return {x + b.x, y + b.y}; }
  P2 operator-(P2 b) const { return {x - b.x, y - b.y}; }
  P2 operator*(double s) const { return {x * s, y * s}; }
};
double cross2(P2 a, P2 b) { return a.x * b.y - a.y * b.x; }
double orient(P2 a, P2 b, P2 c) { return cross2(b - a, c - a); }
bool on(P2 a, P2 b, P2 p) {
  return orient(a, b, p) == 0 && p.x >= std::min(a.x, b.x) &&
         p.x <= std::max(a.x, b.x) && p.y >= std::min(a.y, b.y) &&
         p.y <= std::max(a.y, b.y);
}
bool intersects(P2 a, P2 b, P2 c, P2 d) {
  double p = orient(a, b, c), q = orient(a, b, d), r = orient(c, d, a),
         s = orient(c, d, b);
  return (((p < 0 && q > 0) || (p > 0 && q < 0)) &&
          ((r < 0 && s > 0) || (r > 0 && s < 0))) ||
         on(a, b, c) || on(a, b, d) || on(c, d, a) || on(c, d, b);
}
struct FaceGeometry {
  std::vector<D3> xyz;
  std::vector<P2> xy;
  D3 normal;
  int axis = 0;
  double sign = 1;
  std::vector<std::array<size_t, 3>> triangles;
};
using Vertices = std::map<Id, const PolygonVertex *>;
Vertices index(const PolygonMesh &m, Budget &b) {
  Vertices out;
  for (auto &v : m.vertices()) {
    b.step();
    if (!out.emplace(v.id, &v).second)
      throw std::invalid_argument("Duplicate polygon vertex");
  }
  return out;
}
FaceGeometry geometry(const PolygonFace &f, const Vertices &vertices,
                      Budget &b) {
  FaceGeometry g;
  for (auto &c : f.corners) {
    b.step();
    auto i = vertices.find(c.vertex);
    if (i == vertices.end())
      throw std::invalid_argument("Polygon corner refers to a missing vertex");
    g.xyz.push_back(d3(i->second->position));
  }
  auto origin = g.xyz.front();
  for (size_t i = 1; i + 1 < g.xyz.size(); ++i)
    g.normal = g.normal + cross3(g.xyz[i] - origin, g.xyz[i + 1] - origin);
  if (norm(g.normal) == 0)
    throw std::invalid_argument("Polygon face has zero area");
  g.axis = std::abs(g.normal.x) > std::abs(g.normal.y) ? 0 : 1;
  if (std::abs(g.normal.z) >
      (g.axis == 0 ? std::abs(g.normal.x) : std::abs(g.normal.y)))
    g.axis = 2;
  for (auto p : g.xyz) {
    p = p - origin;
    g.xy.push_back(g.axis == 0   ? P2{p.y, p.z}
                   : g.axis == 1 ? P2{p.z, p.x}
                                 : P2{p.x, p.y});
  }
  double area = 0;
  for (size_t i = 0; i < g.xy.size(); ++i)
    area += cross2(g.xy[i], g.xy[(i + 1) % g.xy.size()]);
  if (area == 0)
    throw std::invalid_argument("Polygon face has a singular projection");
  g.sign = area > 0 ? 1 : -1;
  const size_t n = g.xy.size();
  for (size_t i = 0; i < n; ++i) {
    auto a = g.xy[i], z = g.xy[(i + 1) % n];
    if (a.x == z.x && a.y == z.y)
      throw std::invalid_argument("Polygon face has a zero projected edge");
    for (size_t j = i + 1; j < n; ++j) {
      b.step();
      if (j == i + 1 || (i == 0 && j == n - 1))
        continue;
      if (intersects(a, z, g.xy[j], g.xy[(j + 1) % n]))
        throw std::invalid_argument("Polygon face intersects itself");
    }
  }
  std::vector<size_t> ring(n);
  std::iota(ring.begin(), ring.end(), 0);
  while (ring.size() > 3) {
    bool found = false;
    for (size_t at = 0; at < ring.size(); ++at) {
      size_t a = ring[(at + ring.size() - 1) % ring.size()], c = ring[at],
             d = ring[(at + 1) % ring.size()];
      b.step();
      if (g.sign * orient(g.xy[a], g.xy[c], g.xy[d]) <= 0)
        continue;
      bool contains = false;
      for (auto v : ring) {
        b.step();
        if (v == a || v == c || v == d)
          continue;
        if (g.sign * orient(g.xy[a], g.xy[c], g.xy[v]) >= 0 &&
            g.sign * orient(g.xy[c], g.xy[d], g.xy[v]) >= 0 &&
            g.sign * orient(g.xy[d], g.xy[a], g.xy[v]) >= 0) {
          contains = true;
          break;
        }
      }
      if (contains)
        continue;
      g.triangles.push_back({a, c, d});
      ring.erase(ring.begin() + at);
      found = true;
      break;
    }
    if (!found)
      throw std::invalid_argument(
          "Polygon face cannot be triangulated without losing a corner");
  }
  g.triangles.push_back({ring[0], ring[1], ring[2]});
  for (auto t : g.triangles) {
    b.step();
    if (dot3(cross3(g.xyz[t[1]] - g.xyz[t[0]], g.xyz[t[2]] - g.xyz[t[0]]),
             g.normal) <= 0)
      throw std::invalid_argument(
          "Polygon face folds across its triangulation");
  }
  return g;
}
std::vector<std::pair<Id, double>>
blend(const std::vector<std::pair<const PolygonVertex *, double>> &parts,
      Budget &b) {
  std::map<Id, double> weights;
  for (auto [v, w] : parts)
    for (auto [id, a] : v->provenance) {
      b.step();
      weights[id] += w * a;
    }
  double sum = 0;
  for (auto [id, w] : weights)
    if (w > 0)
      sum += w;
  if (!(sum > 0))
    throw std::invalid_argument("Polygon interpolation has no source identity");
  std::vector<std::pair<Id, double>> out;
  for (auto [id, w] : weights)
    if (w > 0)
      out.push_back({id, w / sum});
  if (out.size() > b.limits.maximum_provenance)
    throw std::length_error(
        "Polygon source interpolation exceeds its identity budget");
  return out;
}
} // namespace
struct PolygonKernel {
  PolygonMesh mesh;
  Budget &budget;
  std::set<Key> propagated_hard{};
  bool trace_corners = false;
  size_t source_entries = 0;
  std::set<Id> input_corners;
  std::map<Id, std::vector<std::pair<Id, double>>> corner_sources;
  static PolygonAttributeEditResult
  edit(const PolygonMesh &, const PolygonEdit &, const PolygonLimits &, bool);
  void record_corner(Id id, std::span<const std::pair<Id, double>> parts) {
    if (!trace_corners)
      return;
    std::map<Id, double> merged;
    for (auto [corner, weight] : parts) {
      budget.step();
      if (!std::isfinite(weight) || weight < 0)
        throw std::invalid_argument(
            "Invalid polygon corner interpolation weight");
      if (weight == 0)
        continue;
      auto add = [&](Id source, double value) {
        budget.step();
        merged[source] += weight * value;
        if (merged.size() > budget.limits.maximum_provenance)
          throw std::length_error(
              "Polygon corner interpolation exceeds its identity budget");
      };
      if (auto found = corner_sources.find(corner);
          found != corner_sources.end())
        for (auto [source, value] : found->second)
          add(source, value);
      else {
        if (!input_corners.contains(corner))
          throw std::invalid_argument(
              "Polygon corner interpolation has no input witness");
        add(corner, 1);
      }
    }
    double sum = 0;
    for (auto [source, weight] : merged)
      sum += weight;
    if (!std::isfinite(sum) || !(sum > 0))
      throw std::invalid_argument("Polygon corner interpolation is empty");
    std::vector<std::pair<Id, double>> weights;
    for (auto [source, weight] : merged)
      if (weight > 0)
        weights.emplace_back(source, weight / sum);
    auto previous = corner_sources.find(id);
    size_t old = previous == corner_sources.end() ? 0 : previous->second.size();
    if (weights.size() > 8'000'000 - (source_entries - old))
      throw std::length_error(
          "Polygon corner interpolation storage exceeds its budget");
    const auto count = weights.size();
    corner_sources.insert_or_assign(id, std::move(weights));
    source_entries = source_entries - old + count;
  }
  void expansion(size_t vertices, size_t faces, size_t corners) {
    size_t existing = 0;
    for (auto &f : mesh.faces_) {
      budget.step();
      existing += f.corners.size();
    }
    if (vertices > budget.limits.maximum_vertices - mesh.vertices_.size() ||
        faces > budget.limits.maximum_faces - mesh.faces_.size() ||
        corners > budget.limits.maximum_corners - existing)
      throw std::length_error("Polygon expansion exceeds its component budget");
  }
  Id fresh() {
    if (mesh.next_ == UINT64_MAX)
      throw std::overflow_error("Polygon component identities exhausted");
    return mesh.next_++;
  }
  PolygonCorner
  corner(Id v, std::array<float, 2> uv, bool generated,
         std::initializer_list<std::pair<Id, double>> sources = {}) {
    auto id = fresh();
    record_corner(id, sources);
    return {id, v, uv, generated};
  }
  std::map<Id, size_t> vertex_index, face_index, edge_index;
  PolygonKernel(PolygonMesh value, Budget &b, bool attributes = false)
      : mesh(std::move(value)), budget(b), trace_corners(attributes) {
    if (trace_corners)
      for (const auto &face : mesh.faces_)
        for (const auto &corner : face.corners) {
          budget.step();
          input_corners.insert(corner.id);
        }
    refresh_vertices();
    refresh_faces();
    refresh_edges();
  }
  void refresh_vertices() {
    vertex_index.clear();
    for (size_t i = 0; i < mesh.vertices_.size(); ++i) {
      budget.step();
      vertex_index.emplace(mesh.vertices_[i].id, i);
    }
  }
  void refresh_faces() {
    face_index.clear();
    for (size_t i = 0; i < mesh.faces_.size(); ++i) {
      budget.step();
      face_index.emplace(mesh.faces_[i].id, i);
    }
  }
  void refresh_edges() {
    edge_index.clear();
    for (size_t i = 0; i < mesh.edges_.size(); ++i) {
      budget.step();
      edge_index.emplace(mesh.edges_[i].id, i);
    }
  }
  void append_vertex(PolygonVertex v) {
    budget.step();
    auto id = v.id;
    auto i = mesh.vertices_.size();
    mesh.vertices_.push_back(std::move(v));
    vertex_index.emplace(id, i);
  }
  void append_face(PolygonFace f) {
    budget.step();
    auto id = f.id;
    auto i = mesh.faces_.size();
    mesh.faces_.push_back(std::move(f));
    face_index.emplace(id, i);
  }
  PolygonVertex &vertex(Id id) {
    budget.step();
    auto i = vertex_index.find(id);
    if (i == vertex_index.end())
      throw std::invalid_argument("Missing selected polygon vertex");
    return mesh.vertices_[i->second];
  }
  PolygonFace &face(Id id) {
    budget.step();
    auto i = face_index.find(id);
    if (i == face_index.end())
      throw std::invalid_argument("Missing selected polygon face");
    return mesh.faces_[i->second];
  }
  PolygonEdge edge(Id id) {
    budget.step();
    auto i = edge_index.find(id);
    if (i == edge_index.end())
      throw std::invalid_argument("Missing selected polygon edge");
    return mesh.edges_[i->second];
  }
  void rebuild() {
    std::map<Key, PolygonEdge> old;
    for (auto &e : mesh.edges_) {
      budget.step();
      old.emplace(key(e.a, e.b), e);
    }
    std::set<Key> keys;
    for (auto &f : mesh.faces_)
      for (size_t i = 0; i < f.corners.size(); ++i) {
        budget.step();
        keys.insert(key(f.corners[i].vertex,
                        f.corners[(i + 1) % f.corners.size()].vertex));
      }
    std::vector<PolygonEdge> out;
    for (auto k : keys) {
      budget.step();
      auto i = old.find(k);
      out.push_back(i != old.end()
                        ? i->second
                        : PolygonEdge{fresh(), k.first, k.second, false});
    }
    mesh.edges_ = std::move(out);
    refresh_edges();
  }
  void prune() {
    std::set<Id> used;
    for (auto &f : mesh.faces_)
      for (auto &c : f.corners) {
        budget.step();
        used.insert(c.vertex);
      }
    std::erase_if(mesh.vertices_,
                  [&](auto &v) { return !used.contains(v.id); });
    refresh_vertices();
    refresh_faces();
  }
  static void validate(const PolygonMesh &m, Budget &b) {
    if (m.vertices_.size() > b.limits.maximum_vertices ||
        m.faces_.size() > b.limits.maximum_faces)
      throw std::length_error("Polygon component budget exceeded");
    if (!m.next_)
      throw std::invalid_argument("Invalid next polygon identity");
    std::set<Id> ids;
    auto id = [&](Id v) {
      b.step();
      if (!v || v >= m.next_ || !ids.insert(v).second)
        throw std::invalid_argument(
            "Invalid or reused polygon component identity");
    };
    for (auto &v : m.vertices_) {
      id(v.id);
      if (!finite(v.position) || v.provenance.empty() ||
          v.provenance.size() > b.limits.maximum_provenance)
        throw std::invalid_argument(
            "Invalid polygon vertex or source identity");
      double sum = 0;
      std::set<Id> roots;
      for (auto [r, w] : v.provenance) {
        b.step();
        if (!r || r >= m.next_ || !std::isfinite(w) || w <= 0 ||
            !roots.insert(r).second)
          throw std::invalid_argument("Invalid polygon source weight");
        sum += w;
      }
      if (std::abs(sum - 1) > 1e-9)
        throw std::invalid_argument(
            "Polygon source weights are not normalized");
    }
    auto vertices = index(m, b);
    std::map<Key, std::vector<Key>> incidence;
    std::map<Id, std::map<Id, std::set<Id>>> links;
    std::set<std::vector<Id>> face_keys;
    size_t corners = 0;
    for (auto &f : m.faces_) {
      id(f.id);
      if (f.corners.size() < 3 ||
          f.corners.size() > b.limits.maximum_face_corners ||
          f.corners.size() > b.limits.maximum_corners - corners)
        throw std::length_error("Polygon corner budget exceeded");
      corners += f.corners.size();
      std::set<Id> vs;
      std::vector<Id> fk;
      for (size_t i = 0; i < f.corners.size(); ++i) {
        auto &c = f.corners[i];
        id(c.id);
        if (!std::isfinite(c.uv[0]) || !std::isfinite(c.uv[1]) ||
            !vs.insert(c.vertex).second)
          throw std::invalid_argument(
              "Invalid or repeated polygon face corner");
        fk.push_back(c.vertex);
        auto next = f.corners[(i + 1) % f.corners.size()].vertex,
             prev = f.corners[(i + f.corners.size() - 1) % f.corners.size()]
                        .vertex;
        auto &uses = incidence[key(c.vertex, next)];
        uses.push_back({c.vertex, next});
        if (uses.size() > 2 || (uses.size() == 2 && uses[0] == uses[1]))
          throw std::invalid_argument(
              "Polygon edges must be oriented manifold boundaries");
        links[c.vertex][prev].insert(next);
        links[c.vertex][next].insert(prev);
      }
      std::sort(fk.begin(), fk.end());
      if (!face_keys.insert(fk).second)
        throw std::invalid_argument("Duplicate polygon face");
      geometry(f, vertices, b);
    }
    std::set<Key> edge_keys;
    for (auto &e : m.edges_) {
      id(e.id);
      if (e.a >= e.b || !incidence.contains({e.a, e.b}) ||
          !edge_keys.insert({e.a, e.b}).second)
        throw std::invalid_argument("Invalid polygon edge authority");
    }
    if (edge_keys.size() != incidence.size())
      throw std::invalid_argument("Polygon edge authority is incomplete");
    for (auto &[v, graph] : links) {
      size_t ends = 0;
      for (auto &[n, adj] : graph) {
        b.step();
        if (adj.size() == 1)
          ++ends;
        else if (adj.size() != 2)
          throw std::invalid_argument("Polygon vertex has a nonmanifold link");
      }
      if (ends != 0 && ends != 2)
        throw std::invalid_argument("Polygon vertex has a branching boundary");
      std::set<Id> seen;
      std::vector<Id> pending{graph.begin()->first};
      while (!pending.empty()) {
        b.step();
        auto n = pending.back();
        pending.pop_back();
        if (seen.insert(n).second)
          for (auto other : graph.at(n))
            pending.push_back(other);
      }
      if (seen.size() != graph.size())
        throw std::invalid_argument(
            "Polygon vertex joins disconnected face fans");
    }
  }
  std::set<Id> selected(const PolygonSelection &s) {
    std::set<Id> ids;
    for (auto id : s.ids) {
      budget.step();
      if (s.mode == PolygonSelectionMode::Vertex)
        vertex(id);
      else if (s.mode == PolygonSelectionMode::Edge)
        edge(id);
      else
        face(id);
      ids.insert(id);
    }
    return ids;
  }
  std::set<Id> selected_vertices(const PolygonSelection &s,
                                 const std::set<Id> &ids) {
    if (s.mode == PolygonSelectionMode::Vertex)
      return ids;
    std::set<Id> out;
    for (auto id : ids) {
      if (s.mode == PolygonSelectionMode::Edge) {
        auto e = edge(id);
        out.insert(e.a);
        out.insert(e.b);
      } else
        for (auto &c : face(id).corners) {
          budget.step();
          out.insert(c.vertex);
        }
    }
    return out;
  }
  void extrude(const std::set<Id> &ids, const PolygonEdit &edit) {
    std::set<Id> source_vertices;
    for (auto id : ids)
      for (auto &c : face(id).corners) {
        budget.step();
        source_vertices.insert(c.vertex);
      }
    if (source_vertices.size() >
        budget.limits.maximum_vertices - mesh.vertices_.size())
      throw std::length_error("Extrusion exceeds its vertex budget");
    mesh.vertices_.reserve(mesh.vertices_.size() + source_vertices.size());
    auto vertices = index(mesh, budget);
    std::map<Key, std::vector<Id>> uses;
    std::map<Id, std::set<Id>> adjacent;
    for (auto id : ids) {
      auto &f = face(id);
      for (size_t i = 0; i < f.corners.size(); ++i) {
        budget.step();
        uses[key(f.corners[i].vertex,
                 f.corners[(i + 1) % f.corners.size()].vertex)]
            .push_back(id);
      }
    }
    for (auto &[k, fs] : uses)
      if (fs.size() == 2) {
        adjacent[fs[0]].insert(fs[1]);
        adjacent[fs[1]].insert(fs[0]);
      }
    size_t outer_edges = 0;
    for (auto &[k, fs] : uses)
      if (fs.size() == 1)
        ++outer_edges;
    if (outer_edges > SIZE_MAX / 4)
      throw std::length_error("Polygon extrusion size overflow");
    expansion(source_vertices.size(), outer_edges, 4 * outer_edges);
    std::set<Id> remaining = ids;
    std::map<Id, Id> copied;
    std::vector<PolygonFace> sides;
    while (!remaining.empty()) {
      std::set<Id> region;
      std::vector<Id> pending{*remaining.begin()};
      D3 normal;
      while (!pending.empty()) {
        budget.step();
        auto id = pending.back();
        pending.pop_back();
        if (!remaining.erase(id))
          continue;
        region.insert(id);
        normal = normal + geometry(face(id), vertices, budget).normal;
        for (auto n : adjacent[id])
          pending.push_back(n);
      }
      bool boundary = false;
      for (auto &[k, fs] : uses)
        if (fs.size() == 1 && region.contains(fs[0]))
          boundary = true;
      if (!boundary)
        throw std::invalid_argument(
            "Extrusion requires an open selected-region boundary");
      D3 delta = d3(edit.delta);
      if (norm(delta) == 0) {
        if (norm(normal) == 0)
          throw std::invalid_argument(
              "Selected extrusion region has no common direction");
        delta = normal * (double(edit.amount) / norm(normal));
      }
      std::set<Id> vs;
      for (auto id : region)
        for (auto &c : face(id).corners)
          vs.insert(c.vertex);
      for (auto id : vs) {
        budget.step();
        if (copied.contains(id))
          throw std::invalid_argument(
              "Extrusion regions touch at a nonmanifold vertex");
        auto v = *vertices.at(id);
        v.id = fresh();
        v.position = f3(d3(v.position) + delta);
        copied[id] = v.id;
        append_vertex(std::move(v));
      }
      for (auto id : region) {
        auto &f = face(id);
        for (size_t i = 0; i < f.corners.size(); ++i) {
          auto a = f.corners[i].vertex,
               z = f.corners[(i + 1) % f.corners.size()].vertex;
          if (uses.at(key(a, z)).size() == 1) {
            PolygonFace side;
            side.id = fresh();
            side.material = f.material;
            auto ca = f.corners[i].id,
                 cz = f.corners[(i + 1) % f.corners.size()].id;
            side.corners = {corner(a, {0, 0}, true, {{ca, 1}}),
                            corner(z, {1, 0}, true, {{cz, 1}}),
                            corner(copied[z], {1, 1}, true, {{cz, 1}}),
                            corner(copied[a], {0, 1}, true, {{ca, 1}})};
            sides.push_back(std::move(side));
          }
        }
      }
    }
    for (auto &e : mesh.edges_)
      if (e.hard && copied.contains(e.a) && copied.contains(e.b))
        propagated_hard.insert(key(copied.at(e.a), copied.at(e.b)));
    for (auto id : ids)
      for (auto &c : face(id).corners)
        c.vertex = copied.at(c.vertex);
    for (auto &side : sides)
      append_face(std::move(side));
    prune();
  }
  void inset(const std::set<Id> &ids, float amount) {
    size_t created = 0;
    for (auto id : ids)
      created += face(id).corners.size();
    if (created > SIZE_MAX / 4)
      throw std::length_error("Polygon inset size overflow");
    expansion(created, created, 4 * created);
    for (auto id : ids) {
      auto original = face(id);
      if (original.corners.size() >
          budget.limits.maximum_vertices - mesh.vertices_.size())
        throw std::length_error("Inset exceeds its vertex budget");
      mesh.vertices_.reserve(mesh.vertices_.size() + original.corners.size());
      auto vertices = index(mesh, budget);
      auto g = geometry(original, vertices, budget);
      D3 n = g.normal * (1 / norm(g.normal));
      double extent = 0;
      for (auto p : g.xyz)
        extent = std::max(extent, norm(p - g.xyz[0]));
      for (auto p : g.xyz)
        if (std::abs(dot3(p - g.xyz[0], n)) > extent * 1e-6)
          throw std::invalid_argument("Inset currently requires a planar face");
      for (size_t i = 0; i < g.xy.size(); ++i)
        if (g.sign * orient(g.xy[(i + g.xy.size() - 1) % g.xy.size()], g.xy[i],
                            g.xy[(i + 1) % g.xy.size()]) <=
            0)
          throw std::invalid_argument("Inset currently requires a convex face");
      std::vector<PolygonCorner> inner;
      std::vector<PolygonFace> sides;
      for (size_t i = 0; i < g.xyz.size(); ++i) {
        budget.step();
        auto prev = g.xyz[(i + g.xyz.size() - 1) % g.xyz.size()], p = g.xyz[i],
             next = g.xyz[(i + 1) % g.xyz.size()];
        D3 e0 = p - prev, e1 = next - p;
        e0 = e0 * (1 / norm(e0));
        e1 = e1 * (1 / norm(e1));
        D3 inward0 = cross3(n, e0), inward1 = cross3(n, e1),
           bisector = inward0 + inward1;
        double divisor = dot3(bisector, inward0);
        if (!(divisor > 0))
          throw std::invalid_argument("Inset has a singular corner");
        D3 q = p + bisector * (double(amount) / divisor);
        P2 qp;
        auto d = q - g.xyz[0];
        qp = g.axis == 0   ? P2{d.y, d.z}
             : g.axis == 1 ? P2{d.z, d.x}
                           : P2{d.x, d.y};
        bool found = false;
        PolygonVertex nv;
        nv.id = fresh();
        nv.position = f3(q);
        std::array<float, 2> uv{};
        std::array<std::pair<Id, double>, 3> color_sources{};
        for (auto t : g.triangles) {
          budget.step();
          auto a = g.xy[t[0]], b = g.xy[t[1]], c = g.xy[t[2]];
          double den = orient(a, b, c), w0 = orient(b, c, qp) / den,
                 w1 = orient(c, a, qp) / den, w2 = 1 - w0 - w1;
          if (w0 < 0 || w1 < 0 || w2 < 0)
            continue;
          nv.provenance =
              blend({{vertices.at(original.corners[t[0]].vertex), w0},
                     {vertices.at(original.corners[t[1]].vertex), w1},
                     {vertices.at(original.corners[t[2]].vertex), w2}},
                    budget);
          for (size_t k = 0; k < 2; ++k)
            uv[k] = float(w0 * original.corners[t[0]].uv[k] +
                          w1 * original.corners[t[1]].uv[k] +
                          w2 * original.corners[t[2]].uv[k]);
          color_sources = {{{original.corners[t[0]].id, w0},
                            {original.corners[t[1]].id, w1},
                            {original.corners[t[2]].id, w2}}};
          found = true;
          break;
        }
        if (!found)
          throw std::invalid_argument(
              "Inset distance leaves the original face");
        append_vertex(std::move(nv));
        auto c = corner(mesh.vertices_.back().id, uv,
                        original.corners[i].generated_uv,
                        {{color_sources[0].first, color_sources[0].second},
                         {color_sources[1].first, color_sources[1].second},
                         {color_sources[2].first, color_sources[2].second}});
        inner.push_back(std::move(c));
      }
      // An excessive inset can cross opposing edges while every point remains
      // inside. Positive corner orientation and reduced positive area are
      // required.
      PolygonFace top = original;
      top.corners = inner;
      auto new_vertices = index(mesh, budget);
      auto top_geometry = geometry(top, new_vertices, budget);
      if (dot3(top_geometry.normal, g.normal) <= 0 ||
          norm(top_geometry.normal) >= norm(g.normal))
        throw std::invalid_argument("Inset collapses or reverses the face");
      for (size_t i = 0; i < inner.size(); ++i) {
        auto j = (i + 1) % inner.size();
        PolygonFace side;
        side.id = fresh();
        side.material = original.material;
        side.corners = {
            corner(original.corners[i].vertex, original.corners[i].uv,
                   original.corners[i].generated_uv,
                   {{original.corners[i].id, 1}}),
            corner(original.corners[j].vertex, original.corners[j].uv,
                   original.corners[j].generated_uv,
                   {{original.corners[j].id, 1}}),
            corner(inner[j].vertex, inner[j].uv, inner[j].generated_uv,
                   {{inner[j].id, 1}}),
            corner(inner[i].vertex, inner[i].uv, inner[i].generated_uv,
                   {{inner[i].id, 1}})};
        sides.push_back(std::move(side));
      }
      face(id) = std::move(top);
      for (auto &side : sides)
        append_face(std::move(side));
    }
  }
  void bridge(const std::set<Id> &ids) {
    if (ids.size() != 2)
      throw std::invalid_argument("Bridge needs exactly two faces");
    auto a = face(*ids.begin()), z = face(*ids.rbegin());
    if (a.corners.size() != z.corners.size())
      throw std::invalid_argument("Bridge faces must have equal corner counts");
    const auto n = a.corners.size();
    expansion(0, n - 2, 2 * n);
    std::set<Id> av;
    for (auto &c : a.corners)
      av.insert(c.vertex);
    for (auto &c : z.corners)
      if (av.contains(c.vertex))
        throw std::invalid_argument("Bridge faces must not share vertices");
    double best = std::numeric_limits<double>::infinity();
    size_t offset = 0;
    for (size_t shift = 0; shift < n; ++shift) {
      double score = 0;
      for (size_t i = 0; i < n; ++i) {
        budget.step();
        auto d = d3(vertex(a.corners[i].vertex).position) -
                 d3(vertex(z.corners[(shift + n - i) % n].vertex).position);
        score += dot3(d, d);
      }
      if (score < best) {
        best = score;
        offset = shift;
      }
    }
    std::erase_if(mesh.faces_, [&](auto &f) { return ids.contains(f.id); });
    refresh_faces();
    for (size_t i = 0; i < n; ++i) {
      budget.step();
      auto j = (i + 1) % n;
      PolygonFace f;
      f.id = fresh();
      f.material = a.material;
      f.corners = {
          corner(a.corners[i].vertex, {0, 0}, true, {{a.corners[i].id, 1}}),
          corner(a.corners[j].vertex, {1, 0}, true, {{a.corners[j].id, 1}}),
          corner(z.corners[(offset + n - j) % n].vertex, {1, 1}, true,
                 {{z.corners[(offset + n - j) % n].id, 1}}),
          corner(z.corners[(offset + n - i) % n].vertex, {0, 1}, true,
                 {{z.corners[(offset + n - i) % n].id, 1}})};
      append_face(std::move(f));
    }
  }
  void dissolve(Id eid) {
    auto e = edge(eid);
    if (e.hard)
      throw std::invalid_argument(
          "Dissolving a hard edge requires explicit conversion");
    std::vector<PolygonFace> fs;
    for (auto &f : mesh.faces_)
      for (size_t i = 0; i < f.corners.size(); ++i) {
        budget.step();
        if (key(f.corners[i].vertex,
                f.corners[(i + 1) % f.corners.size()].vertex) ==
            key(e.a, e.b)) {
          fs.push_back(f);
          break;
        }
      }
    if (fs.size() != 2)
      throw std::invalid_argument("Dissolve needs an interior edge");
    if (fs[0].material != fs[1].material)
      throw std::invalid_argument(
          "Dissolve would merge distinct face materials");
    std::map<Id, PolygonCorner> boundary;
    std::map<Id, Id> next;
    for (auto &f : fs)
      for (size_t i = 0; i < f.corners.size(); ++i) {
        auto &c = f.corners[i];
        auto v = f.corners[(i + 1) % f.corners.size()].vertex;
        if (key(c.vertex, v) == key(e.a, e.b))
          continue;
        if (!boundary.emplace(c.vertex, c).second)
          throw std::invalid_argument("Dissolve has a branching boundary");
        next[c.vertex] = v;
      }
    for (auto v : {e.a, e.b}) {
      std::array<float, 2> uv{};
      bool set = false;
      for (auto &f : fs)
        for (auto &c : f.corners)
          if (c.vertex == v) {
            if (set && uv != c.uv)
              throw std::invalid_argument("Dissolve would discard a UV seam");
            uv = c.uv;
            set = true;
          }
    }
    PolygonFace merged;
    merged.id = fresh();
    merged.material = fs[0].material;
    Id first = boundary.begin()->first, at = first;
    do {
      budget.step();
      auto i = boundary.find(at);
      if (i == boundary.end())
        throw std::invalid_argument("Dissolve boundary is incomplete");
      merged.corners.push_back(i->second);
      boundary.erase(i);
      at = next.at(at);
    } while (at != first);
    if (!boundary.empty())
      throw std::invalid_argument("Dissolve would create a face with holes");
    std::erase_if(mesh.faces_, [&](auto &f) {
      return f.id == fs[0].id || f.id == fs[1].id;
    });
    refresh_faces();
    append_face(std::move(merged));
    rebuild();
  }
  void weld(const std::set<Id> &ids) {
    Id target = *ids.begin();
    D3 position;
    std::vector<std::pair<const PolygonVertex *, double>> sources;
    for (auto id : ids) {
      budget.step();
      auto &v = vertex(id);
      position = position + d3(v.position);
      sources.push_back({&v, 1.0 / ids.size()});
    }
    auto provenance = blend(sources, budget);
    auto &v = vertex(target);
    v.position = f3(position * (1.0 / ids.size()));
    v.provenance = std::move(provenance);
    for (auto &f : mesh.faces_) {
      std::vector<PolygonCorner> out;
      std::map<Id, std::vector<Id>> groups;
      for (auto c : f.corners) {
        budget.step();
        if (ids.contains(c.vertex))
          c.vertex = target;
        if (out.empty() || out.back().vertex != c.vertex) {
          out.push_back(c);
          if (trace_corners)
            groups[c.id].push_back(c.id);
        } else if (trace_corners)
          groups[out.back().id].push_back(c.id);
      }
      if (out.size() > 1 && out.front().vertex == out.back().vertex) {
        if (trace_corners) {
          auto &front = groups.at(out.front().id);
          auto &back = groups.at(out.back().id);
          front.insert(front.end(), back.begin(), back.end());
        }
        out.pop_back();
      }
      if (trace_corners)
        for (const auto &c : out) {
          const auto &group = groups.at(c.id);
          if (group.size() > 1) {
            std::vector<std::pair<Id, double>> weights;
            for (auto id : group)
              weights.emplace_back(id, 1.0 / group.size());
            record_corner(c.id, weights);
          }
        }
      f.corners = std::move(out);
    }
    std::erase_if(mesh.faces_, [](auto &f) { return f.corners.size() < 3; });
    std::erase_if(mesh.vertices_, [&](auto &x) {
      return x.id != target && ids.contains(x.id);
    });
    refresh_vertices();
    refresh_faces();
  }
  void cut(const std::set<Id> &ids, unsigned cuts) {
    if (cuts < 1 || cuts > 64)
      throw std::invalid_argument("Edge cuts must be between one and 64");
    if (ids.size() >
        (budget.limits.maximum_vertices - mesh.vertices_.size()) / cuts)
      throw std::length_error("Polygon cuts exceed their vertex budget");
    std::set<Key> selected_edges;
    for (auto id : ids) {
      auto e = edge(id);
      selected_edges.insert(key(e.a, e.b));
    }
    size_t extra_corners = 0;
    for (auto &f : mesh.faces_) {
      size_t extra = 0;
      for (size_t i = 0; i < f.corners.size(); ++i) {
        budget.step();
        if (selected_edges.contains(
                key(f.corners[i].vertex,
                    f.corners[(i + 1) % f.corners.size()].vertex)))
          extra += cuts;
      }
      if (extra > budget.limits.maximum_face_corners - f.corners.size())
        throw std::length_error("Polygon cuts exceed their face-corner budget");
      extra_corners += extra;
    }
    expansion(ids.size() * cuts, 0, extra_corners);
    std::map<Key, std::vector<Id>> inserted;
    for (auto id : ids) {
      auto e = edge(id);
      auto a = vertex(e.a), z = vertex(e.b);
      std::vector<Id> vs;
      for (unsigned j = 1; j <= cuts; ++j) {
        budget.step();
        double t = double(j) / (cuts + 1);
        PolygonVertex v;
        v.id = fresh();
        v.position = f3(d3(a.position) * (1 - t) + d3(z.position) * t);
        v.provenance = blend({{&a, 1 - t}, {&z, t}}, budget);
        vs.push_back(v.id);
        append_vertex(std::move(v));
      }
      if (e.hard) {
        Id prev = e.a;
        for (auto v : vs) {
          propagated_hard.insert(key(prev, v));
          prev = v;
        }
        propagated_hard.insert(key(prev, e.b));
      }
      inserted.emplace(key(e.a, e.b), std::move(vs));
    }
    for (auto &f : mesh.faces_) {
      std::vector<PolygonCorner> out;
      for (size_t i = 0; i < f.corners.size(); ++i) {
        auto c = f.corners[i], z = f.corners[(i + 1) % f.corners.size()];
        out.push_back(c);
        auto it = inserted.find(key(c.vertex, z.vertex));
        if (it == inserted.end())
          continue;
        for (unsigned j = 1; j <= cuts; ++j) {
          budget.step();
          float t = float(j) / float(cuts + 1);
          Id v = it->second[c.vertex < z.vertex ? j - 1 : cuts - j];
          out.push_back(corner(v,
                               {c.uv[0] * (1 - t) + z.uv[0] * t,
                                c.uv[1] * (1 - t) + z.uv[1] * t},
                               c.generated_uv || z.generated_uv,
                               {{c.id, 1 - double(j) / double(cuts + 1)},
                                {z.id, double(j) / double(cuts + 1)}}));
        }
      }
      f.corners = std::move(out);
    }
  }
  void connect(const std::set<Id> &ids) {
    expansion(0, 1, 2);
    if (ids.size() != 2)
      throw std::invalid_argument("Connect needs two vertices");
    Id a = *ids.begin(), z = *ids.rbegin();
    std::vector<Id> candidates;
    for (auto &f : mesh.faces_) {
      bool x = false, y = false;
      for (auto &c : f.corners) {
        budget.step();
        x |= c.vertex == a;
        y |= c.vertex == z;
      }
      if (x && y)
        candidates.push_back(f.id);
    }
    if (candidates.size() != 1)
      throw std::invalid_argument(
          "Connected vertices must share exactly one face");
    auto old = face(candidates[0]);
    size_t x = 0, y = 0;
    for (size_t i = 0; i < old.corners.size(); ++i) {
      if (old.corners[i].vertex == a)
        x = i;
      if (old.corners[i].vertex == z)
        y = i;
    }
    if (x > y)
      std::swap(x, y);
    if (y == x + 1 || (x == 0 && y + 1 == old.corners.size()))
      throw std::invalid_argument("Selected vertices already share an edge");
    PolygonFace first = old, second;
    first.corners.assign(old.corners.begin() + x, old.corners.begin() + y + 1);
    second.id = fresh();
    second.material = old.material;
    for (size_t i = y;; i = (i + 1) % old.corners.size()) {
      auto c = old.corners[i];
      if (i == x || i == y) {
        auto original_corner = c.id;
        c.id = fresh();
        const std::pair<Id, double> source{original_corner, 1};
        record_corner(c.id, {&source, 1});
      }
      second.corners.push_back(c);
      if (i == x)
        break;
    }
    face(old.id) = std::move(first);
    append_face(std::move(second));
  }
};
PolygonMesh
PolygonMesh::from_faces(std::span<const Vec3> vertices,
                        const std::vector<std::vector<uint32_t>> &faces,
                        const PolygonLimits &limits) {
  Budget b{limits};
  if (vertices.size() > limits.maximum_vertices ||
      faces.size() > limits.maximum_faces)
    throw std::length_error("Polygon input exceeds its component budget");
  size_t corners = 0;
  for (auto &f : faces) {
    b.step();
    if (f.size() < 3 || f.size() > limits.maximum_face_corners ||
        f.size() > limits.maximum_corners - corners)
      throw std::length_error("Polygon input exceeds its corner budget");
    corners += f.size();
  }
  PolygonKernel k{{}, b};
  for (auto p : vertices) {
    b.step();
    Id id = k.fresh();
    k.append_vertex({id, p, {{id, 1}}});
  }
  for (auto &indices : faces) {
    if (indices.size() < 3 || indices.size() > limits.maximum_face_corners)
      throw std::invalid_argument("Invalid polygon input face");
    PolygonFace f;
    f.id = k.fresh();
    for (auto i : indices) {
      b.step();
      if (i >= vertices.size())
        throw std::invalid_argument("Polygon input index is out of bounds");
      f.corners.push_back(k.corner(k.mesh.vertices_[i].id, {}, true));
    }
    k.append_face(std::move(f));
  }
  k.rebuild();
  PolygonKernel::validate(k.mesh, b);
  return std::move(k.mesh);
}
PolygonMesh PolygonMesh::from_triangles(const Mesh &m,
                                        const PolygonLimits &limits) {
  if (m.positions.size() > limits.maximum_vertices ||
      m.indices.size() / 3 > limits.maximum_faces ||
      m.indices.size() > limits.maximum_corners)
    throw std::length_error(
        "Polygon triangle input exceeds its component budget");
  if (m.indices.size() % 3 ||
      (!m.uv.empty() && m.uv.size() != m.positions.size()))
    throw std::invalid_argument("Invalid polygon triangle source");
  std::vector<std::vector<uint32_t>> faces;
  for (size_t i = 0; i < m.indices.size(); i += 3)
    faces.push_back({m.indices[i], m.indices[i + 1], m.indices[i + 2]});
  auto out = from_faces(m.positions, faces, limits);
  for (size_t t = 0; t < out.faces_.size(); ++t)
    for (size_t c = 0; c < 3; ++c)
      if (!m.uv.empty()) {
        out.faces_[t].corners[c].uv = m.uv[m.indices[t * 3 + c]];
        out.faces_[t].corners[c].generated_uv = false;
      }
  out.validate(limits);
  return out;
}
PolygonMesh PolygonMesh::cube(float size, const PolygonLimits &limits) {
  if (!std::isfinite(size) || size <= 0)
    throw std::invalid_argument("Invalid polygon cube size");
  float s = size * .5f;
  std::vector<Vec3> v = {{-s, -s, -s}, {s, -s, -s}, {s, s, -s}, {-s, s, -s},
                         {-s, -s, s},  {s, -s, s},  {s, s, s},  {-s, s, s}};
  auto out = from_faces(v,
                        {{0, 3, 2, 1},
                         {4, 5, 6, 7},
                         {0, 1, 5, 4},
                         {1, 2, 6, 5},
                         {2, 3, 7, 6},
                         {3, 0, 4, 7}},
                        limits);
  for (auto &f : out.faces_)
    for (size_t i = 0; i < 4; ++i)
      f.corners[i].uv = std::array<std::array<float, 2>, 4>{
          {{0, 0}, {1, 0}, {1, 1}, {0, 1}}}[i];
  return out;
}
PolygonMesh PolygonMesh::grid(unsigned rows, unsigned columns, float size,
                              const PolygonLimits &limits) {
  if (!rows || !columns || !std::isfinite(size) || size <= 0)
    throw std::invalid_argument("Invalid polygon grid dimensions");
  uint64_t vertices = (uint64_t(rows) + 1) * (uint64_t(columns) + 1),
           faces = uint64_t(rows) * columns;
  if (vertices > limits.maximum_vertices || faces > limits.maximum_faces ||
      faces > limits.maximum_corners / 4 || vertices > UINT32_MAX)
    throw std::length_error("Polygon grid exceeds its component budget");
  Budget budget{limits};
  std::vector<Vec3> positions;
  std::vector<std::vector<uint32_t>> polygons;
  for (uint64_t y = 0; y <= rows; ++y)
    for (uint64_t x = 0; x <= columns; ++x) {
      budget.step();
      positions.push_back({float((double(x) / columns - .5) * size), 0,
                           float((double(y) / rows - .5) * size)});
    }
  for (uint64_t y = 0; y < rows; ++y)
    for (uint64_t x = 0; x < columns; ++x) {
      budget.step();
      auto a = uint32_t(y * (uint64_t(columns) + 1) + x), step = columns + 1;
      polygons.push_back({a, a + step, a + step + 1, a + 1});
    }
  auto out = from_faces(positions, polygons, limits);
  for (auto &f : out.faces_)
    for (auto &c : f.corners) {
      auto vertex = c.vertex - 1;
      c.uv = {float(double(vertex % (uint64_t(columns) + 1)) / columns),
              float(double(vertex / (uint64_t(columns) + 1)) / rows)};
      c.generated_uv = false;
    }
  return out;
}
void PolygonMesh::validate(const PolygonLimits &limits) const {
  Budget b{limits};
  PolygonKernel::validate(*this, b);
}
PolygonRender PolygonMesh::triangulate(const PolygonLimits &limits) const {
  Budget b{limits};
  PolygonKernel::validate(*this, b);
  auto vertices = index(*this, b);
  PolygonRender out;
  for (auto &f : faces_) {
    auto g = geometry(f, vertices, b);
    Vec3 normal = f3(g.normal * (1 / norm(g.normal)));
    if (out.mesh.positions.size() > UINT32_MAX - f.corners.size())
      throw std::length_error("Polygon render index capacity exceeded");
    uint32_t offset = uint32_t(out.mesh.positions.size());
    for (auto &c : f.corners) {
      b.step();
      out.mesh.positions.push_back(vertices.at(c.vertex)->position);
      out.mesh.normals.push_back(normal);
      out.mesh.uv.push_back(c.uv);
      out.vertices.push_back(c.vertex);
      out.corners.push_back(c.id);
    }
    for (auto t : g.triangles) {
      b.step();
      for (auto i : t)
        out.mesh.indices.push_back(offset + uint32_t(i));
      out.triangle_faces.push_back(f.id);
    }
  }
  return out;
}
PolygonAttributeEditResult PolygonKernel::edit(const PolygonMesh &source,
                                               const PolygonEdit &edit,
                                               const PolygonLimits &limits,
                                               bool attributes) {
  Budget budget{limits};
  PolygonKernel::validate(source, budget);
  if (!finite(edit.delta) || !std::isfinite(edit.amount))
    throw std::invalid_argument("Invalid polygon edit value");
  if (edit.selection.ids.empty())
    return {};
  if (edit.selection.mode != PolygonSelectionMode::Vertex &&
      edit.selection.mode != PolygonSelectionMode::Edge &&
      edit.selection.mode != PolygonSelectionMode::Face)
    throw std::invalid_argument("Unknown polygon selection mode");
  PolygonKernel k{source, budget, attributes};
  k.mesh.next_ = std::max(k.mesh.next_, edit.first_id);
  auto ids = k.selected(edit.selection);
  if (edit.operation == PolygonOperation::Move) {
    if (norm(d3(edit.delta)) == 0)
      return {};
    bool changed = false;
    for (auto id : k.selected_vertices(edit.selection, ids)) {
      budget.step();
      auto &v = k.vertex(id);
      auto next = v.position + edit.delta;
      changed |= next.x != v.position.x || next.y != v.position.y ||
                 next.z != v.position.z;
      v.position = next;
    }
    if (!changed)
      return {};
  } else if (edit.operation == PolygonOperation::Extrude ||
             edit.operation == PolygonOperation::Inset) {
    if (edit.selection.mode != PolygonSelectionMode::Face)
      throw std::invalid_argument("Extrude and inset require face selection");
    if (edit.amount == 0 && norm(d3(edit.delta)) == 0)
      return {};
    if (edit.operation == PolygonOperation::Extrude)
      k.extrude(ids, edit);
    else {
      if (edit.amount <= 0)
        throw std::invalid_argument("Inset distance must be positive");
      k.inset(ids, edit.amount);
    }
  } else if (edit.operation == PolygonOperation::Bridge) {
    if (edit.selection.mode != PolygonSelectionMode::Face)
      throw std::invalid_argument("Bridge requires face selection");
    k.bridge(ids);
  } else if (edit.operation == PolygonOperation::Dissolve) {
    if (edit.selection.mode != PolygonSelectionMode::Edge)
      throw std::invalid_argument("Dissolve requires edge selection");
    for (auto id : ids)
      k.dissolve(id);
  } else if (edit.operation == PolygonOperation::Weld) {
    if (edit.selection.mode != PolygonSelectionMode::Vertex)
      throw std::invalid_argument("Weld requires vertex selection");
    if (ids.size() < 2)
      return {};
    k.weld(ids);
  } else if (edit.operation == PolygonOperation::CutEdges) {
    if (edit.selection.mode != PolygonSelectionMode::Edge)
      throw std::invalid_argument("Cut requires edge selection");
    k.cut(ids, edit.cuts);
  } else if (edit.operation == PolygonOperation::ConnectVertices) {
    if (edit.selection.mode != PolygonSelectionMode::Vertex)
      throw std::invalid_argument("Connect requires vertex selection");
    k.connect(ids);
  } else if (edit.operation == PolygonOperation::DeleteFaces ||
             edit.operation == PolygonOperation::FlipFaces) {
    if (edit.selection.mode != PolygonSelectionMode::Face)
      throw std::invalid_argument("Face operation requires face selection");
    if (edit.operation == PolygonOperation::DeleteFaces) {
      std::erase_if(k.mesh.faces_, [&](auto &f) { return ids.contains(f.id); });
      k.prune();
    } else
      for (auto id : ids)
        std::reverse(k.face(id).corners.begin(), k.face(id).corners.end());
  } else
    throw std::invalid_argument("Unknown polygon operation");
  k.rebuild();
  for (auto &e : k.mesh.edges_)
    if (k.propagated_hard.contains(key(e.a, e.b)))
      e.hard = true;
  PolygonKernel::validate(k.mesh, budget);
  PolygonAttributeEditResult result;
  if (attributes)
    for (const auto &face : k.mesh.faces_)
      for (const auto &corner : face.corners) {
        if (auto found = k.corner_sources.find(corner.id);
            found != k.corner_sources.end()) {
          budget.step();
          result.corner_sources.push_back({corner.id, found->second});
        }
      }
  auto &geometry_result = result.geometry;
  for (auto &v : k.mesh.vertices_)
    if (v.id >= source.next_id())
      geometry_result.created_vertices.push_back(v.id);
  for (auto &f : k.mesh.faces_)
    if (f.id >= source.next_id())
      geometry_result.created_faces.push_back(f.id);
  geometry_result.replacement = std::move(k.mesh);
  return result;
}
PolygonEditResult edit_polygon(const PolygonMesh &source,
                               const PolygonEdit &edit,
                               const PolygonLimits &limits) {
  return PolygonKernel::edit(source, edit, limits, false).geometry;
}
PolygonAttributeEditResult
edit_polygon_with_corner_sources(const PolygonMesh &source,
                                 const PolygonEdit &edit,
                                 const PolygonLimits &limits) {
  return PolygonKernel::edit(source, edit, limits, true);
}
nlohmann::json PolygonMesh::encode() const {
  nlohmann::json j = {{"version", 1},
                      {"next_id", next_},
                      {"vertices", nlohmann::json::array()},
                      {"faces", nlohmann::json::array()},
                      {"edges", nlohmann::json::array()}};
  for (auto &v : vertices_)
    j["vertices"].push_back(
        {v.id, v.position.x, v.position.y, v.position.z, v.provenance});
  for (auto &f : faces_) {
    nlohmann::json corners = nlohmann::json::array();
    for (auto &c : f.corners)
      corners.push_back({c.id, c.vertex, c.uv, c.generated_uv});
    j["faces"].push_back({{"id", f.id},
                          {"material", f.material},
                          {"corners", std::move(corners)}});
  }
  for (auto &e : edges_)
    j["edges"].push_back({e.id, e.a, e.b, e.hard});
  return j;
}
PolygonMesh PolygonMesh::decode(const nlohmann::json &j,
                                const PolygonLimits &limits) {
  Budget budget{limits};
  budget.step();
  if (!j.is_object() || j.size() != 5 ||
      (!j.at("version").is_number_integer() || j.at("version") != 1) ||
      !j.at("vertices").is_array() || !j.at("faces").is_array() ||
      !j.at("edges").is_array())
    throw std::invalid_argument("Unsupported polygon authority archive");
  if (j.at("vertices").size() > limits.maximum_vertices ||
      j.at("faces").size() > limits.maximum_faces ||
      j.at("edges").size() > limits.maximum_corners)
    throw std::length_error("Polygon archive exceeds its component budget");
  size_t count = 0;
  for (auto &f : j.at("faces")) {
    budget.step();
    if (!f.is_object() || f.size() != 3 || !f.at("corners").is_array() ||
        f.at("corners").size() > limits.maximum_face_corners ||
        f.at("corners").size() > limits.maximum_corners - count)
      throw std::length_error("Polygon archive exceeds its corner budget");
    count += f.at("corners").size();
  }
  PolygonMesh out;
  out.next_ = archive_id(j.at("next_id"));
  for (auto &v : j.at("vertices")) {
    budget.step();
    if (!v.is_array() || v.size() != 5 || !v[4].is_array() ||
        v[4].size() > limits.maximum_provenance)
      throw std::invalid_argument("Invalid polygon vertex archive");
    for (auto &w : v[4]) {
      budget.step();
      if (!w.is_array() || w.size() != 2)
        throw std::invalid_argument(
            "Invalid polygon source interpolation archive");
      archive_id(w[0]);
    }
    out.vertices_.push_back(
        {archive_id(v[0]),
         {v[1].get<float>(), v[2].get<float>(), v[3].get<float>()},
         v[4].get<std::vector<std::pair<Id, double>>>()});
  }
  for (auto &f : j.at("faces")) {
    budget.step();
    PolygonFace face;
    face.id = archive_id(f.at("id"));
    auto material = archive_id(f.at("material"));
    if (material > UINT32_MAX)
      throw std::invalid_argument("Polygon material identity is out of range");
    face.material = uint32_t(material);
    for (auto &c : f.at("corners")) {
      budget.step();
      if (!c.is_array() || c.size() != 4 || !c[2].is_array() ||
          c[2].size() != 2 || !c[3].is_boolean())
        throw std::invalid_argument("Invalid polygon corner archive");
      face.corners.push_back({archive_id(c[0]), archive_id(c[1]),
                              c[2].get<std::array<float, 2>>(),
                              c[3].get<bool>()});
    }
    out.faces_.push_back(std::move(face));
  }
  for (auto &e : j.at("edges")) {
    budget.step();
    if (!e.is_array() || e.size() != 4 || !e[3].is_boolean())
      throw std::invalid_argument("Invalid polygon edge archive");
    out.edges_.push_back({archive_id(e[0]), archive_id(e[1]), archive_id(e[2]),
                          e[3].get<bool>()});
  }
  PolygonKernel::validate(out, budget);
  return out;
}
} // namespace dome
