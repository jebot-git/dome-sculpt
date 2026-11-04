// SPDX-License-Identifier: GPL-3.0-or-later
#include "anatomical-continuation.hpp"
#include <iostream>
using namespace dome;
int main() try {
  size_t checks = 0;
  auto check = [&](bool ok, const char *why) {
    ++checks;
    if (!ok)
      throw std::runtime_error(why);
  };
  const std::function<bool()> never = [] { return false; };
  detail::weight_surface::Budget budget{0, 50000000, never};
  const std::vector<detail::HeatSourceSegment> controls{
      {{-1, 0, -.25}, {-1, 1, -.25}, 0},
      {{1, 0, -.25}, {1, 1, -.25}, 1},
      {{0, 0, -.25}, {0, 1, -.25}, 2}};
  detail::NearestWeightSources nearest(controls, budget);
  auto allowed = [](uint64_t v) { return std::array<uint64_t, 4>{v, 0, 0, 0}; };
  check(nearest.nearest({.125f, .5f, 0}, allowed(3), budget) == allowed(2),
        "Foreign closer handle overrides declared component intent");
  check(nearest.nearest({0, .5f, 0}, allowed(3), budget) == allowed(3),
        "Exact row tie drops a genuine permitted control");
  check(nearest.nearest({-1, .5f, 0}, allowed(2), budget) == allowed(2),
        "Explicit permitted intent cannot select farther declared handle");
  bool refused = false;
  try {
    (void)nearest.nearest({0, 0, 0}, allowed(0), budget);
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  check(refused, "Empty row intent silently selects foreign source");
  Mesh m;
  m.positions = {{-1, 0, 0}, {0, 0, 0}, {1, 0, 0},
                 {-1, 1, 0}, {0, 1, 0}, {1, 1, 0}};
  m.normals.assign(6, {0, 0, 1});
  m.uv = {{{0, 0}}, {{.5, 0}}, {{1, 0}}, {{0, 1}}, {{.5, 1}}, {{1, 1}}};
  m.indices = {0, 1, 4, 0, 4, 3, 1, 2, 5, 1, 5, 4};
  Scene scene;
  scene.active().painted = std::make_shared<PaintedMesh>(m, 64);
  scene.active().mesh_active = true;
  DeformationState d(m);
  d.skeleton = Skeleton{{{"left", -1, {{-1, 0, -.25}}, 1, true},
                         {"right", -1, {{1, 0, -.25}}, 1, true}}};
  scene.set_deformation(1, d);
  const auto before = scene.encode();
  detail::DeclaredWeightSupport domain{
      detail::DeclaredSupportPolicy::NearestSourceUnion, true, {{1, {}}}, 1};
  domain.row_anchors = detail::DeclaredRowAnchors::NearestPermitted;
  const auto candidate = detail::anatomical_continuation(
      scene, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
      {50000000, 64u * 1024u * 1024u}, &domain);
  check(scene.encode() == before, "Detached row solve mutated original scene");
  // Independent FEM energy of two unit squares: half conductance on the
  // boundary, unit middle vertical, zero diagonal. Mixed Voronoi areas are
  // 1/4 at the four corners and 1/2 at the two middle boundary vertices.
  std::array<std::array<long double, 7>, 6> a{};
  auto edge = [&](size_t i, size_t j, long double w) {
    a[i][i] += w;
    a[j][j] += w;
    a[i][j] -= w;
    a[j][i] -= w;
  };
  for (auto e :
       {std::array<size_t, 2>{0, 1}, {1, 2}, {3, 4}, {4, 5}, {0, 3}, {2, 5}})
    edge(e[0], e[1], .5L);
  edge(1, 4, 1);
  // Independent composite Simpson integration; no production integral or
  // production sparse assembly/linear solver is used by this oracle.
  constexpr int steps = 4096;
  for (size_t i = 0; i < 6; ++i) {
    const auto p = m.positions[i];
    const long double mass = (i == 1 || i == 4) ? .5L : .25L;
    for (size_t j = 0; j < 2; ++j) {
      if ((p.x < 0 && j == 1) || (p.x > 0 && j == 0))
        continue;
      auto f = [&](long double t) {
        const long double x = p.x - (j ? 1.L : -1.L), y = p.y - t;
        return mass / (x * x + y * y + .0625L + mass);
      };
      long double h = f(0) + f(1);
      for (int k = 1; k < steps; ++k)
        h += (k & 1 ? 4 : 2) * f(static_cast<long double>(k) / steps);
      h /= 3 * steps;
      a[i][i] += h;
      if (j == 0)
        a[i][6] += h;
    }
  }
  const auto original = a;
  for (size_t k = 0; k < 6; ++k) {
    const auto pivot = a[k][k];
    check(pivot > 0, "Independent dense SPD elimination lost pivot");
    for (size_t c = k; c < 7; ++c)
      a[k][c] /= pivot;
    for (size_t r = 0; r < 6; ++r)
      if (r != k) {
        const auto factor = a[r][k];
        for (size_t c = k; c < 7; ++c)
          a[r][c] -= factor * a[k][c];
      }
  }
  double maximum = 0;
  for (size_t i = 0; i < 6; ++i) {
    const auto row = candidate.parts[0].raw_to_row[i];
    const auto v = candidate.fields[row * 2];
    maximum = std::max(maximum, std::abs(v - double(a[i][6])));
    check(std::abs(v - double(a[i][6])) < 2e-7,
          "Nearest-row field differs from independent dense energy solve");
    check(std::abs(candidate.fields[row * 2] + candidate.fields[row * 2 + 1] -
                   1) < 1e-7,
          "Nearest-row result lost partition of unity");
    long double residual = -original[i][6];
    for (size_t j = 0; j < 6; ++j)
      residual += original[i][j] * a[j][6];
    check(std::abs(residual) < 1e-15L, "Independent oracle residual invalid");
  }
  domain.row_anchors = detail::DeclaredRowAnchors::AllVisible;
  const auto all = detail::anatomical_continuation(
      scene, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
      {50000000, 64u * 1024u * 1024u}, &domain);
  double sensitivity = 0;
  for (size_t i = 0; i < candidate.fields.size(); ++i)
    sensitivity =
        std::max(sensitivity, std::abs(candidate.fields[i] - all.fields[i]));
  check(sensitivity > .03,
        "All-visible negative is not sensitive to row anchoring");
  std::cout << "PASS row_energy_checks=" << checks << " dense_max=" << maximum
            << " all_visible_sensitivity=" << sensitivity
            << " work=" << candidate.report.work << '\n';
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
