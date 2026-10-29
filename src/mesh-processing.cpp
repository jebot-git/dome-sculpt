// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh-processing.hpp"
#include <bit>
#include <limits>
#include <map>
#include <numeric>
#include <set>
namespace dome {
namespace {
void cancel(const std::function<bool()> &fn) {
  if (fn && fn())
    throw std::runtime_error("Mesh processing cancelled");
}
uint32_t bits(float f) { return f == 0 ? 0 : std::bit_cast<uint32_t>(f); }
using PointKey = std::array<uint32_t, 3>;
PointKey key(Vec3 p) { return {bits(p.x), bits(p.y), bits(p.z)}; }
void structural(const Mesh &m, const std::function<bool()> &fn = {}) {
  if (m.positions.empty() || m.positions.size() > 2'000'000 ||
      m.indices.empty() || m.indices.size() % 3 ||
      m.indices.size() / 3 > 2'000'000)
    throw std::runtime_error("Invalid mesh size");
  if (m.normals.size() != m.positions.size() ||
      (!m.uv.empty() && m.uv.size() != m.positions.size()) ||
      (!m.uv_pins.empty() && m.uv_pins.size() != m.positions.size()) ||
      (!m.uv_seams.empty() && m.uv_seams.size() != m.indices.size()))
    throw std::runtime_error("Invalid mesh attribute count");
  for (size_t i = 0; i < m.positions.size(); ++i) {
    if (i % 512 == 0)
      cancel(fn);
    auto p = m.positions[i];
    if (std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) > 100)
      throw std::runtime_error("Mesh outside 100m authoring workspace");
    if (!finite(m.positions[i]) || !finite(m.normals[i]) ||
        !std::isfinite(length(m.normals[i])))
      throw std::runtime_error("Nonfinite mesh vertex");
    if (!m.uv.empty() &&
        (!std::isfinite(m.uv[i][0]) || !std::isfinite(m.uv[i][1])))
      throw std::runtime_error("Nonfinite UV");
  }
  for (auto i : m.indices)
    if (i >= m.positions.size())
      throw std::runtime_error("Mesh index out of range");
  for (auto f : m.uv_pins)
    if (f > 1)
      throw std::runtime_error("Invalid UV pin");
  for (auto f : m.uv_seams)
    if (f > 1)
      throw std::runtime_error("Invalid UV seam");
}
struct Geometry {
  std::vector<uint32_t> ids;
  explicit Geometry(const Mesh &m, const std::function<bool()> &fn = {}) {
    std::map<PointKey, uint32_t> unique;
    size_t visited = 0;
    for (auto p : m.positions) {
      if (visited++ % 512 == 0)
        cancel(fn);
      auto [i, added] = unique.emplace(key(p), uint32_t(unique.size()));
      ids.push_back(i->second);
    }
  }
};
bool degenerate(const Mesh &m, size_t t) {
  auto a = m.positions[m.indices[t]], b = m.positions[m.indices[t + 1]],
       c = m.positions[m.indices[t + 2]];
  auto n = cross(b - a, c - a);
  return double(dot(n, n)) < 1e-20;
}
struct Dsu {
  std::vector<uint32_t> p;
  explicit Dsu(size_t n) : p(n) { std::iota(p.begin(), p.end(), 0); }
  uint32_t root(uint32_t i) {
    while (p[i] != i) {
      p[i] = p[p[i]];
      i = p[i];
    }
    return i;
  }
  void join(uint32_t a, uint32_t b) { p[root(a)] = root(b); }
};
struct Bounds {
  Vec3 lo{INFINITY, INFINITY, INFINITY}, hi{-INFINITY, -INFINITY, -INFINITY};
  void add(Vec3 p) {
    lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
    hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
  }
  void add(const Bounds &b) {
    add(b.lo);
    add(b.hi);
  }
  double distance2(Vec3 p) const {
    double x = std::max({double(lo.x - p.x), 0., double(p.x - hi.x)}),
           y = std::max({double(lo.y - p.y), 0., double(p.y - hi.y)}),
           z = std::max({double(lo.z - p.z), 0., double(p.z - hi.z)});
    return x * x + y * y + z * z;
  }
  bool ray(Vec3 p, Vec3 d) const {
    double lo_t = 0, hi_t = INFINITY;
    float ps[] = {p.x, p.y, p.z}, ds[] = {d.x, d.y, d.z},
          ls[] = {lo.x, lo.y, lo.z}, hs[] = {hi.x, hi.y, hi.z};
    for (int k = 0; k < 3; k++) {
      if (std::abs(ds[k]) < 1e-20) {
        if (ps[k] < ls[k] || ps[k] > hs[k])
          return false;
        continue;
      }
      double a = (ls[k] - ps[k]) / ds[k], b = (hs[k] - ps[k]) / ds[k];
      if (a > b)
        std::swap(a, b);
      lo_t = std::max(lo_t, a);
      hi_t = std::min(hi_t, b);
      if (lo_t > hi_t)
        return false;
    }
    return true;
  }
};
// Region classification adapted from Blender's pinned
// closest_on_tri_to_point_v3 (math_geom.cc.reference1045..1178), itself adapted
// from Ericson's RTCD.
Vec3 closest(Vec3 p, Vec3 a, Vec3 b, Vec3 c) {
  Vec3 ab = b - a, ac = c - a, ap = p - a;
  double d1 = dot(ab, ap), d2 = dot(ac, ap);
  if (d1 <= 0 && d2 <= 0)
    return a;
  Vec3 bp = p - b;
  double d3 = dot(ab, bp), d4 = dot(ac, bp);
  if (d3 >= 0 && d4 <= d3)
    return b;
  double vc = d1 * d4 - d3 * d2;
  if (vc <= 0 && d1 >= 0 && d3 <= 0)
    return a + ab * float(d1 / (d1 - d3));
  Vec3 cp = p - c;
  double d5 = dot(ab, cp), d6 = dot(ac, cp);
  if (d6 >= 0 && d5 <= d6)
    return c;
  double vb = d5 * d2 - d1 * d6;
  if (vb <= 0 && d2 >= 0 && d6 <= 0)
    return a + ac * float(d2 / (d2 - d6));
  double va = d3 * d6 - d5 * d4;
  if (va <= 0 && d4 - d3 >= 0 && d5 - d6 >= 0)
    return b + (c - b) * float((d4 - d3) / ((d4 - d3) + (d5 - d6)));
  double denom = 1 / (va + vb + vc);
  return a + ab * float(vb * denom) + ac * float(vc * denom);
}
struct Bvh {
  const Mesh &m;
  std::vector<uint32_t> order;
  struct Node {
    Bounds box;
    size_t start = 0, count = 0;
    int left = -1, right = -1;
  };
  std::vector<Node> nodes;
  std::function<bool()> cancelled;
  Bvh(const Mesh &mesh, std::function<bool()> fn)
      : m(mesh), cancelled(std::move(fn)) {
    order.resize(m.indices.size() / 3);
    std::iota(order.begin(), order.end(), 0);
    build(0, order.size());
  }
  Vec3 point(uint32_t t, int k) const {
    return m.positions[m.indices[size_t(t) * 3 + k]];
  }
  int build(size_t start, size_t count) {
    cancel(cancelled);
    int index = int(nodes.size());
    nodes.push_back({});
    Bounds box, centres;
    for (size_t i = start; i < start + count; i++) {
      if (i % 1024 == 0)
        cancel(cancelled);
      auto t = order[i];
      for (int k = 0; k < 3; k++)
        box.add(point(t, k));
      centres.add((point(t, 0) + point(t, 1) + point(t, 2)) / 3);
    }
    nodes[index].box = box;
    nodes[index].start = start;
    nodes[index].count = count;
    if (count <= 8)
      return index;
    auto extent = centres.hi - centres.lo;
    int axis = extent.y > extent.x ? 1 : 0;
    if (extent.z > (axis == 0 ? extent.x : extent.y))
      axis = 2;
    auto value = [&](uint32_t t) {
      auto c = point(t, 0) + point(t, 1) + point(t, 2);
      return axis == 0 ? c.x : axis == 1 ? c.y : c.z;
    };
    size_t mid = start + count / 2;
    std::nth_element(order.begin() + start, order.begin() + mid,
                     order.begin() + start + count,
                     [&](auto a, auto b) { return value(a) < value(b); });
    int left = build(start, mid - start),
        right = build(mid, start + count - mid);
    nodes[index].left = left;
    nodes[index].right = right;
    return index;
  }
  double nearest(Vec3 p, int n, double best) const {
    auto &node = nodes[n];
    if (node.box.distance2(p) > best)
      return best;
    if (node.left < 0) {
      for (size_t i = node.start; i < node.start + node.count; i++) {
        auto t = order[i];
        auto q = closest(p, point(t, 0), point(t, 1), point(t, 2));
        auto d = p - q;
        best = std::min(best, double(dot(d, d)));
      }
      return best;
    }
    int a = node.left, b = node.right;
    if (nodes[a].box.distance2(p) > nodes[b].box.distance2(p))
      std::swap(a, b);
    best = nearest(p, a, best);
    return nearest(p, b, best);
  }
  double closest_hit(Vec3 p, int n, double best, uint32_t &triangle,
                     Vec3 &position, const SurfaceQueryOptions &options,
                     size_t &visited) const {
    cancel(options.cancelled);
    auto &node = nodes[n];
    if (node.box.distance2(p) > best)
      return best;
    if (node.left < 0) {
      for (size_t i = node.start; i < node.start + node.count; i++) {
        if (++visited > options.max_triangle_tests)
          throw std::length_error(
              "Closest surface triangle work budget exceeded");
        auto t = order[i];
        if (options.triangle_filter && !options.triangle_filter(t))
          continue;
        auto q = closest(p, point(t, 0), point(t, 1), point(t, 2));
        auto d = p - q;
        double distance = dot(d, d);
        if (distance < best) {
          best = distance;
          triangle = t;
          position = q;
        }
      }
      return best;
    }
    int a = node.left, b = node.right;
    if (nodes[a].box.distance2(p) > nodes[b].box.distance2(p))
      std::swap(a, b);
    best = closest_hit(p, a, best, triangle, position, options, visited);
    return closest_hit(p, b, best, triangle, position, options, visited);
  }
  void ray_hit(Vec3 p, Vec3 d, int n, double &best, uint32_t &triangle,
               std::array<float, 3> &bary) const {
    auto &node = nodes[n];
    if (!node.box.ray(p, d))
      return;
    if (node.left >= 0) {
      ray_hit(p, d, node.left, best, triangle, bary);
      ray_hit(p, d, node.right, best, triangle, bary);
      return;
    }
    for (size_t i = node.start; i < node.start + node.count; i++) {
      auto t = order[i];
      auto a = point(t, 0), e1 = point(t, 1) - a, e2 = point(t, 2) - a,
           h = cross(d, e2);
      double det = dot(e1, h),
             scale = std::sqrt(double(dot(e1, e1)) * dot(e2, e2));
      if (std::abs(det) < 1e-10 * scale)
        continue;
      double inv = 1 / det;
      auto s = p - a, q = cross(s, e1);
      double u = dot(s, h) * inv, v = dot(d, q) * inv,
             distance = dot(e2, q) * inv;
      if (u < 0 || v < 0 || u + v > 1 || distance < 0 || distance > best)
        continue;
      best = distance;
      triangle = t;
      bary = {float(1 - u - v), float(u), float(v)};
    }
  }
  void intersections(Vec3 p, Vec3 d, int n, std::vector<double> &hits,
                     bool &edge) const {
    auto &node = nodes[n];
    if (!node.box.ray(p, d))
      return;
    if (node.left >= 0) {
      intersections(p, d, node.left, hits, edge);
      intersections(p, d, node.right, hits, edge);
      return;
    }
    for (size_t i = node.start; i < node.start + node.count; i++) {
      auto t = order[i];
      auto a = point(t, 0), e1 = point(t, 1) - a, e2 = point(t, 2) - a;
      auto h = cross(d, e2);
      double det = dot(e1, h);
      double scale = std::sqrt(double(dot(e1, e1)) * dot(e2, e2));
      if (std::abs(det) < 1e-10 * scale)
        continue;
      double inv = 1 / det;
      auto s = p - a;
      double u = dot(s, h) * inv;
      auto q = cross(s, e1);
      double v = dot(d, q) * inv, tau = dot(e2, q) * inv;
      if (u < -1e-7 || v < -1e-7 || u + v > 1 + 1e-7 || tau <= 1e-8)
        continue;
      if (u < 1e-6 || v < 1e-6 || 1 - u - v < 1e-6)
        edge = true;
      hits.push_back(tau);
    }
  }
  bool inside(Vec3 p) const {
    static const Vec3 dirs[] = {{1, .37139067f, .5291772f},
                                {.314159f, 1, .271828f},
                                {.618033f, .414213f, 1}};
    int votes = 0;
    for (auto direction : dirs) {
      std::vector<double> hits;
      bool edge = false;
      intersections(p, normalized(direction), 0, hits, edge);
      std::sort(hits.begin(), hits.end());
      size_t count = 0;
      double previous = -INFINITY;
      for (double t : hits)
        if (t - previous > 1e-6 * std::max(1., std::abs(t))) {
          count++;
          previous = t;
        }
      bool result = count % 2;
      if (!edge)
        return result;
      votes += result;
    }
    return votes >= 2;
  }
};
} // namespace
struct SurfaceQuery::Impl {
  Mesh mesh;
  Bvh tree;
  Impl(const Mesh &m, const std::function<bool()> &fn)
      : mesh(m), tree(mesh, fn) {}
  SurfaceHit hit(uint32_t triangle, Vec3 position, std::array<float, 3> bary,
                 float distance) const {
    auto a = mesh.indices[triangle * 3], b = mesh.indices[triangle * 3 + 1],
         c = mesh.indices[triangle * 3 + 2];
    auto n = mesh.normals[a] * bary[0] + mesh.normals[b] * bary[1] +
             mesh.normals[c] * bary[2];
    if (length(n) < 1e-12f)
      n = cross(mesh.positions[b] - mesh.positions[a],
                mesh.positions[c] - mesh.positions[a]);
    return {triangle, position, normalized(n), bary, distance};
  }
};
SurfaceQuery::SurfaceQuery(const Mesh &m, const std::function<bool()> &fn) {
  cancel(fn);
  structural(m, fn);
  m.validate();
  impl_ = std::make_unique<Impl>(m, fn);
  cancel(fn);
}
SurfaceQuery::~SurfaceQuery() = default;
SurfaceQuery::SurfaceQuery(SurfaceQuery &&) noexcept = default;
SurfaceQuery &SurfaceQuery::operator=(SurfaceQuery &&) noexcept = default;
SurfaceHit SurfaceQuery::closest(Vec3 p) const { return closest(p, {}); }
SurfaceHit SurfaceQuery::closest(Vec3 p,
                                 const SurfaceQueryOptions &options) const {
  cancel(options.cancelled);
  if (!options.max_triangle_tests || options.max_triangle_tests > 2'000'000)
    throw std::invalid_argument("Invalid closest surface query budget");
  if (!impl_ || !finite(p) ||
      std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) > 1000)
    throw std::invalid_argument("Invalid closest-surface query");
  uint32_t triangle = UINT32_MAX;
  Vec3 position;
  size_t visited = 0;
  double d2 = impl_->tree.closest_hit(p, 0, INFINITY, triangle, position,
                                      options, visited);
  if (triangle == UINT32_MAX)
    throw std::runtime_error("No eligible source surface triangle");
  auto a = impl_->tree.point(triangle, 0), b = impl_->tree.point(triangle, 1),
       c = impl_->tree.point(triangle, 2);
  auto ab = b - a, ac = c - a, ap = position - a;
  auto dp = [](Vec3 x, Vec3 y) {
    return double(x.x) * y.x + double(x.y) * y.y + double(x.z) * y.z;
  };
  double aa = dp(ab, ab), cc = dp(ac, ac), xy = dp(ab, ac),
         denom = aa * cc - xy * xy;
  if (denom <= 0)
    throw std::runtime_error("Ill-conditioned closest triangle");
  double v = (cc * dp(ap, ab) - xy * dp(ap, ac)) / denom,
         w = (aa * dp(ap, ac) - xy * dp(ap, ab)) / denom;
  std::array<float, 3> bary = {float(std::max(0., 1 - v - w)),
                               float(std::max(0., v)), float(std::max(0., w))};
  float sum = bary[0] + bary[1] + bary[2];
  for (auto &f : bary)
    f /= sum;
  cancel(options.cancelled);
  return impl_->hit(triangle, position, bary, float(std::sqrt(d2)));
}
std::optional<SurfaceHit> SurfaceQuery::raycast(Vec3 origin, Vec3 direction,
                                                float maximum) const {
  if (!impl_ || !finite(origin) || !finite(direction) ||
      !std::isfinite(length(direction)) ||
      std::max({std::abs(origin.x), std::abs(origin.y), std::abs(origin.z)}) >
          1000 ||
      length(direction) < 1e-12f || !std::isfinite(maximum) || maximum < 0 ||
      maximum > 1000)
    throw std::invalid_argument("Invalid surface ray");
  direction = normalized(direction);
  double best = maximum;
  uint32_t triangle = UINT32_MAX;
  std::array<float, 3> bary{};
  impl_->tree.ray_hit(origin, direction, 0, best, triangle, bary);
  if (triangle == UINT32_MAX)
    return {};
  return impl_->hit(triangle, origin + direction * float(best), bary,
                    float(best));
}
TopologyReport inspect_mesh(const Mesh &m,
                            const std::function<bool()> &cancelled) {
  cancel(cancelled);
  structural(m, cancelled);
  Geometry g(m, cancelled);
  TopologyReport r;
  r.vertices = m.positions.size();
  r.triangles = m.indices.size() / 3;
  std::set<uint32_t> used;
  std::set<std::array<uint32_t, 3>> faces;
  Dsu components(r.triangles);
  struct Edge {
    size_t count = 0;
    int direction = 0;
    uint32_t face = 0;
  };
  std::map<std::pair<uint32_t, uint32_t>, Edge> edges;
  for (size_t t = 0; t < r.triangles; t++) {
    if (t % 512 == 0)
      cancel(cancelled);
    if (degenerate(m, t * 3))
      r.degenerate_triangles++;
    std::array<uint32_t, 3> f;
    for (int k = 0; k < 3; k++) {
      auto i = m.indices[t * 3 + k];
      used.insert(i);
      f[k] = g.ids[i];
    }
    auto sorted = f;
    std::sort(sorted.begin(), sorted.end());
    if (!faces.insert(sorted).second)
      r.duplicate_triangles++;
    for (int k = 0; k < 3; k++) {
      uint32_t a = f[k], b = f[(k + 1) % 3];
      auto key = std::minmax(a, b);
      auto &edge = edges[key];
      if (edge.count)
        components.join(edge.face, t);
      else
        edge.face = t;
      edge.count++;
      edge.direction += a < b ? 1 : -1;
    }
  }
  std::set<uint32_t> points, parts;
  for (auto i : used)
    points.insert(g.ids[i]);
  for (uint32_t i = 0; i < r.triangles; i++)
    parts.insert(components.root(i));
  r.geometric_vertices = points.size();
  r.unused_vertices = m.positions.size() - used.size();
  r.components = parts.size();
  for (auto &[k, e] : edges) {
    if (e.count == 1)
      r.boundary_edges++;
    else if (e.count != 2)
      r.nonmanifold_edges++;
    else if (e.direction != 0)
      r.inconsistent_edges++;
  }
  cancel(cancelled);
  return r;
}
CleanupResult cleanup_mesh(const Mesh &m, const CleanupOptions &o) {
  cancel(o.cancelled);
  structural(m, o.cancelled);
  CleanupResult out;
  std::set<std::array<std::vector<uint32_t>, 3>> faces;
  auto attributes = [&](uint32_t i) {
    auto p = m.positions[i], n = m.normals[i];
    std::vector<uint32_t> k = {bits(p.x), bits(p.y), bits(p.z),
                               bits(n.x), bits(n.y), bits(n.z)};
    if (!m.uv.empty()) {
      k.push_back(bits(m.uv[i][0]));
      k.push_back(bits(m.uv[i][1]));
    }
    if (!m.uv_pins.empty())
      k.push_back(m.uv_pins[i]);
    return k;
  };
  std::map<std::vector<uint32_t>, uint32_t> vertices;
  for (size_t t = 0; t < m.indices.size(); t += 3) {
    if (t % 1536 == 0)
      cancel(o.cancelled);
    if (degenerate(m, t) && o.remove_degenerate) {
      out.removed_degenerate++;
      continue;
    }
    std::array<std::vector<uint32_t>, 3> face;
    for (int k = 0; k < 3; k++) {
      face[k] = attributes(m.indices[t + k]);
      if (!m.uv_seams.empty())
        face[k].push_back(m.uv_seams[t + k]);
    }
    std::sort(face.begin(), face.end());
    if (!faces.insert(face).second && o.remove_duplicates) {
      out.removed_duplicates++;
      continue;
    }
    out.source_triangles.push_back(t / 3);
    for (int k = 0; k < 3; k++) {
      auto i = m.indices[t + k];
      auto p = m.positions[i], n = m.normals[i];
      std::vector<uint32_t> keyv = {bits(p.x), bits(p.y), bits(p.z),
                                    bits(n.x), bits(n.y), bits(n.z)};
      if (!m.uv.empty()) {
        keyv.push_back(bits(m.uv[i][0]));
        keyv.push_back(bits(m.uv[i][1]));
      }
      if (!m.uv_pins.empty())
        keyv.push_back(m.uv_pins[i]);
      if (!o.merge_identical_vertices)
        keyv.push_back(i);
      auto [it, added] = vertices.emplace(std::move(keyv),
                                          uint32_t(out.mesh.positions.size()));
      if (added) {
        out.source_vertices.push_back(i);
        out.mesh.positions.push_back(p);
        out.mesh.normals.push_back(n);
        if (!m.uv.empty())
          out.mesh.uv.push_back(m.uv[i]);
        if (!m.uv_pins.empty())
          out.mesh.uv_pins.push_back(m.uv_pins[i]);
      }
      out.mesh.indices.push_back(it->second);
      if (!m.uv_seams.empty())
        out.mesh.uv_seams.push_back(m.uv_seams[t + k]);
    }
  }
  if (out.mesh.indices.empty())
    throw std::runtime_error("Cleanup removed the entire surface");
  out.removed_vertices = m.positions.size() - out.mesh.positions.size();
  out.mesh.validate();
  cancel(o.cancelled);
  return out;
}
Volume mesh_to_volume(const Mesh &m, const VoxelizeOptions &o) {
  cancel(o.cancelled);
  if (!std::isfinite(o.voxel_size) || o.voxel_size < .00001f ||
      o.voxel_size > 1 || !o.max_grid_points || o.max_grid_points > 4'000'000)
    throw std::invalid_argument("Invalid voxelization limits");
  auto topology = inspect_mesh(m, o.cancelled);
  if (!topology.closed_manifold())
    throw std::runtime_error("Voxelization requires closed consistently "
                             "oriented manifold triangles");
  Bvh tree(m, o.cancelled);
  auto box = tree.nodes[0].box;
  float margin = o.voxel_size * 4;
  Int3 lo, hi;
  float low[] = {box.lo.x, box.lo.y, box.lo.z},
        high[] = {box.hi.x, box.hi.y, box.hi.z};
  int *ls[] = {&lo.x, &lo.y, &lo.z}, *hs[] = {&hi.x, &hi.y, &hi.z};
  uint64_t grid = 1;
  for (int k = 0; k < 3; k++) {
    if (low[k] < -100 || high[k] > 100 || low[k] - margin < -101 ||
        high[k] + margin > 101)
      throw std::runtime_error("Imported surface exceeds sculpt workspace");
    *ls[k] = int(std::floor((low[k] - margin) / o.voxel_size));
    *hs[k] = int(std::ceil((high[k] + margin) / o.voxel_size));
    uint64_t n = int64_t(*hs[k]) - *ls[k] + 1;
    if (n > o.max_grid_points || grid > o.max_grid_points / n)
      throw std::length_error(
          "Voxelization exceeds grid budget; use a coarser resolution");
    grid *= n;
  }
  Volume::Voxels values;
  size_t visited = 0;
  double surface_epsilon = double(o.voxel_size) * o.voxel_size * 1e-12;
  for (int z = lo.z; z <= hi.z; z++)
    for (int y = lo.y; y <= hi.y; y++)
      for (int x = lo.x; x <= hi.x; x++) {
        if (visited++ % 128 == 0)
          cancel(o.cancelled);
        Vec3 p{x * o.voxel_size, y * o.voxel_size, z * o.voxel_size};
        double d2 = tree.nearest(p, 0, INFINITY);
        float d = d2 <= surface_epsilon
                      ? 0
                      : float(std::sqrt(d2)) * (tree.inside(p) ? -1 : 1);
        d = std::clamp(d, -margin, margin);
        if (d < margin)
          values.emplace(Int3{x, y, z}, d);
      }
  cancel(o.cancelled);
  return Volume::from_samples(o.voxel_size, std::move(values));
}
} // namespace dome
