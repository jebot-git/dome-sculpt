// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only private assembly/seed observation. No production testing hooks.
#include "../src/heat-binding.cpp"
#include <iostream>
using namespace dome;
using namespace dome::detail;
int main() try {
  Mesh m;
  m.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  m.normals.assign(3, {0, 0, 1});
  m.uv = {{0, 0}, {1, 0}, {0, 1}};
  m.indices = {0, 1, 2};
  Skeleton rig;
  rig.bones.resize(2);
  rig.bones[0].name = "A";
  rig.bones[1].name = "B";
  HeatBindingOptions o;
  o.operation = HeatOperator::PositiveGraph;
  o.visibility = HeatVisibility::EntryFaces;
  o.maximum_seed_distance = 2;
  o.heat_strength = 4;
  std::array<HeatSourceSegment, 2> sources{
      {{{0, 0, -.1}, {0, 0, -.1}, 0}, {{0, 0, -.100005}, {0, 0, -.100005}, 1}}};
  std::function<bool()> cancel;
  auto get = [&](HeatSeedRule mode, std::span<const HeatSourceSegment> src) {
    Budget budget{0, 100000, cancel};
    auto s = assemble(m, budget, o.operation, {});
    auto options = o;
    options.seed_rule = mode;
    seed(s, m, rig, options, budget, src, {});
    return s;
  };
  auto strict = get(HeatSeedRule::DistinctBones, sources),
       ref = get(HeatSeedRule::ReferenceSegments, sources);
  if (strict.ties[0] != 1 || ref.ties[0] != 2 || ref.source_counts[0] != 1 ||
      ref.source_counts[1] != 1)
    throw std::runtime_error("Exact 50 ppm near-junction tie did not "
                             "distinguish 1e-6 from reference1e-4");
  const long double nearest = .1L / 1.0005L,
                    expected = (1.L / 6) * 4 * 2 / (nearest * nearest);
  if (std::abs(ref.heat[0] / expected - 1) > 2e-15L ||
      std::abs(ref.heat[0] - 2 * strict.heat[0]) > 1e-12)
    throw std::runtime_error("Reference source H differs from independent "
                             "analytic nearest distance/tie count");
  std::array<HeatSourceSegment, 3> repeated{
      {sources[0], sources[0], sources[0]}};
  repeated[2].bone = 1;
  auto multiple = get(HeatSeedRule::ReferenceSegments, repeated);
  if (multiple.ties[0] != 3 || multiple.source_counts[0] != 2 ||
      multiple.source_counts[1] != 1)
    throw std::runtime_error(
        "Reference segment sources lost same-bone multiplicity");
  for (auto mode :
       {HeatSeedRule::DistinctBones, HeatSeedRule::ReferenceSegments}) {
    o.seed_rule = mode;
    o.output = HeatOutput::ReferenceLimit;
    auto result = heat_bind_surface(m, rig, o, {}, nullptr, repeated);
    for (const auto &row : result.vertices) {
      double a = 0, b = 0;
      for (auto value : row)
        (value.joint ? b : a) = value.weight;
      if (std::abs(a - (mode == HeatSeedRule::ReferenceSegments ? 2. / 3
                                                                : .5)) > 1e-6 ||
          std::abs(a + b - 1) > 1e-6)
        throw std::runtime_error("Same-bone linear source RHS aggregation "
                                 "differs from independent constant solution");
    }
  }
  for (auto [x, y] : std::array<std::pair<double, double>, 7>{{{0, 0},
                                                               {.024, 0},
                                                               {.025, 0},
                                                               {.03, .01},
                                                               {.04, .03},
                                                               {.05, .05},
                                                               {.8, .8}}})
    if (std::abs(reference_limit(x) - y) > 1e-15)
      throw std::runtime_error(
          "Reference output limiter differs from independent piecewise values");
  for (auto [p, q] : std::array<std::pair<D, D>, 3>{{{{-1, 1, 0}, {0, 0, 0}},
                                                     {{.5, 1, 0}, {.5, 0, 0}},
                                                     {{2, 1, 0}, {1, 0, 0}}}})
    if (closest(p, {{0, 0, 0}, {1, 0, 0}, 0}) != q)
      throw std::runtime_error("Segment root/interior/tip projection disagrees "
                               "with analytic geometry");
  Mesh boundary;
  boundary.positions = {{0, 0, 0}, {20, 0, 0}, {5, 1, 0}, {0, 20, 0}};
  boundary.normals.assign(4, {0, 0, 1});
  boundary.uv.assign(4, {0, 0});
  boundary.indices = {0, 1, 2, 0, 2, 3};
  Budget budget{0, 100000, cancel};
  auto fem = assemble(boundary, budget, HeatOperator::FiniteElements, {});
  auto counted = assemble(boundary, budget, HeatOperator::FiniteElements, {},
                          HeatNormals::UnitFaces, HeatMass::MixedVoronoi,
                          HeatEdges::IncidentMean);
  std::array<double, 4> direction{-3, -1, -2, -3}, out;
  auto energy = [&](const System &matrix) {
    matrix.multiply(direction, out, budget);
    double value = 0;
    for (int i = 0; i < 4; ++i)
      value += direction[i] * out[i];
    return value;
  };
  const double standard = energy(fem), incidence = energy(counted);
  // Independent exact rational barycentric-gradient/count certificate gives
  // 23/5 and -1/5. Negative curvature here is not numerical roundoff.
  if (std::abs(standard - 23. / 5) > 1e-12 ||
      std::abs(incidence + 1. / 5) > 1e-12)
    throw std::runtime_error("Boundary normalization differs from independent "
                             "exact energy certificate");
  std::cout << "BOUNDARY_ENERGY standard=" << standard
            << " counted=" << incidence << '\n';
  std::cout
      << "PASS analytic nearest-source geometry, 50ppm ties, H, same-bone "
         "segment multiplicity/RHS, reference limiter and segment endpoints\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
