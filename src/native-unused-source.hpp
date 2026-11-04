// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh.hpp"
#include "heat-component-intent.hpp"
#include "heat-source.hpp"
namespace dome::detail {
// A one-sided proof that an unobserved native handle is genuinely dominated
// over every complete target triangle. No vertex-only absence is authority.
// Squared distance to a convex segment is convex: its maximum over a triangle
// is bounded by the largest vertex distance. We use explicit points on the
// competing segment for outward upper bounds, and disjoint AABBs for a lower
// bound to the unused segment. Failure to separate is UNKNOWN, never unused.
namespace unused_source {
struct Interval {
  double lo, hi;
};
inline double down(double x) { return std::nextafter(x, -INFINITY); }
inline double up(double x) { return std::nextafter(x, INFINITY); }
inline Interval add(Interval a, Interval b) {
  return {down(a.lo + b.lo), up(a.hi + b.hi)};
}
inline Interval subtract(Interval a, Interval b) {
  return {down(a.lo - b.hi), up(a.hi - b.lo)};
}
inline Interval multiply(Interval a, double b) {
  return b >= 0 ? Interval{down(a.lo * b), up(a.hi * b)}
                : Interval{down(a.hi * b), up(a.lo * b)};
}
inline void point(const std::array<double, 3> &p) {
  for (auto v : p)
    if (!std::isfinite(v) || std::abs(v) > 10000 || double(float(v)) != v)
      throw std::invalid_argument(
          "Unused-source proof needs captured bounded binary32 coordinates");
}
inline double upper(Vec3 p, const HeatSourceSegment &s) {
  const double v[3] = {p.x, p.y, p.z};
  double dot = 0, length = 0;
  for (size_t k = 0; k < 3; ++k) {
    const double d = s.tail[k] - s.head[k];
    dot += (v[k] - s.head[k]) * d;
    length += d * d;
  }
  // This need not be an exact projection: any finite t in[0,1] names a
  // genuine segment point. Its position and norm are enclosed independently.
  const double t = length ? std::clamp(dot / length, 0., 1.) : 0.;
  if (!std::isfinite(t))
    throw std::range_error("Unused-source projection range");
  double sum = 0;
  for (size_t k = 0; k < 3; ++k) {
    auto delta = subtract({s.tail[k], s.tail[k]}, {s.head[k], s.head[k]});
    auto q = add({s.head[k], s.head[k]}, multiply(delta, t));
    auto r = subtract({v[k], v[k]}, q);
    const auto a = std::max(std::abs(r.lo), std::abs(r.hi));
    sum = up(sum + up(a * a));
  }
  return sum;
}
inline double lower(std::array<Vec3, 3> triangle, const HeatSourceSegment &s) {
  double result = 0;
  for (size_t k = 0; k < 3; ++k) {
    auto coordinate = [k](Vec3 p) {
      return std::array<double, 3>{p.x, p.y, p.z}[k];
    };
    const double p[3] = {coordinate(triangle[0]), coordinate(triangle[1]),
                         coordinate(triangle[2])};
    const double lo = std::min({p[0], p[1], p[2]}),
                 hi = std::max({p[0], p[1], p[2]}),
                 slo = std::min(s.head[k], s.tail[k]),
                 shi = std::max(s.head[k], s.tail[k]);
    const double gap = std::max({0., down(slo - hi), down(lo - shi)});
    result = std::max(0., down(result + std::max(0., down(gap * gap))));
  }
  return result;
}
} // namespace unused_source
template <class Budget>
bool certify_unused_native_source(const HeatSourceSegment &unused,
                                  std::span<const HeatSourceSegment> sources,
                                  const Mesh &m,
                                  std::span<const HeatJointMask> allowed,
                                  Budget &w) {
#if defined(__FAST_MATH__)
  (void)unused;
  (void)sources;
  (void)m;
  (void)allowed;
  (void)w;
  throw std::runtime_error(
      "Unused native-source proof requires strict floating arithmetic");
#else
  w.stop();
  if (m.positions.empty() || m.indices.empty() || unused.bone >= 256 ||
      allowed.size() != m.positions.size() || m.indices.size() % 3 ||
      sources.empty() || sources.size() > 512)
    throw std::invalid_argument("Unused-source owning mesh/mask dimensions");
  unused_source::point(unused.head);
  unused_source::point(unused.tail);
  for (const auto &s : sources) {
    w.step(8);
    if (s.bone >= 256)
      throw std::invalid_argument("Unused-source control outside owning mask");
    unused_source::point(s.head);
    unused_source::point(s.tail);
  }
  for (size_t c = 0; c < m.indices.size(); c += 3) {
    w.step(64);
    const auto a = m.indices[c], b = m.indices[c + 1], d = m.indices[c + 2];
    if (a >= allowed.size() || b >= allowed.size() || d >= allowed.size() ||
        allowed[a] != allowed[b] || allowed[a] != allowed[d])
      throw std::invalid_argument(
          "Unused-source triangle lost component ownership");
    const std::array<Vec3, 3> tri{m.positions[a], m.positions[b],
                                  m.positions[d]};
    for (auto p : tri)
      unused_source::point({p.x, p.y, p.z});
    const double lo = unused_source::lower(tri, unused);
    bool dominated = false;
    for (const auto &s : sources) {
      w.step(512);
      if (!(allowed[a][s.bone / 64] & (uint64_t(1) << (s.bone % 64))))
        continue;
      const double hi = std::max({unused_source::upper(tri[0], s),
                                  unused_source::upper(tri[1], s),
                                  unused_source::upper(tri[2], s)});
      if (lo > hi) {
        dominated = true;
        break;
      }
    }
    if (!dominated)
      return false;
  }
  w.stop();
  return true;
#endif
}
} // namespace dome::detail
