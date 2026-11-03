// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only production system/seed observation; no production testing hooks.
#include "../src/heat-binding.cpp"
#include <iostream>
using namespace dome;
using namespace dome::detail;
Mesh layers(bool connected, float gap = .1f) {
  Mesh m;
  for (int layer = 0; layer < 2; ++layer) {
    float e = layer ? 1 : .1f, z = layer ? gap : 0;
    uint32_t b = m.positions.size();
    for (auto p : std::array<Vec3, 4>{
             {{-e, -e, z}, {e, -e, z}, {e, e, z}, {-e, e, z}}}) {
      m.positions.push_back(p);
      m.normals.push_back({0, 0, 1});
      m.uv.push_back({0, 0});
    }
    if (layer)
      m.indices.insert(m.indices.end(), {b, b + 2, b + 1, b, b + 3, b + 2});
    else
      m.indices.insert(m.indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
  }
  if (connected)
    m.indices.insert(m.indices.end(), {1, 5, 6, 1, 6, 2});
  return m;
}
System sources(const Mesh &m, HeatOcclusion domain, bool &failed) {
  Skeleton rig;
  Bone b;
  b.name = "Physical point source";
  b.rest.translation = {0, 0, .2f};
  b.length = .05f;
  rig.bones.push_back(b);
  HeatSourceSegment source{{0, 0, .2}, {0, 0, .2}, 0};
  HeatBindingOptions o;
  o.operation = HeatOperator::PositiveGraph;
  o.visibility = HeatVisibility::EntryFaces;
  o.occlusion = domain;
  o.maximum_seed_distance = 2;
  std::function<bool()> cancel;
  Budget budget{0, 10'000'000, cancel};
  auto system = assemble(m, budget, o.operation, {});
  try {
    seed(system, m, rig, o, budget, {&source, 1}, {});
  } catch (const HeatSeedFailure &) {
    failed = true;
  }
  return system;
}
int main() try {
  bool failed = false;
  auto whole = sources(layers(false), HeatOcclusion::WholeMesh, failed);
  if (!failed || whole.heat[0] != 0)
    throw std::runtime_error(
        "Whole-owner entry-wall control no longer blocks lower source");
  failed = false;
  auto independent =
      sources(layers(false), HeatOcclusion::ConnectedSurface, failed);
  if (failed || independent.components != 2 || !(independent.heat[0] > 0))
    throw std::runtime_error(
        "Independent source surfaces did not remain separate/seeded");
  failed = false;
  auto self = sources(layers(true), HeatOcclusion::ConnectedSurface, failed);
  if (failed || self.components != 1 || self.heat[0] != 0 ||
      !(self.heat[4] > 0))
    throw std::runtime_error("Genuine same-surface entry occluder was ignored");
  std::cout << "PASS whole-owner blocker, explicit independent-layer policy, "
               "and actual connected entry-wall self-occlusion\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
