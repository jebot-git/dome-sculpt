// SPDX-License-Identifier: GPL-3.0-or-later
#include "key-mirror-refinement.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
namespace dome::detail {
namespace {
struct V {
  double x, y, z;
};
V operator+(V a, V b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V operator-(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V operator*(V a, double b) { return {a.x * b, a.y * b, a.z * b}; }
double dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V cross(V a, V b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V reflected(V a, double offset = 0) { return {offset == 0 ? -a.x : 2 * offset - a.x, a.y, a.z}; }
V point(Vec3 a) { return {a.x, a.y, a.z}; }
struct P {
  double x, y;
};
P operator+(P a, P b) { return {a.x + b.x, a.y + b.y}; }
P operator-(P a, P b) { return {a.x - b.x, a.y - b.y}; }
P operator*(P a, double s) { return {a.x * s, a.y * s}; }
double det(P a, P b) { return a.x * b.y - a.y * b.x; }
constexpr double roundoff = 256 * std::numeric_limits<double>::epsilon();
struct Polygon {
  std::array<P, 8> p{};
  size_t count = 0;
};
double signed_area(const Polygon &p) {
  double a = 0;
  for (size_t i = 0; i < p.count; ++i)
    a += det(p.p[i], p.p[(i + 1) % p.count]);
  return a * .5;
}
Polygon positive(Polygon p) {
  if (signed_area(p) < 0)
    std::reverse(p.p.begin(), p.p.begin() + p.count);
  return p;
}
Polygon clip(Polygon p, const Polygon &bounds) {
  for (size_t e = 0; e < bounds.count && p.count; ++e) {
    Polygon next;
    const P a = bounds.p[e], b = bounds.p[(e + 1) % bounds.count];
    auto distance = [&](P q) { return det(b - a, q - a); };
    auto add = [&](P q) {
      if (next.count && q.x == next.p[next.count - 1].x && q.y == next.p[next.count - 1].y)
        return;
      if (next.count == next.p.size())
        throw std::invalid_argument("Mirror refinement clipping exceeded its genuine convex bound");
      next.p[next.count++] = q;
    };
    for (size_t i = 0; i < p.count; ++i) {
      const P q = p.p[i], r = p.p[(i + 1) % p.count];
      const double d = distance(q), f = distance(r);
      if (d >= 0)
        add(q);
      if ((d < 0 && f > 0) || (d > 0 && f < 0)) {
        const double t = d / (d - f);
        add(q + (r - q) * t);
      }
    }
    if (next.count > 1 && next.p[0].x == next.p[next.count - 1].x &&
        next.p[0].y == next.p[next.count - 1].y)
      --next.count;
    p = next;
  }
  return p;
}
struct Bounds {
  V low, high;
};
Bounds bounds(const std::array<V, 3> &v) {
  Bounds b{v[0], v[0]};
  for (auto p : v) {
    b.low = {std::min(b.low.x, p.x), std::min(b.low.y, p.y), std::min(b.low.z, p.z)};
    b.high = {std::max(b.high.x, p.x), std::max(b.high.y, p.y), std::max(b.high.z, p.z)};
  }
  return b;
}
bool overlaps(Bounds a, Bounds b, double t) {
  return a.low.x <= b.high.x + t && b.low.x <= a.high.x + t && a.low.y <= b.high.y + t &&
         b.low.y <= a.high.y + t && a.low.z <= b.high.z + t && b.low.z <= a.high.z + t;
}
struct Budget {
  const KeyMirrorLimits &limits;
  size_t work = 0, bytes = 0;
  void poll(size_t n = 1) {
    if (n > limits.max_work - work)
      throw std::length_error("Mirror refinement exceeds bounded work");
    work += n;
    if (limits.cancelled && limits.cancelled())
      throw std::runtime_error("Mirror refinement cancelled");
  }
  void charge(size_t n) {
    if (n > limits.max_bytes - bytes)
      throw std::length_error("Mirror refinement exceeds prospective scratch/output quota");
    bytes += n;
  }
  template <class T> void grow(std::vector<T> &v, size_t needed) {
    if (needed <= v.capacity())
      return;
    const size_t capacity = std::max(needed, v.capacity() + v.capacity() / 2 + 1);
    if (capacity > SIZE_MAX / sizeof(T))
      throw std::length_error("Mirror refinement allocation overflow");
    if (capacity * sizeof(T) > limits.max_bytes - bytes)
      throw std::length_error("Mirror refinement reallocation exceeds prospective peak quota");
    charge((capacity - v.capacity()) * sizeof(T));
    v.reserve(capacity);
  }
};
std::array<V, 3> triangle(const Mesh &m, uint32_t t, bool mirror = false, double offset = 0) {
  std::array<V, 3> v;
  for (size_t k = 0; k < 3; ++k) {
    v[k] = point(m.positions[m.indices[size_t(t) * 3 + k]]);
    if (mirror)
      v[k] = reflected(v[k], offset);
  }
  return v;
}
V evaluate(const Mesh &m, const MirrorSurfacePoint &p) {
  const auto v = triangle(m, p.triangle);
  return v[0] * p.barycentric[0] + v[1] * p.barycentric[1] + v[2] * p.barycentric[2];
}
struct Projection {
  V origin;
  int axis;
  P operator()(V q) const {
    q = q - origin;
    return axis == 0 ? P{q.y, q.z} : axis == 1 ? P{q.x, q.z} : P{q.x, q.y};
  }
};
std::array<double, 3> bary(P p, const std::array<P, 3> &t) {
  const auto a = t[1] - t[0], b = t[2] - t[0], q = p - t[0];
  const double d = det(a, b);
  if (!std::isfinite(d) || d == 0)
    throw std::invalid_argument("Mirror refinement has a degenerate parameter triangle");
  const double u = det(q, b) / d, v = det(a, q) / d;
  std::array<double, 3> w{1 - u - v, u, v};
  for (double &x : w) {
    if (!std::isfinite(x) || x < -roundoff || x > 1 + roundoff)
      throw std::invalid_argument("Mirror refinement left its actual source triangle");
    if (x < 0)
      x = 0;
    if (x > 1)
      x = 1;
  }
  const double sum = w[0] + w[1] + w[2];
  for (double &x : w)
    x /= sum;
  return w;
}
struct Piece {
  Polygon parameter;
};
bool same_weights(const std::array<double, 3> &a, const std::array<double, 3> &b) {
  for (size_t k = 0; k < 3; ++k)
    if (std::abs(a[k] - b[k]) > roundoff)
      return false;
  return true;
}
} // namespace
KeyMirrorRefinement refine_reflected_key(std::shared_ptr<const Mesh> source, float tolerance,
                                         const KeyMirrorLimits &limits) {
  return refine_reflected_key(std::move(source), KeyMirrorPlane{}, tolerance, limits);
}
KeyMirrorRefinement refine_reflected_key(std::shared_ptr<const Mesh> source, KeyMirrorPlane plane,
                                         float tolerance, const KeyMirrorLimits &limits) {
  if (!std::isfinite(plane.offset) || !std::isfinite(plane.offset * 2))
    throw std::invalid_argument("Invalid explicit Mirror plane");
  if (!source || !std::isfinite(tolerance) || tolerance <= 0 || tolerance > 1 ||
      !limits.max_bytes || limits.max_bytes > 256 * 1024 * 1024 || !limits.max_work ||
      limits.max_work > 256 * 1024 * 1024)
    throw std::invalid_argument("Invalid mirror refinement source or resource policy");
  Budget budget{limits};
  budget.poll();
  if (source->positions.size() > 2'000'000 || source->normals.size() > 2'000'000 ||
      source->uv.size() > 2'000'000 || source->indices.size() > 6'000'000 ||
      source->uv_pins.size() > 2'000'000 || source->uv_seams.size() > 6'000'000)
    throw std::length_error("Mirror refinement source streams exceed bounded geometry");
  const size_t input_work = source->positions.size() + source->normals.size() + source->uv.size() +
                            source->indices.size() + source->uv_pins.size() +
                            source->uv_seams.size();
  budget.poll(input_work);
  const size_t count = source->indices.size() / 3;
  if (!count || count > 2'000'000)
    throw std::length_error("Mirror refinement needs bounded actual source triangles");
  // This first structural kernel uses exhaustive bounded broad-phase pairs.
  // It chooses no nearest triangle. A production adapter needs a bounded BVH
  // before admitting inputs whose actual pair count exceeds this work policy.
  if (count > limits.max_work / count)
    throw std::length_error("Mirror refinement source pair preflight exceeds bounded work");
  source->validate();
  budget.charge(2 * 8 * sizeof(MirrorRefinementCorner));
  budget.charge(count * (sizeof(std::vector<Piece>) + sizeof(Bounds)));
  std::vector<std::vector<Piece>> coverage(count);
  std::vector<Bounds> boxes;
  boxes.reserve(count);
  for (size_t t = 0; t < count; ++t) {
    budget.poll();
    const auto v = triangle(*source, uint32_t(t));
    const auto n = cross(v[1] - v[0], v[2] - v[0]);
    if (!std::isfinite(dot(n, n)) || dot(n, n) <= 1e-28)
      throw std::invalid_argument("Degenerate triangle has no authentic mirror refinement");
    boxes.push_back(bounds(v));
  }
  KeyMirrorRefinement out;
  out.source = source;
  out.plane_offset = plane.offset;
  auto append_piece = [&](uint32_t t, const std::vector<MirrorRefinementCorner> &corners) {
    Polygon p;
    p.count = corners.size();
    for (size_t k = 0; k < p.count; ++k)
      p.p[k] = {corners[k].current.barycentric[1], corners[k].current.barycentric[2]};
    p = positive(p);
    const double area = signed_area(p);
    if (area <= roundoff)
      throw std::invalid_argument("Mirror refinement intersection is numerically unresolved");
    auto &prior = coverage[t];
    if (prior.size() >= 128)
      throw std::length_error("Mirror refinement triangle exceeds bounded piece count");
    for (const auto &old : prior) {
      budget.poll();
      if (std::abs(signed_area(clip(p, old.parameter))) > roundoff)
        throw std::invalid_argument(
            "Overlapping mirror source faces have ambiguous corner authority");
    }
    budget.grow(prior, prior.size() + 1);
    prior.push_back({p});
  };
  auto emit = [&](const MirrorRefinementCorner &a, const MirrorRefinementCorner &b,
                  const MirrorRefinementCorner &c) {
    if (out.corners.size() > 2'000'000 - 3)
      throw std::length_error("Mirror refinement output exceeds current vertex quota");
    budget.grow(out.corners, out.corners.size() + 3);
    budget.grow(out.opposite, out.opposite.size() + 3);
    const auto base = uint32_t(out.corners.size());
    out.corners.insert(out.corners.end(), {a, b, c});
    out.opposite.insert(out.opposite.end(), 3, UINT32_MAX);
    return base;
  };
  auto swap = [&](MirrorRefinementCorner c) {
    std::swap(c.current, c.reflected);
    const auto p = evaluate(*source, c.current);
    c.position = {float(p.x), float(p.y), float(p.z)};
    return c;
  };
  for (uint32_t t = 0; t < count; ++t) {
    budget.poll();
    const auto a = triangle(*source, t);
    const auto n = cross(a[1] - a[0], a[2] - a[0]);
    const double nlen = std::sqrt(dot(n, n));
    const int axis = std::abs(n.x) >= std::abs(n.y) && std::abs(n.x) >= std::abs(n.z) ? 0
                     : std::abs(n.y) >= std::abs(n.z)                                 ? 1
                                                                                      : 2;
    const Projection project{a[0], axis};
    std::array<P, 3> ap;
    for (size_t k = 0; k < 3; ++k)
      ap[k] = project(a[k]);
    Polygon pa;
    pa.count = 3;
    std::copy(ap.begin(), ap.end(), pa.p.begin());
    pa = positive(pa);
    for (uint32_t s = t; s < count; ++s) {
      budget.poll();
      const auto reflected_box =
          Bounds{reflected({boxes[s].high.x, boxes[s].low.y, boxes[s].low.z}, plane.offset),
                 reflected({boxes[s].low.x, boxes[s].high.y, boxes[s].high.z}, plane.offset)};
      if (!overlaps(boxes[t], reflected_box, tolerance))
        continue;
      const auto b = triangle(*source, s, true, plane.offset);
      bool within_plane = true;
      for (auto p : b)
        within_plane &= std::abs(dot(n, p - a[0])) <= double(tolerance) * nlen;
      if (!within_plane)
        continue;
      std::array<P, 3> bp;
      Polygon pb;
      pb.count = 3;
      for (size_t k = 0; k < 3; ++k)
        pb.p[k] = bp[k] = project(b[k]);
      pb = positive(pb);
      auto intersection = clip(pa, pb);
      if (intersection.count < 3)
        continue;
      const double area = signed_area(intersection);
      if (area <= 0)
        continue;
      if (area <= roundoff * std::max(1., std::abs(signed_area(pa))))
        throw std::invalid_argument("Mirror refinement contact is numerically unresolved");
      std::vector<MirrorRefinementCorner> vertices;
      vertices.reserve(intersection.count);
      for (size_t k = 0; k < intersection.count; ++k) {
        budget.poll();
        MirrorRefinementCorner c;
        c.current = {t, bary(intersection.p[k], ap)};
        c.reflected = {s, bary(intersection.p[k], bp)};
        const auto p = evaluate(*source, c.current),
                   q = reflected(evaluate(*source, c.reflected), plane.offset);
        c.position = {float(p.x), float(p.y), float(p.z)};
        const double error = std::sqrt(dot(p - q, p - q));
        if (error > tolerance)
          throw std::invalid_argument(
              "Mirror refinement exceeds declared original/rest correspondence error");
        out.maximum_rest_error = std::max(out.maximum_rest_error, error);
        vertices.push_back(c);
      }
      if (det(ap[1] - ap[0], ap[2] - ap[0]) < 0)
        std::reverse(vertices.begin(), vertices.end());
      append_piece(t, vertices);
      Polygon source_parameter;
      source_parameter.count = vertices.size();
      for (size_t k = 0; k < vertices.size(); ++k)
        source_parameter.p[k] = {vertices[k].reflected.barycentric[1],
                                 vertices[k].reflected.barycentric[2]};
      const bool reversed = signed_area(source_parameter) < 0;
      if (s != t) {
        auto other = vertices;
        for (auto &c : other)
          c = swap(c);
        if (reversed)
          std::reverse(other.begin(), other.end());
        append_piece(s, other);
      }
      MirrorRefinementCorner center;
      center.current.triangle = t;
      center.reflected.triangle = s;
      for (auto &c : vertices)
        for (size_t k = 0; k < 3; ++k) {
          center.current.barycentric[k] += c.current.barycentric[k] / vertices.size();
          center.reflected.barycentric[k] += c.reflected.barycentric[k] / vertices.size();
        }
      const auto center_position = evaluate(*source, center.current);
      center.position = {float(center_position.x), float(center_position.y),
                         float(center_position.z)};
      if (s != t) {
        for (size_t k = 0; k < vertices.size(); ++k) {
          const auto next = (k + 1) % vertices.size();
          const auto first = emit(center, vertices[k], vertices[next]),
                     second = reversed
                                  ? emit(swap(center), swap(vertices[next]), swap(vertices[k]))
                                  : emit(swap(center), swap(vertices[k]), swap(vertices[next]));
          out.opposite[first] = second;
          out.opposite[first + 1] = second + (reversed ? 2 : 1);
          out.opposite[first + 2] = second + (reversed ? 1 : 2);
          out.opposite[second] = first;
          out.opposite[second + 1] = first + (reversed ? 2 : 1);
          out.opposite[second + 2] = first + (reversed ? 1 : 2);
        }
      } else {
        if (!same_weights(center.current.barycentric, center.reflected.barycentric))
          throw std::invalid_argument(
              "Self-mirrored refinement center lacks an exact source involution");
        std::array<size_t, 8> opposite{};
        for (size_t k = 0; k < vertices.size(); ++k) {
          size_t matches = 0;
          for (size_t j = 0; j < vertices.size(); ++j)
            if (same_weights(vertices[k].reflected.barycentric, vertices[j].current.barycentric)) {
              opposite[k] = j;
              ++matches;
            }
          if (matches != 1)
            throw std::invalid_argument(
                "Self-mirrored refinement boundary lacks unique source involution");
        }
        std::array<uint32_t, 8> starts{};
        for (size_t k = 0; k < vertices.size(); ++k)
          starts[k] = emit(center, vertices[k], vertices[(k + 1) % vertices.size()]);
        for (size_t k = 0; k < vertices.size(); ++k) {
          const size_t next = (k + 1) % vertices.size(),
                       j = reversed ? opposite[next] : opposite[k];
          if (reversed ? opposite[k] != (j + 1) % vertices.size()
                       : opposite[next] != (j + 1) % vertices.size())
            throw std::invalid_argument(
                "Self-mirrored refinement did not preserve actual source corner order");
          out.opposite[starts[k]] = starts[j];
          out.opposite[starts[k] + 1] = starts[j] + (reversed ? 2 : 1);
          out.opposite[starts[k] + 2] = starts[j] + (reversed ? 1 : 2);
        }
      }
    }
  }
  for (const auto &pieces : coverage) {
    budget.poll();
    double area = 0;
    for (const auto &p : pieces)
      area += signed_area(p.parameter);
    if (std::abs(area - .5) > roundoff * std::max(size_t(1), pieces.size()))
      throw std::invalid_argument(
          "Reflected source surface has a hole or unrepresented noncoplanar geometry");
  }
  for (size_t c = 0; c < out.corners.size(); ++c) {
    budget.poll();
    if (out.opposite[c] >= out.corners.size() || out.opposite[out.opposite[c]] != c)
      throw std::invalid_argument("Mirror refinement output has no genuine corner involution");
    const auto &a = out.corners[c];
    const auto &b = out.corners[out.opposite[c]];
    const auto stored = point(a.position), current = evaluate(*source, a.current),
               mirrored = reflected(evaluate(*source, a.reflected), plane.offset);
    const double represented_error = std::sqrt(std::max(dot(stored - current, stored - current),
                                                        dot(stored - mirrored, stored - mirrored)));
    if (represented_error > tolerance)
      throw std::invalid_argument("Mirror refinement float representation exceeds its actual "
                                  "source correspondence tolerance");
    out.maximum_rest_error = std::max(out.maximum_rest_error, represented_error);
    if (c % 3 == 0) {
      const auto x = point(out.corners[c + 1].position) - stored,
                 y = point(out.corners[c + 2].position) - stored;
      if (dot(cross(x, y), cross(x, y)) <= 1e-28)
        throw std::invalid_argument(
            "Mirror refinement float representation collapsed an actual triangle");
    }
    if (a.current.triangle != b.reflected.triangle || a.reflected.triangle != b.current.triangle ||
        !same_weights(a.current.barycentric, b.reflected.barycentric) ||
        !same_weights(a.reflected.barycentric, b.current.barycentric))
      throw std::invalid_argument("Mirror refinement opposite provenance was fabricated");
  }
  out.work_units = budget.work;
  return out;
}
} // namespace dome::detail
