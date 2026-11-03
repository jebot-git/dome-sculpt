// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/heat-binding.cpp"
#include "../src/sampled-anatomical-domain.hpp"
#include "dome/creation.hpp"
#include "heat-component-intent.hpp"
#include "heat-seams.hpp"
#include "heat-source.hpp"
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
using namespace dome::detail;
int main(int argc, char **argv) try {
  std::string gender = argc > 1 ? argv[1] : "male";
  if (gender != "male" && gender != "female")
    throw std::invalid_argument("Unknown fixture asset");
  Scene scene;
  append_humanoid(scene,
                  gender == "male"
                      ? humanoid_baseline_defaults("vroid-beta-male")
                      : HumanoidControls{},
                  "Actual domain audit", {}, 64);
  auto before = scene.encode();
  size_t parts = 0, unresolved = 0, collisions = 0;
  std::cout
      << std::setprecision(12)
      << "CONSTRUCTION_ONLY no_weight_values_in_domain=1 no_admission=1\n";
  for (const auto &o : scene.objects()) {
    if (!o.painted || !o.deformation || !o.deformation->skin ||
        !o.deformation->humanoid)
      continue;
    HeatBindingOptions options;
    options.maximum_work = 500'000'000;
    options.maximum_bytes = 512u * 1024u * 1024u;
    options.maximum_seed_distance = 1;
    options.visibility = HeatVisibility::EntryFaces;
    options.occlusion = HeatOcclusion::ConnectedSurface;
    options.distance_rule = HeatDistanceRule::EuclideanSurface;
    auto profile = imported_heat_profile(
        *o.deformation->skeleton, *o.appearance, options, {},
        &*o.deformation->humanoid, ImportedHeatPolicy::OriginalPrimitive);
    auto seams = capture_heat_seams(o, options);
    auto intent = capture_imported_component_intent(o, seams, options);
    std::function<bool()> cancelled;
    Budget budget{0, options.maximum_work, cancelled};
    budget.step(profile.work + seams.work() + intent.work());
    auto s = assemble(o.painted->mesh(), budget, HeatOperator::FiniteElements,
                      seams.groups(o), HeatNormals::UnitFaces,
                      HeatMass::MixedVoronoi);
    std::vector<std::array<uint64_t, 4>> usage(s.positions.size());
    std::vector<uint8_t> assigned(s.positions.size());
    auto masks = intent.masks(o);
    for (size_t raw = 0; raw < masks.size(); ++raw) {
      budget.step(8);
      auto root = s.parent[s.raw_to_row[raw]];
      if (assigned[root] && usage[root] != masks[raw])
        throw std::runtime_error("Current component intent varies");
      assigned[root] = 1;
      usage[root] = masks[raw];
    }
    const auto &rig = *o.deformation->skeleton;
    auto world = rig.world();
    std::vector<std::array<double, 3>> axes(profile.segments.size());
    for (size_t at = 0; at < profile.segments.size(); ++at) {
      const auto &source = profile.segments[at];
      int parent = rig.bones[source.bone].parent;
      size_t depth = 0;
      while (parent >= 0) {
        budget.step(16);
        if (size_t(parent) >= rig.bones.size() || ++depth > rig.bones.size())
          throw std::runtime_error("Current point-axis ancestry invalid");
        std::array<double, 3> v{source.head[0] - world[parent][12],
                                source.head[1] - world[parent][13],
                                source.head[2] - world[parent][14]};
        if (std::hypot(v[0], v[1], v[2]) > 0) {
          axes[at] = v;
          break;
        }
        parent = rig.bones[parent].parent;
      }
    }
    auto domain = diagnose_sampled_anatomical_domain(
        s, o.painted->mesh(), profile.segments, axes, usage, options, budget);
    ++parts;
    unresolved += !domain.all_axially_complete;
    std::cout << "PART " << o.id << ' ' << o.name
              << " rows=" << s.positions.size()
              << " sources=" << profile.segments.size()
              << " h=" << domain.mean_edge << " samples=" << domain.points
              << " rays=" << domain.rays
              << " axial_complete=" << domain.all_axially_complete
              << " work=" << budget.used << '\n';
    for (const auto &r : domain.records)
      std::cout << "SUPPORT bone=" << r.bone << " component=" << r.component
                << " kind=" << int(r.support) << " length=" << r.physical_length
                << " begin=" << r.supported_begin << " end=" << r.supported_end
                << " samples=" << r.samples << '\n';
    std::vector<uint32_t> joints;
    for (uint32_t j = 0; j < rig.bones.size(); ++j)
      if (profile.eligible[j])
        joints.push_back(j);
    for (auto j : joints) {
      std::cout << "COLUMN " << j << " rows=";
      for (size_t row = 0; row < s.positions.size(); ++row) {
        budget.step();
        if (domain.candidate_anchors[row][j / 64] & (uint64_t(1) << (j % 64)))
          std::cout << row << ',';
      }
      std::cout << '\n';
    }
    for (size_t a = 0; a < joints.size(); ++a)
      for (size_t b = a + 1; b < joints.size(); ++b) {
        bool equal = true, nonempty = false;
        for (size_t row = 0; row < s.positions.size(); ++row) {
          budget.step(2);
          bool x = domain.candidate_anchors[row][joints[a] / 64] &
                   (uint64_t(1) << (joints[a] % 64));
          bool y = domain.candidate_anchors[row][joints[b] / 64] &
                   (uint64_t(1) << (joints[b] % 64));
          nonempty |= x;
          if (x != y) {
            equal = false;
            break;
          }
        }
        if (equal && nonempty) {
          ++collisions;
          std::cout << "COLLISION " << joints[a] << ',' << joints[b] << '\n';
        }
      }
    std::cout << std::flush;
  }
  bool exact = scene.encode() == before;
  std::cout << "SUMMARY parts=" << parts
            << " unresolved_axial_parts=" << unresolved
            << " nonempty_collisions=" << collisions
            << " original_scene_exact=" << exact << " no_field_generated=1\n";
  return !exact || parts != 19;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
