// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh-processing.hpp"
#include "affine-query-metric.hpp"
#include "packed-material-query.hpp"
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
  // Degeneracy is geometric, independent of metre scale. A fixed squared-area
  // threshold incorrectly removes valid fine-resolution faces. Match
  // Mesh::validate with double differences and actual collinearity.
  const double ab[]={double(b.x)-a.x,double(b.y)-a.y,double(b.z)-a.z};
  const double ac[]={double(c.x)-a.x,double(c.y)-a.y,double(c.z)-a.z};
  return ab[1]*ac[2]-ab[2]*ac[1]==0 && ab[2]*ac[0]-ab[0]*ac[2]==0 &&
         ab[0]*ac[1]-ab[1]*ac[0]==0;
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
    double x = std::max({double(lo.x) - p.x, 0., double(p.x) - hi.x}),
           y = std::max({double(lo.y) - p.y, 0., double(p.y) - hi.y}),
           z = std::max({double(lo.z) - p.z, 0., double(p.z) - hi.z});
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
      double a = (double(ls[k]) - ps[k]) / ds[k], b = (double(hs[k]) - ps[k]) / ds[k];
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
// Dominant-axis shear/edge tests adapted from Blender 5.4 Alpha's
// isect_ray_tri_watertight_v3[_precalc] (GPL-2.0-or-later), pinned in
// third_party/blender/math_geom.cc.reference. Double intermediates retain
// identical shared-edge expressions, avoiding holes between adjacent faces.
struct SurfaceRay {
  Vec3 origin;
  int x, y, z;
  double sx, sy, sz;
  SurfaceRay(Vec3 p, Vec3 direction) : origin(p) {
    float d[] = {direction.x,direction.y,direction.z};
    z = std::abs(d[0]) > std::abs(d[1]) ? 0 : 1;
    if (std::abs(d[2]) > std::abs(d[z])) z = 2;
    x = (z+1)%3; y = (x+1)%3;
    if (d[z] < 0) std::swap(x,y);
    sz = 1. / d[z]; sx = d[x]*sz; sy = d[y]*sz;
  }
  bool hit(Vec3 a, Vec3 b, Vec3 c, double maximum, double &distance,
            std::array<float,3> &bary) const {
    auto shear = [&](Vec3 v) {
      double p[] = {double(v.x)-origin.x,double(v.y)-origin.y,double(v.z)-origin.z};
      return std::array<double,3>{p[x]-sx*p[z],p[y]-sy*p[z],p[z]};
    };
    auto av=shear(a),bv=shear(b),cv=shear(c);
    double u=cv[0]*bv[1]-cv[1]*bv[0], v=av[0]*cv[1]-av[1]*cv[0],
           w=bv[0]*av[1]-bv[1]*av[0], det=u+v+w;
    if (((u<0 || v<0 || w<0) && (u>0 || v>0 || w>0)) || !det || !std::isfinite(det))
      return false;
    distance=(u*av[2]+v*bv[2]+w*cv[2])*sz/det;
    if (!std::isfinite(distance) || distance < 0 || distance > maximum) return false;
    bary={float(u/det),float(v/det),float(w/det)};
    return true;
  }
};
using PrecisePoint = std::array<double, 3>;
PrecisePoint difference(Vec3 a, Vec3 b) {
  return {double(a.x)-b.x, double(a.y)-b.y, double(a.z)-b.z};
}
double precise_dot(const PrecisePoint &a, const PrecisePoint &b) {
  return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
PrecisePoint precise_cross(const PrecisePoint &a, const PrecisePoint &b) {
  return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2],
          a[0]*b[1]-a[1]*b[0]};
}
struct ClosestPoint {
  Vec3 position;
  std::array<float,3> bary;
  double distance2, planar_distance2;
};
// Project onto the plane, then test its interior or the three closed segments.
// Cross products avoid the cancellation in |AB|²|AC|²-(AB.AC)² for valid thin
// faces. Keep the actual barycentrics: recovering them from the rounded output
// point can lose a seam side or collapse an interior hit onto an edge.
ClosestPoint closest(Vec3 p, Vec3 a, Vec3 b, Vec3 c) {
  const auto ab=difference(b,a), ac=difference(c,a), ap=difference(p,a);
  const auto n=precise_cross(ab,ac);
  const double area2=precise_dot(n,n);
  if(!(area2>0))throw std::runtime_error("Degenerate closest triangle");
  const double plane_factor=precise_dot(ap,n)/area2;
  PrecisePoint projection;
  for(size_t k=0;k<3;++k)projection[k]=ap[k]-n[k]*plane_factor;
  auto candidate=[&](const std::array<double,3> &bary) {
    PrecisePoint q,delta,planar_delta;
    for(size_t k=0;k<3;++k) {
      q[k]=ab[k]*bary[1]+ac[k]*bary[2];
      delta[k]=ap[k]-q[k];
      planar_delta[k]=projection[k]-q[k];
    }
    return ClosestPoint{{float(double(a.x)+q[0]),float(double(a.y)+q[1]),float(double(a.z)+q[2])},
      {float(bary[0]),float(bary[1]),float(bary[2])},precise_dot(delta,delta),precise_dot(planar_delta,planar_delta)};
  };
  const double v=precise_dot(precise_cross(ap,ac),n)/area2;
  const double w=precise_dot(precise_cross(ab,ap),n)/area2;
  const double u=1-v-w;
  if(u>=0&&v>=0&&w>=0)return candidate({u,v,w});
  const std::array<Vec3,3> vertices{a,b,c};
  ClosestPoint best{{},{},INFINITY,INFINITY};
  for(size_t i=0;i<3;++i) {
    const size_t j=(i+1)%3;
    const auto edge=difference(vertices[j],vertices[i]);
    const double t=std::clamp(precise_dot(difference(p,vertices[i]),edge)/precise_dot(edge,edge),0.,1.);
    std::array<double,3> bary{};bary[i]=1-t;bary[j]=t;
    const auto hit=candidate(bary);
    // Every edge shares the same plane-normal distance. Comparing totals can
    // round distinct tiny in-plane distances to a tie for an off-plane query.
    if(hit.planar_distance2<best.planar_distance2)best=hit;
  }
  return best;
}
// An optional private material-policy proof over immutable BVH triangle ID
// bounds. Standalone SurfaceQuery never supplies it and retains exact legacy
// traversal/budget behavior. Spatial bounds and precise hit math are unchanged.
struct TraversalEligibility {
 std::span<const std::array<uint32_t,2>> bounds;
 uint32_t first;
 const detail::PackedMaterialQuery::Eligibility &eligible;
 bool allows(size_t node)const {const auto b=bounds[node];return eligible(first+b[0],b[1]-b[0]+1);}
};
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
  Bvh(const Mesh &mesh, std::function<bool()> fn,size_t reserved_nodes=0)
      : m(mesh), cancelled(std::move(fn)) {
    if(reserved_nodes)nodes.reserve(reserved_nodes);
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
        best = std::min(best, q.distance2);
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
                     Vec3 &position, std::array<float,3> &bary,
                     const SurfaceQueryOptions &options,
                     size_t &visited, bool stable_ties=false,const TraversalEligibility*eligibility=nullptr) const {
    cancel(options.cancelled);
    auto &node = nodes[n];
    if (node.box.distance2(p) > best)
      return best;
    if(eligibility&&!eligibility->allows(size_t(n)))return best;
    if (node.left < 0) {
      for (size_t i = node.start; i < node.start + node.count; i++) {
        if (++visited > options.max_triangle_tests)
          throw std::length_error(
              "Closest surface triangle work budget exceeded");
        auto t = order[i];
        if (options.triangle_filter && !options.triangle_filter(t))
          continue;
        auto q = closest(p, point(t, 0), point(t, 1), point(t, 2));
        const double distance = q.distance2;
        if (distance < best || (stable_ties&&distance==best&&(triangle==UINT32_MAX||t<triangle))) {
          if(options.closest_filter&&!options.closest_filter(t,q.bary))continue;
          best = distance;
          triangle = t;
          position = q.position;
          bary = q.bary;
        }
      }
      return best;
    }
    int a = node.left, b = node.right;
    if (nodes[a].box.distance2(p) > nodes[b].box.distance2(p))
      std::swap(a, b);
    best = closest_hit(p, a, best, triangle, position, bary, options, visited, stable_ties, eligibility);
    return closest_hit(p, b, best, triangle, position, bary, options, visited, stable_ties, eligibility);
  }
  void ray_hit(Vec3 p, Vec3 d, const SurfaceRay &ray, int n, double &best, uint32_t &triangle,
               std::array<float, 3> &bary, const SurfaceQueryOptions &options, size_t &visited, bool stable_ties=false,const TraversalEligibility*eligibility=nullptr) const {
    cancel(options.cancelled);
    auto &node = nodes[n];
    if (!node.box.ray(p, d))
      return;
    if(eligibility&&!eligibility->allows(size_t(n)))return;
    if (node.left >= 0) {
      ray_hit(p, d, ray, node.left, best, triangle, bary, options, visited, stable_ties, eligibility);
      ray_hit(p, d, ray, node.right, best, triangle, bary, options, visited, stable_ties, eligibility);
      return;
    }
    for (size_t i = node.start; i < node.start + node.count; i++) {
      if (++visited > options.max_triangle_tests) throw std::length_error("Surface ray triangle budget exceeded");
      auto t = order[i];
      if (options.triangle_filter && !options.triangle_filter(t)) continue;
      double distance;
      std::array<float,3> coordinates;
      if (!ray.hit(point(t,0),point(t,1),point(t,2),best,distance,coordinates))
        continue;
      if (options.ray_filter && !options.ray_filter(t, coordinates)) continue;
      if(stable_ties&&distance==best&&triangle!=UINT32_MAX&&t>triangle)continue;
      best = distance;
      triangle = t;
      bary = coordinates;
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
SurfaceHit packed_query_hit(const Mesh&mesh,uint32_t triangle,Vec3 position,std::array<float,3>bary,float distance){
    auto a = mesh.indices[triangle * 3], b = mesh.indices[triangle * 3 + 1],
         c = mesh.indices[triangle * 3 + 2];
    auto n = mesh.normals[a] * bary[0] + mesh.normals[b] * bary[1] +
             mesh.normals[c] * bary[2];
    if (length(n) < 1e-12f) {
      // The triangle was validated in double precision. Its geometric normal
      // can be much smaller than normalized(Vec3)'s fallback threshold (or
      // underflow when squared in float), without being degenerate.
      const auto geometric=precise_cross(difference(mesh.positions[b],mesh.positions[a]),
                                        difference(mesh.positions[c],mesh.positions[a]));
      const double magnitude=std::sqrt(precise_dot(geometric,geometric));
      n={float(geometric[0]/magnitude),float(geometric[1]/magnitude),
         float(geometric[2]/magnitude)};
    }
    return {triangle, position, normalized(n), bary, distance};
}
} // namespace
struct SurfaceQuery::Impl {
  // Public construction retains its old defensive Mesh copy. Only the sealed
  // private placed-chunk factory can share an actually const geometry owner.
  Mesh copied;
  std::shared_ptr<const Mesh> immutable;
  const Mesh &mesh;
  Bvh tree;
  Impl(const Mesh &m, const std::function<bool()> &fn)
      : copied(m),mesh(copied),tree(mesh, fn) {
    // Construction callbacks may own worker/model state. The completed BVH
    // never uses this build-only callback during later queries.
    tree.cancelled={};
  }
  Impl(std::shared_ptr<const Mesh> m,const std::function<bool()> &fn,size_t nodes)
      : immutable(std::move(m)),mesh(*immutable),
        tree(mesh,fn?std::function<bool()>(std::cref(fn)):std::function<bool()>{},nodes) {
    // Build-only borrowed callback. Never retain a model/Scene callback or a
    // reference to a completed worker operation in the immutable query owner.
    tree.cancelled={};
  }
  SurfaceHit hit(uint32_t triangle, Vec3 position, std::array<float, 3> bary,
                 float distance) const {
    return packed_query_hit(mesh,triangle,position,bary,distance);
  }
};
SurfaceQuery::SurfaceQuery(const Mesh &m, const std::function<bool()> &fn) {
  cancel(fn);
  structural(m, fn);
  m.validate();
  impl_ = std::make_unique<Impl>(m, fn);
  cancel(fn);
}
namespace {
size_t chunk_bvh_nodes(size_t triangles) {
  if(triangles<=8)return 1;
  auto a=chunk_bvh_nodes(triangles/2),b=chunk_bvh_nodes(triangles-triangles/2);
  if(b==SIZE_MAX||a>SIZE_MAX-b-1)throw std::length_error("Chunk BVH node count overflow");
  return 1+a+b;
}
}
size_t SurfaceQuery::immutable_working_bytes(const Mesh &mesh) {
  const auto triangles=mesh.indices.size()/3;
  if(!triangles||mesh.indices.size()%3)throw std::invalid_argument("Invalid immutable query triangle count");
  size_t bytes=sizeof(Impl)+sizeof(SurfaceQuery)+256;
  auto charge=[&](size_t n,size_t width){
    if(n>(SIZE_MAX-bytes)/width)throw std::length_error("Immutable query byte count overflow");
    bytes+=n*width;
  };
  charge(triangles,sizeof(uint32_t));charge(chunk_bvh_nodes(triangles),sizeof(Bvh::Node));
  return bytes;
}
SurfaceQuery::SurfaceQuery(ImmutableGeometry,std::shared_ptr<const Mesh> mesh,
                           const std::function<bool()> &fn) {
  cancel(fn);
  if(!mesh)throw std::invalid_argument("Missing immutable query geometry owner");
  structural(*mesh,fn);mesh->validate();
  const auto nodes=chunk_bvh_nodes(mesh->indices.size()/3);
  impl_=std::make_unique<Impl>(std::move(mesh),fn,nodes);cancel(fn);
}
SurfaceQuery::~SurfaceQuery() = default;
const Mesh &SurfaceQuery::mesh() const {
  if (!impl_) throw std::logic_error("Moved-from surface query");
  return impl_->mesh;
}
SurfaceQuery::SurfaceQuery(SurfaceQuery &&) noexcept = default;
SurfaceQuery &SurfaceQuery::operator=(SurfaceQuery &&) noexcept = default;
SurfaceHit SurfaceQuery::closest(Vec3 p) const { return closest(p, {}); }
SurfaceHit SurfaceQuery::closest(Vec3 p,
                                 const SurfaceQueryOptions &options) const {
  auto result=nearest(p,options);
  if(!result)throw std::runtime_error("No eligible source surface triangle");
  return *result;
}
std::optional<SurfaceHit> SurfaceQuery::nearest(Vec3 p,const SurfaceQueryOptions &options) const {
  const auto result=nearest_result(p,options);return result?std::optional<SurfaceHit>(result->surface):std::nullopt;
}
std::optional<SurfaceQueryResult> SurfaceQuery::nearest_result(Vec3 p,const SurfaceQueryOptions &options) const {
  cancel(options.cancelled);
  if (!options.max_triangle_tests || options.max_triangle_tests > 2'000'000)
    throw std::invalid_argument("Invalid closest surface query budget");
  if (!impl_ || !finite(p) ||
      std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) > 1000)
    throw std::invalid_argument("Invalid closest-surface query");
  if(std::isnan(options.closest_maximum)||options.closest_maximum<=0)throw std::invalid_argument("Invalid closest surface range");
  uint32_t triangle = UINT32_MAX;
  Vec3 position;
  std::array<float,3> bary{};
  size_t visited = 0;
  const double maximum=std::nextafter(double(options.closest_maximum)*options.closest_maximum,INFINITY);
  double d2 = impl_->tree.closest_hit(p, 0, maximum, triangle, position,
                                      bary, options, visited);
  if (triangle == UINT32_MAX)
    return {};
  cancel(options.cancelled);
  return SurfaceQueryResult{impl_->hit(triangle,position,bary,float(std::sqrt(d2))),d2};
}
std::optional<SurfaceHit> SurfaceQuery::raycast(Vec3 origin, Vec3 direction,
                                                float maximum) const {
  return raycast(origin, direction, maximum, {});
}
std::optional<SurfaceHit> SurfaceQuery::raycast(Vec3 origin,Vec3 direction,float maximum,const SurfaceQueryOptions &options) const {
  const auto result=raycast_result(origin,direction,maximum,options);return result?std::optional<SurfaceHit>(result->surface):std::nullopt;
}
std::optional<SurfaceQueryResult> SurfaceQuery::raycast_result(Vec3 origin, Vec3 direction,
                                                float maximum, const SurfaceQueryOptions &options) const {
  cancel(options.cancelled);
  if (!options.max_triangle_tests) throw std::invalid_argument("Empty surface ray budget");
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
  size_t visited = 0;
  impl_->tree.ray_hit(origin, direction, SurfaceRay(origin,direction), 0, best, triangle, bary, options, visited);
  if (triangle == UINT32_MAX)
    return {};
  return SurfaceQueryResult{impl_->hit(triangle,origin+direction*float(best),bary,float(best)),best*best};
}
std::optional<SurfaceQueryResult> SurfaceQuery::nearest_result(Vec3 point,const AffineTransform &world,
    const SurfaceQueryOptions &options) const {
  cancel(options.cancelled);
  if(!impl_||!finite(point)||std::max({std::abs(point.x),std::abs(point.y),std::abs(point.z)})>1000||
     !options.max_triangle_tests||options.max_triangle_tests>2'000'000||std::isnan(options.closest_maximum)||options.closest_maximum<=0)
    throw std::invalid_argument("Invalid affine closest-surface query");
  const auto inverse=world.inverse();const auto local=inverse.point(detail::query_point(point));
  double best=std::nextafter(double(options.closest_maximum)*options.closest_maximum,INFINITY);uint32_t triangle=UINT32_MAX;
  std::array<float,3> bary{};Vec3 position;size_t visited=0;
  auto distance=[&](const Bvh::Node &node){return detail::metric_bounds_distance(node.box.lo,node.box.hi,local,world);};
  const auto walk=[&](auto &self,int index)->void {
    cancel(options.cancelled);const auto &node=impl_->tree.nodes[index];if(distance(node)>best)return;
    if(node.left>=0){auto a=node.left,b=node.right;if(distance(impl_->tree.nodes[a])>distance(impl_->tree.nodes[b]))std::swap(a,b);self(self,a);self(self,b);return;}
    for(size_t i=node.start;i<node.start+node.count;++i){
      if(++visited>options.max_triangle_tests)throw std::length_error("Affine closest triangle work budget exceeded");
      const auto t=impl_->tree.order[i];if(options.triangle_filter&&!options.triangle_filter(t))continue;
      const auto a=detail::query_point(impl_->tree.point(t,0)),b=detail::query_point(impl_->tree.point(t,1)),c=detail::query_point(impl_->tree.point(t,2));
      const auto ap=world.direction(detail::query_sub(local,a)),ab=world.direction(detail::query_sub(b,a)),ac=world.direction(detail::query_sub(c,a));
      const auto hit=detail::metric_closest(ap,ab,ac);
      if(hit.distance2<best||(hit.distance2==best&&t<triangle)){
        if(options.closest_filter&&!options.closest_filter(t,hit.bary))continue;
        best=hit.distance2;triangle=t;bary=hit.bary;
        position={float(double(point.x)-ap[0]+hit.relative[0]),float(double(point.y)-ap[1]+hit.relative[1]),float(double(point.z)-ap[2]+hit.relative[2])};
      }
    }
  };
  walk(walk,0);if(triangle==UINT32_MAX)return {};cancel(options.cancelled);
  return SurfaceQueryResult{SurfaceHit{triangle,position,detail::metric_normal(impl_->mesh,triangle,bary,inverse),bary,float(std::sqrt(best))},best};
}
std::optional<SurfaceQueryResult> SurfaceQuery::raycast_result(Vec3 origin,Vec3 direction,float maximum,
    const AffineTransform &world,const SurfaceQueryOptions &options) const {
  cancel(options.cancelled);
  if(!impl_||!finite(origin)||!finite(direction)||!std::isfinite(length(direction))||length(direction)<1e-12f||
     std::max({std::abs(origin.x),std::abs(origin.y),std::abs(origin.z)})>1000||!std::isfinite(maximum)||maximum<0||maximum>1000||!options.max_triangle_tests)
    throw std::invalid_argument("Invalid affine surface ray");
  const auto inverse=world.inverse();auto d=detail::query_point(direction);const auto magnitude=std::hypot(d[0],d[1],d[2]);for(double &v:d)v/=magnitude;
  const detail::MetricRay ray(inverse.point(detail::query_point(origin)),inverse.direction(d));
  double best=maximum;uint32_t triangle=UINT32_MAX;std::array<float,3>bary{};size_t visited=0;
  const auto walk=[&](auto &self,int index)->void {
    cancel(options.cancelled);const auto &node=impl_->tree.nodes[index];if(!ray.box(node.box.lo,node.box.hi,best))return;
    if(node.left>=0){self(self,node.left);self(self,node.right);return;}
    for(size_t i=node.start;i<node.start+node.count;++i){
      if(++visited>options.max_triangle_tests)throw std::length_error("Affine ray triangle work budget exceeded");
      const auto t=impl_->tree.order[i];if(options.triangle_filter&&!options.triangle_filter(t))continue;
      double distance;std::array<float,3>coordinates{};
      if(!ray.hit(impl_->tree.point(t,0),impl_->tree.point(t,1),impl_->tree.point(t,2),best,distance,coordinates))continue;
      if(distance==best&&triangle!=UINT32_MAX&&t>triangle)continue;
      if(options.ray_filter&&!options.ray_filter(t,coordinates))continue;
      best=distance;triangle=t;bary=coordinates;
    }
  };
  walk(walk,0);if(triangle==UINT32_MAX)return {};cancel(options.cancelled);
  Vec3 position{float(double(origin.x)+d[0]*best),float(double(origin.y)+d[1]*best),float(double(origin.z)+d[2]*best)};
  return SurfaceQueryResult{SurfaceHit{triangle,position,detail::metric_normal(impl_->mesh,triangle,bary,inverse),bary,float(best)},best*best};
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

namespace detail {
namespace {
size_t packed_sum(size_t a,size_t n,size_t width=1){if(width&&n>(SIZE_MAX-a)/width)throw std::length_error("Packed query byte forecast overflow");return a+n*width;}
size_t packed_mesh_bytes(const Mesh&m){size_t n=0;n=packed_sum(n,m.positions.size(),sizeof(Vec3));n=packed_sum(n,m.normals.size(),sizeof(Vec3));n=packed_sum(n,m.uv.size(),sizeof(std::array<float,2>));n=packed_sum(n,m.indices.size(),sizeof(uint32_t));n=packed_sum(n,m.uv_pins.size());return packed_sum(n,m.uv_seams.size());}
void validate_packed_original(const Mesh&m,const std::function<bool()>&fn){
 if(m.positions.empty()||m.indices.empty()||m.indices.size()%3||m.indices.size()/3>UINT32_MAX)throw std::invalid_argument("Invalid original packed query size");
 if(m.normals.size()!=m.positions.size()||(!m.uv.empty()&&m.uv.size()!=m.positions.size())||(!m.uv_pins.empty()&&m.uv_pins.size()!=m.positions.size())||(!m.uv_seams.empty()&&m.uv_seams.size()!=m.indices.size()))throw std::invalid_argument("Invalid original packed query attributes");
 for(size_t v=0;v<m.positions.size();++v){if((v&511)==0)cancel(fn);const auto p=m.positions[v];if(std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>100)throw std::invalid_argument("Mesh outside 100m authoring workspace");if(!finite(p)||!finite(m.normals[v])||!std::isfinite(length(m.normals[v])))throw std::invalid_argument("Nonfinite original packed query vertex");if(!m.uv.empty()&&(!std::isfinite(m.uv[v][0])||!std::isfinite(m.uv[v][1])))throw std::invalid_argument("Nonfinite original packed query UV");if(!m.uv_pins.empty()&&m.uv_pins[v]>1)throw std::invalid_argument("Invalid original packed query pin");}
 for(size_t c=0;c<m.indices.size();c+=3){if(c%1536==0)cancel(fn);for(size_t k=0;k<3;++k){if(m.indices[c+k]>=m.positions.size())throw std::invalid_argument("Original packed query index out of range");if(!m.uv_seams.empty()&&m.uv_seams[c+k]>1)throw std::invalid_argument("Invalid original packed query seam");}const auto a=m.positions[m.indices[c]],b=m.positions[m.indices[c+1]],d=m.positions[m.indices[c+2]];const auto cross=precise_cross(difference(b,a),difference(d,a));if(cross[0]==0&&cross[1]==0&&cross[2]==0)throw std::invalid_argument("Degenerate original packed query face");}
 cancel(fn);
}
std::vector<uint32_t> packed_references(const Mesh&m,size_t first,size_t triangles,const std::function<bool()>&fn){
 std::vector<uint32_t>r;r.reserve(triangles*3);for(size_t c=0;c<triangles*3;++c){if((c&1023)==0)cancel(fn);r.push_back(m.indices[first*3+c]);}size_t comparisons=0;std::sort(r.begin(),r.end(),[&](uint32_t a,uint32_t b){if((++comparisons&2047)==0)cancel(fn);return a<b;});r.erase(std::unique(r.begin(),r.end()),r.end());cancel(fn);return r;
}
}
struct PackedMaterialQuery::Impl {
 struct Plan {size_t first,triangles,vertices,nodes;};
 struct Part {
  Mesh mesh;uint32_t first;Bvh tree;std::vector<std::array<uint32_t,2>>intervals;
  Part(Mesh m,uint32_t offset,size_t nodes,const std::function<bool()>&fn):mesh(std::move(m)),first(offset),tree(mesh,fn?std::function<bool()>(std::cref(fn)):std::function<bool()>{},nodes){
   tree.cancelled={};intervals.reserve(nodes);intervals.resize(tree.nodes.size());
   for(size_t i=tree.nodes.size();i-->0;){if((i&511)==0)cancel(fn);const auto&n=tree.nodes[i];uint32_t lo=UINT32_MAX,hi=0;
    if(n.left<0){for(size_t k=n.start;k<n.start+n.count;++k){lo=std::min(lo,tree.order[k]);hi=std::max(hi,tree.order[k]);}}
    else{lo=std::min(intervals[size_t(n.left)][0],intervals[size_t(n.right)][0]);hi=std::max(intervals[size_t(n.left)][1],intervals[size_t(n.right)][1]);}
    intervals[i]={lo,hi};
   }cancel(fn);
  }
 };
 std::unique_ptr<SurfaceQuery>small;
 Mesh original;std::vector<std::unique_ptr<Part>>parts;Usage used;
 Impl(const Mesh&m,const std::function<bool()>&fn,Limits limits,size_t metadata){
  cancel(fn);if(!limits.retained_bytes||!limits.working_bytes||!limits.partitions||limits.partitions>128||!limits.part_triangles||limits.part_triangles>262144||!limits.nodes||limits.nodes>4194304)throw std::invalid_argument("Invalid packed query resource limits");
  const auto source_bytes=packed_mesh_bytes(m);size_t retained=packed_sum(16384,source_bytes);retained=packed_sum(retained,metadata);
  if(retained>limits.retained_bytes||retained>limits.working_bytes)throw std::length_error("Original packed query payload/metadata exceeds byte budget before copy");
  if(m.positions.size()<=2'000'000&&m.indices.size()/3<=2'000'000){
   const auto n=m.indices.size()/3;const auto nodes=n?chunk_bvh_nodes(n):0;
   // The unchanged legacy vector grows geometrically; include its retained
   // capacity and concurrent old/new node storage, not only live node count.
   retained=packed_sum(retained,n,sizeof(uint32_t));retained=packed_sum(retained,nodes,2*sizeof(Bvh::Node));const auto working=packed_sum(retained,nodes,sizeof(Bvh::Node));
   if(nodes>limits.nodes||retained>limits.retained_bytes||working>limits.working_bytes)throw std::length_error("Legacy material query exceeds prospective byte/node budget");
   small=std::make_unique<SurfaceQuery>(m,fn);used={retained,working,1,nodes,m.positions.size(),false};return;
  }
  validate_packed_original(m,fn);const size_t triangles=m.indices.size()/3,count=(triangles+limits.part_triangles-1)/limits.part_triangles;
  if(count>limits.partitions)throw std::length_error("Packed query partition budget exceeded before copy");
  retained=packed_sum(retained,count,16384);if(retained>limits.retained_bytes||retained>limits.working_bytes)throw std::length_error("Packed query plan metadata exceeds prospective byte budget");std::vector<Plan>plan;plan.reserve(count);size_t all_nodes=0,vertices=0,peak_refs=0;
  // Plan one bounded ref list at a time; no input-vertex-sized remap exists.
  for(size_t first=0;first<triangles;){cancel(fn);const auto n=std::min(limits.part_triangles,triangles-first);if(packed_sum(retained,n*3,sizeof(uint32_t))>limits.working_bytes)throw std::length_error("Packed query planning scratch exceeds byte budget before allocation");const auto refs=packed_references(m,first,n,fn);const auto nodes=chunk_bvh_nodes(n);if(nodes>limits.nodes-all_nodes)throw std::length_error("Packed query total BVH node budget exceeded");all_nodes+=nodes;vertices=packed_sum(vertices,refs.size());retained=packed_sum(retained,refs.size(),2*sizeof(Vec3));retained=packed_sum(retained,n,3*sizeof(uint32_t)+sizeof(uint32_t));retained=packed_sum(retained,nodes,sizeof(Bvh::Node)+sizeof(std::array<uint32_t,2>));peak_refs=std::max(peak_refs,n*3*sizeof(uint32_t));if(retained>limits.retained_bytes||packed_sum(retained,peak_refs)>limits.working_bytes)throw std::length_error("Packed query retained/build byte budget exceeded before source/part copies");plan.push_back({first,n,refs.size(),nodes});first+=n;}
  const auto working=packed_sum(retained,peak_refs);if(working>limits.working_bytes)throw std::length_error("Packed query build byte budget exceeded");
  original=m;parts.reserve(count);
  for(const auto&p:plan){cancel(fn);const auto refs=packed_references(m,p.first,p.triangles,fn);Mesh compact;compact.positions.reserve(p.vertices);compact.normals.reserve(p.vertices);compact.indices.reserve(p.triangles*3);for(size_t v=0;v<refs.size();++v){if((v&511)==0)cancel(fn);compact.positions.push_back(m.positions[refs[v]]);compact.normals.push_back(m.normals[refs[v]]);}for(size_t c=0;c<p.triangles*3;++c){if((c&1023)==0)cancel(fn);const auto index=m.indices[p.first*3+c];const auto found=std::lower_bound(refs.begin(),refs.end(),index);if(found==refs.end()||*found!=index)throw std::logic_error("Packed query reference plan lost a source vertex");compact.indices.push_back(uint32_t(found-refs.begin()));}structural(compact,fn);compact.validate();parts.push_back(std::make_unique<Part>(std::move(compact),uint32_t(p.first),p.nodes,fn));}
  used={retained,working,count,all_nodes,vertices,true};cancel(fn);
 }
 SurfaceQueryOptions mapped(const SurfaceQueryOptions&o,uint32_t first)const{
  SurfaceQueryOptions result;result.max_triangle_tests=o.max_triangle_tests;result.closest_maximum=o.closest_maximum;if(o.cancelled)result.cancelled=std::cref(o.cancelled);
  if(o.triangle_filter)result.triangle_filter=[&o,first](uint32_t t){return o.triangle_filter(first+t);};
  if(o.ray_filter)result.ray_filter=[&o,first](uint32_t t,const std::array<float,3>&b){return o.ray_filter(first+t,b);};
  if(o.closest_filter)result.closest_filter=[&o,first](uint32_t t,const std::array<float,3>&b){return o.closest_filter(first+t,b);};
  return result;
 }
};
PackedMaterialQuery::PackedMaterialQuery(const Mesh&m,const std::function<bool()>&fn):PackedMaterialQuery(m,fn,Limits{}){}
PackedMaterialQuery::PackedMaterialQuery(const Mesh&m,const std::function<bool()>&fn,Limits l,size_t metadata):impl_(std::make_unique<Impl>(m,fn,l,metadata)){}
PackedMaterialQuery::~PackedMaterialQuery()=default;
const Mesh&PackedMaterialQuery::mesh()const{return impl_->small?impl_->small->mesh():impl_->original;}
PackedMaterialQuery::Usage PackedMaterialQuery::usage()const noexcept{return impl_->used;}
SurfaceHit PackedMaterialQuery::closest(Vec3 p)const{return closest(p,{});}
SurfaceHit PackedMaterialQuery::closest(Vec3 p,const SurfaceQueryOptions&o,const Eligibility&eligible)const{const auto hit=nearest(p,o,eligible);if(!hit)throw std::runtime_error("No eligible source surface triangle");return *hit;}
std::optional<SurfaceHit>PackedMaterialQuery::nearest(Vec3 p,const SurfaceQueryOptions&o,const Eligibility&eligible)const{const auto hit=nearest_result(p,o,eligible);return hit?std::optional<SurfaceHit>(hit->surface):std::nullopt;}
std::optional<SurfaceQueryResult>PackedMaterialQuery::nearest_result(Vec3 p,const SurfaceQueryOptions&o,const Eligibility&eligible)const{
 if(impl_->small)return impl_->small->nearest_result(p,o);cancel(o.cancelled);
 if(!o.max_triangle_tests||o.max_triangle_tests>2'000'000)throw std::invalid_argument("Invalid closest surface query budget");if(!finite(p)||std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>1000)throw std::invalid_argument("Invalid closest-surface query");if(std::isnan(o.closest_maximum)||o.closest_maximum<=0)throw std::invalid_argument("Invalid closest surface range");
 double best=std::nextafter(double(o.closest_maximum)*o.closest_maximum,INFINITY);size_t visited=0;std::optional<SurfaceQueryResult>result;
 for(const auto&part:impl_->parts){auto options=impl_->mapped(o,part->first);const TraversalEligibility policy{part->intervals,part->first,eligible};uint32_t triangle=UINT32_MAX;Vec3 position;std::array<float,3>bary{};const auto distance=part->tree.closest_hit(p,0,best,triangle,position,bary,options,visited,true,eligible?&policy:nullptr);if(triangle==UINT32_MAX)continue;const auto global=part->first+triangle;if(result&&(distance>best||(distance==best&&global>=result->surface.triangle)))continue;auto hit=packed_query_hit(part->mesh,triangle,position,bary,float(std::sqrt(distance)));hit.triangle=global;best=distance;result=SurfaceQueryResult{hit,distance};}cancel(o.cancelled);return result;
}
std::optional<SurfaceHit>PackedMaterialQuery::raycast(Vec3 p,Vec3 d,float maximum,const SurfaceQueryOptions&o,const Eligibility&eligible)const{const auto hit=raycast_result(p,d,maximum,o,eligible);return hit?std::optional<SurfaceHit>(hit->surface):std::nullopt;}
std::optional<SurfaceQueryResult>PackedMaterialQuery::raycast_result(Vec3 p,Vec3 d,float maximum,const SurfaceQueryOptions&o,const Eligibility&eligible)const{
 if(impl_->small)return impl_->small->raycast_result(p,d,maximum,o);cancel(o.cancelled);if(!o.max_triangle_tests)throw std::invalid_argument("Empty surface ray budget");if(!finite(p)||!finite(d)||!std::isfinite(length(d))||std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>1000||length(d)<1e-12f||!std::isfinite(maximum)||maximum<0||maximum>1000)throw std::invalid_argument("Invalid surface ray");
 d=normalized(d);const SurfaceRay ray(p,d);double best=maximum;size_t visited=0;std::optional<SurfaceQueryResult>result;
 for(const auto&part:impl_->parts){auto options=impl_->mapped(o,part->first);const TraversalEligibility policy{part->intervals,part->first,eligible};uint32_t triangle=UINT32_MAX;std::array<float,3>bary{};double distance=best;part->tree.ray_hit(p,d,ray,0,distance,triangle,bary,options,visited,true,eligible?&policy:nullptr);if(triangle==UINT32_MAX)continue;const auto global=part->first+triangle;if(result&&(distance>best||(distance==best&&global>=result->surface.triangle)))continue;auto hit=packed_query_hit(part->mesh,triangle,p+d*float(distance),bary,float(distance));hit.triangle=global;best=distance;result=SurfaceQueryResult{hit,distance*distance};}cancel(o.cancelled);return result;
}
} // namespace detail

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
