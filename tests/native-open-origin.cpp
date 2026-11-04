// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/bone-shadow-query.hpp"
#include <iostream>
using namespace dome::detail;
using P = bone_shadow::Point;
namespace {
struct Work {
  size_t used = 0, maximum = 500000000, cancel_at = SIZE_MAX;
  void stop() const {
    if (used >= cancel_at)
      throw std::runtime_error("cancelled");
  }
  void step(size_t n = 1) {
    if (n > maximum - used)
      throw std::length_error("work");
    used += n;
    stop();
  }
};
size_t checks = 0;
void check(bool ok, const char *why) {
  ++checks;
  if (!ok)
    throw std::runtime_error(why);
}
struct Geometry {
  std::vector<P> p{{0, 0, 0},       {1, 0, 0},      {0, 1, 0},
                   {-.5f, -.5f, 0}, {.5f, -.5f, 0}, {0, .5f, 0}};
  std::vector<uint32_t> i{0, 1, 2, 3, 4, 5, 1, 3, 4};
};
double simpson(P row, P head, P tail, double mass) {
  constexpr int n = 10000;
  long double sum = 0;
  for (int t = 0; t <= n; ++t) {
    long double d = mass;
    for (int k = 0; k < 3; ++k) {
      const long double v = static_cast<long double>(head[k]) +
                            static_cast<long double>(t) / n *
                                (static_cast<long double>(tail[k]) - head[k]) -
                            row[k];
      d += v * v;
    }
    sum += (t == 0 || t == n ? 1 : t & 1 ? 4 : 2) * mass / d;
  }
  return double(sum / (3 * n));
}
double query(const Geometry &g, P h, P t, double mass = .1,
             bool filtered = true) {
  Work w;
  BoneShadowQuery q(g.p, g.i, 16u * 1024u * 1024u, w);
  BoneShadowQuery::Scratch s(q);
  BoneQueryReport r;
  return q.integrate(0, h, t, mass, 0, 1, s, w, r,
                     BoneWallDomain::ConnectedSourceSurface, filtered, true,
                     BoneOriginPolicy::NativeOpenSegment);
}
bool ambiguous(BoneShadowQuery &q, BoneShadowQuery::Scratch &s, P h, P t,
               BoneOriginPolicy policy = BoneOriginPolicy::NativeOpenSegment) {
  Work w;
  BoneQueryReport r;
  try {
    q.integrate(0, h, t, .1, 0, 1, s, w, r,
                BoneWallDomain::ConnectedSourceSurface, true, true, policy);
  } catch (const BoneQueryAmbiguity &e) {
    return e.kind == bone_shadow::Kind::OriginContact;
  }
  return false;
}
} // namespace
int main() try {
  Geometry g;
  const P h{0, 0, 1}, t{.25f, 0, 2};
  const auto before = g.p;
  const auto indices = g.i;
  Work w;
  BoneShadowQuery q(g.p, g.i, 16u * 1024u * 1024u, w);
  BoneShadowQuery::Scratch scratch(q);
  check(ambiguous(q, scratch, h, t, BoneOriginPolicy::StrictOwnership),
        "Original strict unowned-origin negative remains a refusal");
  const auto expected = simpson(g.p[0], h, t, .1);
  const auto a = query(g, h, t), b = query(g, h, t, .1, false);
  check(std::abs(a - expected) < 1e-12 && a == b,
        "Exact closed-triangle origin proof and strict source halfspace match "
        "independent quadrature, filtered and exact controls");
  check(g.p == before && g.i == indices,
        "No incidence, FEM rows, topology or source geometry is changed");
  BoneQueryReport r;
  auto first = q.integrate(0, h, t, .1, 0, 1, scratch, w, r,
                           BoneWallDomain::ConnectedSourceSurface, true, true,
                           BoneOriginPolicy::NativeOpenSegment);
  check(
      first == a,
      "Same scratch accepts the source-specific exact origin-only certificate");
  check(ambiguous(q, scratch, h, t, BoneOriginPolicy::StrictOwnership),
        "Native proof never authorizes the source-independent strict origin "
        "cache");
  check(ambiguous(q, scratch, {0, 0, -1}, {0, 0, 1}),
        "Source straddling the origin plane remains refused after a cached "
        "positive");
  check(ambiguous(q, scratch, {.1f, 0, 0}, {.2f, 0, 0}),
        "Entire coplanar source remains refused");
  check(ambiguous(q, scratch, {0, 0, 0}, {0, 0, 1}),
        "Source endpoint on the plane is not silently treated as strictly "
        "separated");
  BoneQueryReport again;
  check(q.integrate(0, h, t, .1, 0, 1, scratch, w, again,
                    BoneWallDomain::ConnectedSourceSurface, true, true,
                    BoneOriginPolicy::NativeOpenSegment) == a,
        "Changed-source epoch can retry the exact original source without "
        "stale contact authorization");
  bool wrongdomain = false;
  try {
    q.integrate(0, h, t, .1, 0, 1, scratch, w, again,
                BoneWallDomain::WholeGroup, true, true,
                BoneOriginPolicy::NativeOpenSegment);
  } catch (const std::invalid_argument &) {
    wrongdomain = true;
  }
  check(
      wrongdomain,
      "Open native contacts cannot widen to a foreign whole-group wall domain");
  for (int sign : {-1, 1}) {
    P x{0, 0, sign * 0x1p-100f}, y{0, 0, sign * 0x1p-99f};
    check(std::abs(query(g, x, y) - simpson(g.p[0], x, y, .1)) < 1e-12,
          "Exact tiny nonzero source halfspace has no near-zero epsilon skip");
  }
  for (P origin : {P{0, .5f, 0}, P{.5f, -.5f, 0}, P{0, 0, 0x1p-100f}}) {
    auto edge = g;
    edge.p[0] = origin;
    check(std::abs(query(edge, h, t) - simpson(origin, h, t, .1)) < 1e-12,
          "Closed origin edge/vertex or genuinely noncoplanar near-origin "
          "geometry retains exact classification");
  }
  for (float height : {.5f, 0x1p-30f}) {
    auto wall = g;
    const auto v = uint32_t(wall.p.size());
    wall.p.insert(wall.p.end(),
                  {{-2, -2, height}, {0, 2, height}, {2, -2, height}});
    wall.i.insert(wall.i.end(), {v, v + 1, v + 2, 1, v, v + 1});
    check(query(wall, h, t) == 0 && query(wall, h, t, .1, false) == 0,
          "A genuine interior or arbitrarily thin nearby wall remains opaque "
          "after excluding only origin contact");
  }
  auto reflected = g;
  for (auto &p : reflected.p)
    p[0] = -p[0];
  auto rh = h, rt = t;
  rh[0] = -rh[0];
  rt[0] = -rt[0];
  for (size_t c = 0; c < reflected.i.size(); c += 3)
    std::swap(reflected.i[c + 1], reflected.i[c + 2]);
  check(query(reflected, rh, rt) == a,
        "Reflection with genuine winding preserves the origin certificate");
  auto transformed = g;
  auto transform = [](P p) {
    return P{4 * p[2] + 2, 4 * p[0] - 3, 4 * p[1] + 1};
  };
  for (auto &p : transformed.p)
    p = transform(p);
  check(std::abs(query(transformed, transform(h), transform(t), 1.6) - a) <
            1e-12,
        "Independent metric rescale/rotation/translation preserves the "
        "normalized field integral");
  auto separate = g;
  separate.i = {0, 1, 2, 3, 4, 5};
  check(std::abs(query(separate, h, t) - expected) < 1e-12,
        "Touching separate indexed components are never unioned into native "
        "origin incidence");
  Work limited;
  limited.maximum = 1;
  bool bounded = false;
  try {
    BoneShadowQuery denied(g.p, g.i, 16u * 1024u * 1024u, limited);
  } catch (const std::length_error &) {
    bounded = true;
  }
  check(bounded, "Construction forecasts reject before owning copies");
  Work cancelled;
  cancelled.cancel_at = 1;
  BoneQueryReport untouched;
  bool stopped = false;
  try {
    q.integrate(0, h, t, .1, 0, 1, scratch, cancelled, untouched,
                BoneWallDomain::ConnectedSourceSurface, true, true,
                BoneOriginPolicy::NativeOpenSegment);
  } catch (const std::runtime_error &) {
    stopped = true;
  }
  check(stopped && untouched.nodes == 0 && untouched.intervals == 0,
        "Real bounded cancellation leaves staged query report unpublished");
  std::cout << "PASS native_open_origin_checks=" << checks << " integral=" << a
            << " independent=" << expected
            << " strict_control_retained=1 no_weld=1\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
