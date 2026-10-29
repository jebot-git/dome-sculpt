// SPDX-License-Identifier: GPL-3.0-or-later
// Schedule, edge-length hysteresis and valence-improving swaps informed by
// VCGLib isotropic_remeshing.h, Copyright(C)2004-2017 Visual Computing Lab,
// ISTI-CNR, GPL-2.0-or-later. Native transactional/chart-preserving
// integration.
#include "dome/remesh.hpp"
#include <bit>
#include <map>
#include <numeric>
#include <set>
namespace dome {
namespace {
using Edge = std::array<uint32_t, 2>;
Edge edge(uint32_t a, uint32_t b) { return {std::min(a, b), std::max(a, b)}; }
using Point = std::array<uint32_t, 3>;
Point point(Vec3 p) {
  return {p.x == 0 ? 0 : std::bit_cast<uint32_t>(p.x),
          p.y == 0 ? 0 : std::bit_cast<uint32_t>(p.y),
          p.z == 0 ? 0 : std::bit_cast<uint32_t>(p.z)};
}
using GeometricEdge = std::array<Point, 2>;
GeometricEdge geometric(Vec3 a, Vec3 b) {
  auto x = point(a), y = point(b);
  return {std::min(x, y), std::max(x, y)};
}
struct Vertex {
  Vec3 p{}, n{};
  std::array<float, 2> uv{};
  float target = 0;
  bool pinned = false;
};
struct Face {
  std::array<uint32_t, 3> v{};
  uint32_t material = 0, chart = 0;
  bool live = true;
};
struct Use {
  uint32_t face = 0;
  uint8_t corner = 0;
};
struct Adjacency {
  std::map<Edge, std::vector<Use>> edges;
  std::vector<std::set<uint32_t>> neighbors;
  std::vector<std::vector<uint32_t>> faces;
};
struct Dsu {
  std::vector<uint32_t> p;
  explicit Dsu(size_t n) : p(n) { std::iota(p.begin(), p.end(), 0); }
  uint32_t root(uint32_t x) {
    while (p[x] != x) {
      p[x] = p[p[x]];
      x = p[x];
    }
    return x;
  }
  void join(uint32_t a, uint32_t b) { p[root(a)] = root(b); }
};
struct Worker {
  const Mesh &source;
  const RemeshOptions &o;
  SurfaceQuery query;
  std::vector<Vertex> vertices;
  std::vector<Face> faces;
  std::set<Edge> protected_edges, seams;
  std::vector<uint32_t> source_charts;
  std::vector<Vec3> source_normals;
  size_t operations = 0, edge_visits = 0, projection_tests = 0;
  RemeshResult result;
  Worker(const Mesh &s, const RemeshOptions &options)
      : source(s), o(options), query(s, options.cancelled) {}
  void stop() const {
    if (o.cancelled && o.cancelled())
      throw std::runtime_error("Remeshing cancelled");
  }
  void visit() {
    if (++edge_visits > o.max_edge_visits)
      throw std::length_error("Remesh edge work budget exceeded");
    if (edge_visits % 128 == 0)
      stop();
  }
  void operation() {
    stop();
    if (++operations > o.max_operations)
      throw std::length_error("Remesh operation budget exceeded");
  }
  Vec3 normal(const Face &f) const {
    return cross(vertices[f.v[1]].p - vertices[f.v[0]].p,
                 vertices[f.v[2]].p - vertices[f.v[0]].p);
  }
  Adjacency adjacency() {
    stop();
    Adjacency out;
    out.neighbors.resize(vertices.size());
    out.faces.resize(vertices.size());
    for (uint32_t t = 0; t < faces.size(); t++)
      if (faces[t].live) {
        auto &f = faces[t];
        for (uint8_t k = 0; k < 3; k++) {
          visit();
          auto a = f.v[k], b = f.v[(k + 1) % 3];
          out.edges[edge(a, b)].push_back({t, k});
          out.neighbors[a].insert(b);
          out.neighbors[b].insert(a);
          out.faces[a].push_back(t);
        }
      }
    return out;
  }
  std::vector<bool> fixed(const Adjacency &a) const {
    std::vector<bool> out(vertices.size());
    for (size_t i = 0; i < vertices.size(); i++)
      out[i] = vertices[i].pinned;
    for (auto &[e, use] : a.edges)
      if (use.size() != 2 || protected_edges.contains(e)) {
        out[e[0]] = out[e[1]] = true;
      }
    return out;
  }
  SurfaceHit project(Vec3 p, uint32_t chart, Vec3 expected) {
    expected = normalized(expected);
    stop();
    SurfaceQueryOptions opts;
    opts.cancelled = o.cancelled;
    opts.triangle_filter = [&](uint32_t t) {
      if (++projection_tests > o.max_projection_triangle_tests)
        throw std::length_error("Remesh projection work budget exceeded");
      return source_charts[t] == chart &&
             dot(source_normals[t], expected) > .1f;
    };
    auto hit = query.closest(p, opts);
    if (hit.distance > o.maximum_projection_distance)
      throw std::runtime_error(
          "Remesh source projection exceeds distance bound");
    return hit;
  }
  Vertex projected(Vec3 p, uint32_t chart, float target, Vec3 expected) {
    auto h = project(p, chart, expected);
    Vertex v;
    v.p = h.position;
    v.n = h.normal;
    v.target = target;
    if (!source.uv.empty())
      for (size_t k = 0; k < 3; k++)
        for (size_t j = 0; j < 2; j++)
          v.uv[j] += source.uv[source.indices[h.triangle * 3 + k]][j] *
                     h.barycentric[k];
    return v;
  }
  bool shape(std::array<uint32_t, 3> ids, Vec3 old,
             std::optional<std::pair<uint32_t, Vec3>> replacement = {}) const {
    std::array<Vec3, 3> p;
    for (size_t k = 0; k < 3; k++)
      p[k] = replacement && replacement->first == ids[k] ? replacement->second
                                                         : vertices[ids[k]].p;
    auto n = cross(p[1] - p[0], p[2] - p[0]);
    double area2 = dot(n, n), old2 = dot(old, old);
    return area2 > 1e-20 && std::isfinite(area2) &&
           dot(n, old) > std::sqrt(area2 * old2) * .1;
  }
  void validate(const Adjacency &a) {
    for (auto &[e, uses] : a.edges) {
      visit();
      if (uses.size() > 2)
        throw std::runtime_error("Remesh generated nonmanifold edge");
      if (uses.size() == 2) {
        auto u = uses[0], v = uses[1];
        if (faces[u.face].v[u.corner] != faces[v.face].v[(v.corner + 1) % 3])
          throw std::runtime_error("Remesh generated inconsistent winding");
      }
    }
    std::set<std::array<uint32_t, 3>> unique;
    for (auto &f : faces)
      if (f.live) {
        auto sorted = f.v;
        std::sort(sorted.begin(), sorted.end());
        if (sorted[0] == sorted[1] || sorted[1] == sorted[2] ||
            !unique.insert(sorted).second || dot(normal(f), normal(f)) < 1e-20f)
          throw std::runtime_error(
              "Remesh generated duplicate/degenerate faces");
      }
    for (uint32_t v = 0; v < vertices.size(); v++)
      if (!a.faces[v].empty()) {
        stop();
        std::set<uint32_t> pending = {a.faces[v][0]}, seen;
        while (!pending.empty()) {
          auto t = *pending.begin();
          pending.erase(pending.begin());
          if (!seen.insert(t).second)
            continue;
          auto &f = faces[t];
          for (size_t k = 0; k < 3; k++)
            if (f.v[k] == v || f.v[(k + 1) % 3] == v)
              for (auto use : a.edges.at(edge(f.v[k], f.v[(k + 1) % 3])))
                if (!seen.contains(use.face))
                  pending.insert(use.face);
        }
        if (seen.size() != a.faces[v].size())
          throw std::runtime_error(
              "Remesh input/result has disconnected vertex face fans");
      }
  }
  void split_pass() {
    auto a = adjacency();
    std::map<GeometricEdge, std::vector<Edge>> groups;
    for (auto &[e, use] : a.edges)
      groups[geometric(vertices[e[0]].p, vertices[e[1]].p)].push_back(e);
    std::set<uint32_t> busy;
    for (auto &[key, group] : groups) {
      visit();
      float target = INFINITY;
      bool available = true;
      for (auto e : group) {
        target = std::min(
            target, (vertices[e[0]].target + vertices[e[1]].target) * .5f);
        for (auto use : a.edges.at(e))
          if (busy.contains(use.face))
            available = false;
      }
      auto first = group[0];
      if (!available || length(vertices[first[0]].p - vertices[first[1]].p) <=
                            target * (4.f / 3))
        continue;
      if (vertices.size() + group.size() > o.max_vertices ||
          faces.size() + group.size() * 2 > o.max_triangles)
        throw std::length_error("Remesh split resource budget exceeded");
      // All attribute copies of one geometric boundary edge split together.
      std::vector<std::pair<Edge, Vertex>> additions;
      for (auto e : group) {
        auto x = vertices[e[0]], y = vertices[e[1]];
        auto p = x.p + (y.p - x.p) * .5f;
        if (point(y.p) < point(x.p))
          p = y.p + (x.p - y.p) * .5f;
        Vertex v;
        if (protected_edges.contains(e) || a.edges.at(e).size() != 2) {
          v.p = p;
          v.n = normalized(x.n + y.n);
          v.target = (x.target + y.target) * .5f;
          for (size_t j = 0; j < 2; j++)
            v.uv[j] = (x.uv[j] + y.uv[j]) * .5f;
        } else
          v = projected(p, faces[a.edges.at(e)[0].face].chart,
                        (x.target + y.target) * .5f,
                        normal(faces[a.edges.at(e)[0].face]));
        for (auto use : a.edges.at(e)) {
          auto old = faces[use.face];
          auto aa = old.v[use.corner], bb = old.v[(use.corner + 1) % 3],
               c = old.v[(use.corner + 2) % 3];
          auto n = normal(old);
          auto p0 = vertices[aa].p, p1 = vertices[bb].p, pc = vertices[c].p;
          if (dot(cross(v.p - p0, pc - p0), n) <= 0 ||
              dot(cross(p1 - v.p, pc - v.p), n) <= 0) {
            available = false;
            break;
          }
        }
        additions.push_back({e, v});
      }
      if (!available)
        continue;
      for (auto &[e, v] : additions) {
        operation();
        auto mid = uint32_t(vertices.size());
        vertices.push_back(v);
        bool protect = protected_edges.erase(e), seam = seams.erase(e);
        if (protect) {
          protected_edges.insert(edge(e[0], mid));
          protected_edges.insert(edge(mid, e[1]));
        }
        if (seam) {
          seams.insert(edge(e[0], mid));
          seams.insert(edge(mid, e[1]));
        }
        for (auto use : a.edges.at(e)) {
          auto old = faces[use.face];
          busy.insert(use.face);
          auto aa = old.v[use.corner], bb = old.v[(use.corner + 1) % 3],
               c = old.v[(use.corner + 2) % 3];
          faces[use.face].v = {aa, mid, c};
          old.v = {mid, bb, c};
          faces.push_back(old);
        }
        ++result.splits;
      }
    }
  }
  void collapse_pass() {
    auto a = adjacency();
    auto locked = fixed(a);
    std::set<uint32_t> busy;
    for (auto &[e, use] : a.edges) {
      visit();
      auto aa = e[0], bb = e[1];
      if (use.size() != 2 || locked[aa] || locked[bb] || busy.contains(aa) ||
          busy.contains(bb))
        continue;
      if (length(vertices[aa].p - vertices[bb].p) >=
          (vertices[aa].target + vertices[bb].target) * .4f)
        continue;
      std::set<uint32_t> shared;
      std::set_intersection(a.neighbors[aa].begin(), a.neighbors[aa].end(),
                            a.neighbors[bb].begin(), a.neighbors[bb].end(),
                            std::inserter(shared, shared.end()));
      std::set<uint32_t> opposite;
      for (auto u : use)
        for (auto v : faces[u.face].v)
          if (v != aa && v != bb)
            opposite.insert(v);
      if (shared != opposite || opposite.size() != 2)
        continue; // Manifold link condition.
      uint32_t chart = faces[use[0].face].chart;
      bool okay = true;
      std::set<uint32_t> incident(a.faces[aa].begin(), a.faces[aa].end());
      incident.insert(a.faces[bb].begin(), a.faces[bb].end());
      for (auto t : incident) {
        if (faces[t].chart != chart)
          okay = false;
        for (auto v : faces[t].v)
          if (busy.contains(v))
            okay = false;
      }
      if (!okay)
        continue;
      auto p =
          projected((vertices[aa].p + vertices[bb].p) * .5f, chart,
                    (vertices[aa].target + vertices[bb].target) * .5f,
                    normal(faces[use[0].face]) + normal(faces[use[1].face]));
      std::set<std::array<uint32_t, 3>> candidate;
      for (auto t : incident) {
        auto f = faces[t];
        bool both = std::find(f.v.begin(), f.v.end(), aa) != f.v.end() &&
                    std::find(f.v.begin(), f.v.end(), bb) != f.v.end();
        if (both)
          continue;
        for (auto &v : f.v)
          if (v == bb)
            v = aa;
        if (!shape(f.v, normal(faces[t]), std::pair{aa, p.p})) {
          okay = false;
          break;
        }
        auto sorted = f.v;
        std::sort(sorted.begin(), sorted.end());
        if (!candidate.insert(sorted).second) {
          okay = false;
          break;
        }
      }
      if (!okay)
        continue;
      operation();
      vertices[aa] = p;
      for (auto t : incident) {
        auto &f = faces[t];
        bool both = std::find(f.v.begin(), f.v.end(), aa) != f.v.end() &&
                    std::find(f.v.begin(), f.v.end(), bb) != f.v.end();
        if (both)
          f.live = false;
        else
          for (auto &v : f.v)
            if (v == bb)
              v = aa;
        for (auto v : f.v)
          busy.insert(v);
      }
      busy.insert(bb);
      ++result.collapses;
    }
  }
  void flip_pass() {
    auto a = adjacency();
    auto locked = fixed(a);
    std::set<uint32_t> busy;
    for (auto &[e, use] : a.edges) {
      visit();
      if (use.size() != 2 || protected_edges.contains(e))
        continue;
      auto u = use[0], v = use[1];
      if (busy.contains(u.face) || busy.contains(v.face))
        continue;
      auto f = faces[u.face], g = faces[v.face];
      if (f.chart != g.chart || f.material != g.material)
        continue;
      uint32_t aa = f.v[u.corner], bb = f.v[(u.corner + 1) % 3],
               c = f.v[(u.corner + 2) % 3], d = g.v[(v.corner + 2) % 3];
      if (c == d || a.edges.contains(edge(c, d)))
        continue;
      auto ideal = [&](uint32_t i) { return locked[i] ? 4 : 6; };
      auto cost = [&](uint32_t i, int delta) {
        int x = int(a.neighbors[i].size()) + delta - ideal(i);
        return x * x;
      };
      int before = cost(aa, 0) + cost(bb, 0) + cost(c, 0) + cost(d, 0),
          after = cost(aa, -1) + cost(bb, -1) + cost(c, 1) + cost(d, 1);
      if (after >= before)
        continue;
      std::array<uint32_t, 3> nf = {c, d, bb}, ng = {d, c, aa};
      if (!shape(nf, normal(f)) || !shape(nf, normal(g)) ||
          !shape(ng, normal(f)) || !shape(ng, normal(g)))
        continue;
      auto midpoint = (vertices[c].p + vertices[d].p) * .5f;
      project(midpoint, f.chart, normal(f) + normal(g));
      operation();
      faces[u.face].v = nf;
      faces[v.face].v = ng;
      busy.insert(u.face);
      busy.insert(v.face);
      ++result.flips;
    }
  }
  void relax_pass() {
    auto a = adjacency();
    auto locked = fixed(a);
    auto previous = vertices;
    for (uint32_t i = 0; i < vertices.size(); i++) {
      stop();
      if (locked[i] || a.faces[i].empty() || a.neighbors[i].empty())
        continue;
      auto chart = faces[a.faces[i][0]].chart;
      bool okay = true;
      for (auto t : a.faces[i])
        if (faces[t].chart != chart)
          okay = false;
      if (!okay)
        continue;
      Vec3 average{};
      for (auto other : a.neighbors[i])
        average += previous[other].p;
      Vec3 expected{};
      for (auto t : a.faces[i])
        expected += normal(faces[t]);
      auto v =
          projected(previous[i].p + (average / float(a.neighbors[i].size()) -
                                     previous[i].p) *
                                        o.relax_strength,
                    chart, previous[i].target, expected);
      for (auto t : a.faces[i])
        if (!shape(faces[t].v, normal(faces[t]), std::pair{i, v.p})) {
          okay = false;
          break;
        }
      if (okay && length(v.p - vertices[i].p) > 1e-10f) {
        operation();
        vertices[i] = v;
        ++result.relaxed_vertices;
      }
    }
  }
};
} // namespace
RemeshResult remesh_triangles(const Mesh &source, const RemeshOptions &o,
                              std::span<const uint32_t> materials,
                              std::span<const float> targets) {
  if (o.cancelled && o.cancelled())
    throw std::runtime_error("Remeshing cancelled");
  if (!std::isfinite(o.target_length) || o.target_length < 1e-5f ||
      o.target_length > 100 || o.iterations > 50 ||
      !std::isfinite(o.adaptivity) || o.adaptivity < 0 || o.adaptivity > 16 ||
      !std::isfinite(o.feature_angle_degrees) || o.feature_angle_degrees < 0 ||
      o.feature_angle_degrees > 180 || !std::isfinite(o.relax_strength) ||
      o.relax_strength < 0 || o.relax_strength > 1 ||
      !std::isfinite(o.maximum_projection_distance) ||
      o.maximum_projection_distance < 0 ||
      o.maximum_projection_distance > 100 || !o.max_vertices ||
      o.max_vertices > 2'000'000 || !o.max_triangles ||
      o.max_triangles > 2'000'000 || !o.max_operations ||
      o.max_operations > 2'000'000 || !o.max_edge_visits ||
      o.max_edge_visits > 200'000'000 || !o.max_projection_triangle_tests ||
      o.max_projection_triangle_tests > 200'000'000)
    throw std::invalid_argument("Invalid remeshing options");
  source.validate();
  auto report = inspect_mesh(source, o.cancelled);
  if (report.degenerate_triangles || report.duplicate_triangles ||
      report.nonmanifold_edges || report.inconsistent_edges)
    throw std::runtime_error(
        "Remeshing requires consistently oriented manifold triangles");
  if (source.positions.size() > o.max_vertices ||
      source.indices.size() / 3 > o.max_triangles)
    throw std::length_error("Remesh input exceeds resource budget");
  if ((!materials.empty() && materials.size() != source.indices.size() / 3) ||
      (!targets.empty() && targets.size() != source.positions.size()))
    throw std::invalid_argument("Remesh material/target field count mismatch");
  Worker w(source, o);
  w.vertices.resize(source.positions.size());
  for (size_t i = 0; i < source.positions.size(); i++) {
    auto &v = w.vertices[i];
    v.p = source.positions[i];
    v.n = source.normals[i];
    if (!source.uv.empty())
      v.uv = source.uv[i];
    v.pinned = !source.uv_pins.empty() && source.uv_pins[i];
    v.target = targets.empty() ? o.target_length : targets[i];
    if (!std::isfinite(v.target) || v.target < 1e-5f || v.target > 100)
      throw std::invalid_argument(
          "Remesh target field outside finite length bounds");
  }
  for (uint32_t t = 0; t < source.indices.size() / 3; t++) {
    Face f;
    for (size_t k = 0; k < 3; k++) {
      f.v[k] = source.indices[t * 3 + k];
      if (!source.uv_seams.empty() && source.uv_seams[t * 3 + k])
        w.seams.insert(edge(f.v[k], source.indices[t * 3 + (k + 1) % 3]));
    }
    f.material = materials.empty() ? 0 : materials[t];
    w.faces.push_back(f);
  }
  auto original = w.adjacency();
  w.validate(original);
  Dsu charts(w.faces.size());
  float cosine =
      std::cos(o.feature_angle_degrees * 3.14159265358979323846f / 180);
  std::vector<float> curvature(w.vertices.size());
  for (auto &[e, use] : original.edges) {
    w.visit();
    bool barrier = use.size() != 2 || w.seams.contains(e);
    if (use.size() == 2) {
      auto n = normalized(w.normal(w.faces[use[0].face])),
           m = normalized(w.normal(w.faces[use[1].face]));
      float alignment = std::clamp(dot(n, m), -1.f, 1.f);
      auto bend = std::acos(alignment) / 3.14159265358979323846f;
      curvature[e[0]] = std::max(curvature[e[0]], bend);
      curvature[e[1]] = std::max(curvature[e[1]], bend);
      barrier |= alignment < cosine ||
                 w.faces[use[0].face].material != w.faces[use[1].face].material;
    }
    if (barrier)
      w.protected_edges.insert(e);
    else
      charts.join(use[0].face, use[1].face);
  }
  w.source_charts.resize(w.faces.size());
  w.source_normals.resize(w.faces.size());
  for (uint32_t t = 0; t < w.faces.size(); t++) {
    w.faces[t].chart = charts.root(t);
    w.source_charts[t] = w.faces[t].chart;
    w.source_normals[t] = normalized(w.normal(w.faces[t]));
  }
  if (targets.empty())
    for (size_t i = 0; i < w.vertices.size(); i++)
      w.vertices[i].target =
          o.target_length *
          std::max(.25f, 1 / (1 + o.adaptivity * curvature[i]));
  for (unsigned pass = 0; pass < o.iterations; pass++) {
    w.stop();
    if (o.split)
      w.split_pass();
    if (o.collapse)
      w.collapse_pass();
    if (o.flip)
      w.flip_pass();
    if (o.relax && o.relax_strength > 0)
      w.relax_pass();
    w.validate(w.adjacency());
  }
  std::map<uint32_t, uint32_t> packed;
  for (auto &f : w.faces)
    if (f.live) {
      w.stop();
      w.result.materials.push_back(f.material);
      for (auto old : f.v) {
        auto [it, added] =
            packed.emplace(old, uint32_t(w.result.mesh.positions.size()));
        auto &v = w.vertices[old];
        if (added) {
          w.result.mesh.positions.push_back(v.p);
          w.result.mesh.normals.push_back(v.n);
          if (!source.uv.empty())
            w.result.mesh.uv.push_back(v.uv);
          w.result.mesh.uv_pins.push_back(v.pinned);
        }
        w.result.mesh.indices.push_back(it->second);
        auto h = w.project(v.p, f.chart, w.normal(f));
        w.result.source_corners.push_back({h.triangle, h.barycentric});
      }
      for (size_t k = 0; k < 3; k++)
        w.result.mesh.uv_seams.push_back(
            w.seams.contains(edge(f.v[k], f.v[(k + 1) % 3])));
    }
  w.result.mesh.validate();
  auto final = inspect_mesh(w.result.mesh, o.cancelled);
  if (final.degenerate_triangles || final.duplicate_triangles ||
      final.nonmanifold_edges || final.inconsistent_edges ||
      final.components != report.components ||
      (report.closed_manifold() && !final.closed_manifold()))
    throw std::runtime_error(
        "Remesh result failed geometric topology preservation");
  w.stop();
  return std::move(w.result);
}
} // namespace dome
