// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/heat-binding.cpp"
#include <iostream>
using namespace dome;
using namespace dome::detail;
int main() try {
  Mesh m;
  m.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  m.normals.assign(3, {0, 0, 1});
  m.uv.assign(3, {0, 0});
  m.indices = {0, 1, 2};
  Skeleton rig;
  rig.bones.resize(2);
  rig.bones[0].name = "Below";
  rig.bones[1].name = "Nearer above";
  std::array<HeatSourceSegment, 2> sources{
      {{{0, 0, -.1}, {0, 0, -.1}, 0}, {{0, 0, .05}, {0, 0, .05}, 1}}};
  HeatBindingOptions o;
  o.operation = HeatOperator::BoundedFiniteElements;
  o.visibility = HeatVisibility::EntryFaces;
  o.seed_rule = HeatSeedRule::ReferenceSegments;
  o.maximum_seed_distance = 1000;
  std::function<bool()> cancel;
  auto selected = [&](const Mesh &mesh, std::span<const HeatSourceSegment> src,
                      HeatDistanceRule rule) {
    Budget budget{0, 100000, cancel};
    auto s = assemble(mesh, budget, o.operation, {});
    auto opts = o;
    opts.distance_rule = rule;
    seed(s, mesh, rig, opts, budget, src, {});
    return s;
  };
  auto reversed = m;
  std::swap(reversed.indices[1], reversed.indices[2]);
  auto signed_a = selected(m, sources, HeatDistanceRule::OutwardNormal),
       signed_b = selected(reversed, sources, HeatDistanceRule::OutwardNormal);
  auto unsigned_a = selected(m, sources, HeatDistanceRule::EuclideanSurface),
       unsigned_b =
           selected(reversed, sources, HeatDistanceRule::EuclideanSurface);
  if (!(signed_a.seeds[0][0] & 1) || !(signed_b.seeds[0][0] & 2) ||
      unsigned_a.seeds[0] != unsigned_b.seeds[0] ||
      !(unsigned_a.seeds[0][0] & 2))
    throw std::runtime_error("Signed control or common nearer-source "
                             "orientation independence was lost");
  const long double expected = (1.L / 6) / (.05L * .05L);
  if (std::abs(unsigned_a.heat[0] / expected - 1) > 2e-15L ||
      unsigned_a.heat[0] != unsigned_b.heat[0])
    throw std::runtime_error(
        "Unsigned surface H differs from independent physical distance/area");
  o.distance_rule = HeatDistanceRule::EuclideanSurface;
  auto original = heat_bind_surface(m, rig, o, {}, nullptr, sources),
       flip = heat_bind_surface(reversed, rig, o, {}, nullptr, sources);
  for (size_t i = 0; i < 3; ++i) {
    if (original.vertices[i].size() != flip.vertices[i].size())
      throw std::runtime_error(
          "Actual bind changed under source winding reversal");
    for (size_t j = 0; j < original.vertices[i].size(); ++j)
      if (original.vertices[i][j] != flip.vertices[i][j])
        throw std::runtime_error(
            "Actual bound field changed under source winding reversal");
  }
  for (double scale : {.01, 100.})
    for (double angle : {0., .37, 1.2}) {
      const double c = std::cos(angle), s = std::sin(angle);
      auto transform = [&](D p) {
        return D{scale * (c * p[0] + s * p[2]), scale * p[1],
                 scale * (-s * p[0] + c * p[2])};
      };
      auto mesh = m;
      auto src = sources;
      for (auto &p : mesh.positions)
        p = vec(transform(point(p)));
      for (auto &source : src) {
        source.head = transform(source.head);
        source.tail = transform(source.tail);
      }
      auto next = selected(mesh, src, HeatDistanceRule::EuclideanSurface);
      if (next.seeds[0] != unsigned_a.seeds[0] ||
          std::abs(next.heat[0] / unsigned_a.heat[0] - 1) > 2e-6)
        throw std::runtime_error("Unsigned source/H changed under coherent "
                                 "unit scale or rigid rotation");
    }
  HeatBindingReport report;
  report.work = 173;
  auto invalid = o;
  invalid.distance_rule = static_cast<HeatDistanceRule>(17);
  bool refused = false;
  try {
    heat_bind_surface(m, rig, invalid, {}, &report, sources);
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  if (!refused || report.work != 173)
    throw std::runtime_error("Invalid distance policy published a result");
  std::cout << "PASS signed wrong-source negative, independent nearer "
               "source/H, winding reversal, six scale/rotation and actual "
               "bounded bind/policy-refusal controls\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
