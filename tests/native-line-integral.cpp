// SPDX-License-Identifier: GPL-3.0-or-later
// Independent scalar controls for explicitly selected finite-line measure.
#define main previous_native_open_origin_main
#include "native-open-origin.cpp"
#undef main
namespace {
std::pair<double, BoneQueryReport> line(const Geometry &g, P h, P t,
                                        bool filtered = true) {
  Work w;
  BoneShadowQuery q(g.p, g.i, 16u * 1024u * 1024u, w, {},
                    BoneOriginPolicy::NativeFiniteLineIntegral);
  BoneShadowQuery::Scratch s(q);
  BoneQueryReport r;
  auto v = q.integrate(0, h, t, .1, 0, 1, s, w, r,
                       BoneWallDomain::ConnectedSourceSurface, filtered, true,
                       BoneOriginPolicy::NativeFiniteLineIntegral);
  return {v, r};
}
} // namespace
int main() try {
  checks = 0;
  Geometry g;
  // Keep the topological bridge outside the whole x=0 ray fan. The prior
  // pointwise fixture's coplanar bridge has a separate unresolved grazing ray.
  g.p = {{0, 0, 0}, {2, 0, 0}, {0, 2, 0}, {-1, 0, 0}, {1, -1, 0}, {1, 1, 0}};
  g.i = {0, 1, 2, 3, 4, 5, 1, 4, 5};
  const P h{0, 0, -1}, t{0, 0, 1};
  bool grazing_refused = false;
  try {
    (void)line(Geometry{}, h, t);
  } catch (const BoneQueryAmbiguity &e) {
    grazing_refused = e.kind == bone_shadow::Kind::Grazing;
  }
  check(grazing_refused, "Unproved outside-origin coplanar grazing remains a "
                         "refusal, not a blanket contact skip");
  const auto before = g.p;
  const auto indices = g.i;
  const auto [value, report] = line(g, h, t);
  const auto expected = std::sqrt(.1) * std::atan(1 / std::sqrt(.1));
  check(std::abs(value - expected) < 1e-12 &&
            value == line(g, h, t, false).first,
        "Exact nonidentically-zero plane function selects the analytic "
        "continuous a.e. representative, not quadrature sample deletion");
  check(report.measure_zero_origins == 1 && g.p == before && g.i == indices,
        "Finite root-set certificate is measured with no incidence or geometry "
        "changes");
  Work w;
  BoneShadowQuery q(g.p, g.i, 16u * 1024u * 1024u, w, {},
                    BoneOriginPolicy::NativeFiniteLineIntegral);
  BoneShadowQuery::Scratch s(q);
  BoneQueryReport r;
  check(ambiguous(q, s, h, t, BoneOriginPolicy::StrictOwnership),
        "Strict ownership retains the original straddling-origin failure after "
        "integral work");
  bool policy_refused = false;
  try {
    q.integrate(0, h, t, .1, 0, 1, s, w, r,
                BoneWallDomain::ConnectedSourceSurface, true, true,
                BoneOriginPolicy::NativeOpenSegment);
  } catch (const std::invalid_argument &) {
    policy_refused = true;
  }
  check(policy_refused, "Pointwise native policy cannot borrow integral-only "
                        "admission/cache authorization");
  bool coplanar = false;
  try {
    (void)line(g, {.1f, 0, 0}, {.2f, 0, 0});
  } catch (const BoneQueryAmbiguity &) {
    coplanar = true;
  }
  check(coplanar, "An identically-zero source-plane function remains refused");
  bool degenerate = false;
  try {
    (void)line(g, {0, 0, 1}, {0, 0, 1});
  } catch (const std::invalid_argument &) {
    degenerate = true;
  }
  check(degenerate,
        "A degenerate point source cannot masquerade as a genuine finite line");
  for (float z : {0x1p-20f, 0x1p-100f}) {
    auto near = g;
    near.p[0][2] = z;
    const auto result = line(near, h, t);
    check(result.second.measure_zero_origins == 0 &&
              std::abs(result.first - simpson(near.p[0], h, t, .1, .5, 1)) <
                  1e-12,
          "A near-but-not-exact origin plane retains its genuine "
          "positive-measure blocker without epsilon promotion");
  }
  const auto tiny = line(g, {0, 0, -0x1p-100f}, {0, 0, 0x1p-100f});
  check(
      std::abs(tiny.first - 1) < 1e-12 && tiny.second.measure_zero_origins == 1,
      "Tiny but genuine transverse segment keeps exact finite-root semantics");
  for (float z : {.25f, 0x1p-30f}) {
    auto wall = g;
    const auto v = uint32_t(wall.p.size());
    wall.p.insert(wall.p.end(), {{1, -2, z}, {-2, 0, z}, {1, 2, z}});
    wall.i.insert(wall.i.end(), {v, v + 1, v + 2, 1, v, v + 2});
    const auto result = line(wall, h, t);
    const double end = (1 + double(z)) / 2;
    check(result.second.intervals > 0 &&
              std::abs(result.first - simpson(g.p[0], h, t, .1, 0, end)) <
                  1e-12 &&
              result.first < value,
          "A separate finite-width source occlusion remains in the exact "
          "interval union");
  }
  BoneQueryReport overflow;
  overflow.measure_zero_origins = SIZE_MAX;
  const auto old = overflow;
  bool bounded = false;
  try {
    q.integrate(0, h, t, .1, 0, 1, s, w, overflow,
                BoneWallDomain::ConnectedSourceSurface, true, true,
                BoneOriginPolicy::NativeFiniteLineIntegral);
  } catch (const std::length_error &) {
    bounded = true;
  }
  check(bounded && overflow.measure_zero_origins == old.measure_zero_origins &&
            overflow.nodes == old.nodes,
        "Finite-root accounting overflow publishes no partial report");
  Work cancelled;
  cancelled.cancel_at = 1;
  BoneQueryReport untouched;
  bool stopped = false;
  try {
    q.integrate(0, h, t, .1, 0, 1, s, cancelled, untouched,
                BoneWallDomain::ConnectedSourceSurface, true, true,
                BoneOriginPolicy::NativeFiniteLineIntegral);
  } catch (const std::runtime_error &) {
    stopped = true;
  }
  check(stopped && !untouched.nodes && !untouched.measure_zero_origins,
        "Integral cancellation preserves staged accounting");
  std::cout << "PASS native_finite_line_controls=" << checks
            << " analytic=" << value << " independent=" << expected
            << " root_bound=" << report.measure_zero_origins
            << " pointwise_policy_unchanged=1\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL after_controls=" << checks << " " << e.what() << '\n';
  return 1;
}
