// SPDX-License-Identifier: GPL-3.0-or-later
#include "heat-binding.hpp"
#include <iostream>
using namespace dome;
using namespace dome::detail;
Mesh layers(bool entry) {
  Mesh m;
  for (int layer = 0; layer < 2; ++layer) {
    float extent = layer ? 1 : .1f, z = layer ? .1f : 0;
    uint32_t base = m.positions.size();
    for (auto p : std::array<Vec3, 4>{{{-extent, -extent, z},
                                       {extent, -extent, z},
                                       {extent, extent, z},
                                       {-extent, extent, z}}}) {
      m.positions.push_back(p);
      m.normals.push_back({0, 0, 1});
      m.uv.push_back({0, 0});
    }
    if (layer && entry)
      m.indices.insert(m.indices.end(),
                       {base, base + 2, base + 1, base, base + 3, base + 2});
    else
      m.indices.insert(m.indices.end(),
                       {base, base + 1, base + 2, base, base + 2, base + 3});
  }
  return m;
}
int main() try {
  Skeleton rig;
  Bone source;
  source.name = "Exact point source";
  source.rest.translation = {0, 0, .2f};
  source.length = .05f;
  rig.bones.push_back(source);
  HeatSourceSegment segment{{0, 0, .2}, {0, 0, .2}, 0};
  HeatBindingOptions o;
  o.operation = HeatOperator::PositiveGraph;
  o.maximum_seed_distance = 2;
  auto exit = layers(false);
  HeatBindingReport old{9};
  bool failed = false;
  try {
    heat_bind_surface(exit, rig, o, {}, &old, {&segment, 1});
  } catch (const HeatSeedFailure &e) {
    failed = e.rows == 4 && e.exit_hits > 0 && e.entry_hits == 0;
    std::cout << "CONTROL component_rows=" << e.rows << " exits=" << e.exit_hits
              << " entries=" << e.entry_hits << '\n';
  }
  if (!failed || old.vertices != 9)
    throw std::runtime_error(
        "All-face control failed to expose actual exit-only occlusion");
  o.visibility = HeatVisibility::EntryFaces;
  HeatBindingReport report;
  auto solved = heat_bind_surface(exit, rig, o, {}, &report, {&segment, 1});
  if (report.components != 2)
    throw std::runtime_error("Entry-visibility component count changed");
  for (auto &row : solved.vertices)
    if (row.size() != 1 || row[0].joint != 0 || row[0].weight != 1)
      throw std::runtime_error(
          "Exit-facing layer altered the single-source field");
  auto entry = layers(true);
  failed = false;
  try {
    heat_bind_surface(entry, rig, o, {}, nullptr, {&segment, 1});
  } catch (const HeatSeedFailure &e) {
    failed = e.rows == 4 && e.entry_hits > 0 && e.exit_hits == 0;
    std::cout << "NEGATIVE component_rows=" << e.rows
              << " exits=" << e.exit_hits << " entries=" << e.entry_hits
              << '\n';
  }
  if (!failed)
    throw std::runtime_error("Entry-facing occluder was silently ignored");
  std::cout << "PASS actual exit/entry facing visibility controls and exact "
               "single-source solution\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
