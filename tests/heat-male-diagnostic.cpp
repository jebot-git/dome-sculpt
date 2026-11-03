// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only direct observation of private production matrix/source control
// flow.
#include "../src/heat-binding.cpp"
#include "dome/creation.hpp"
#include "heat-seams.hpp"
#include "heat-source.hpp"
#include <iomanip>
#include <iostream>
using namespace dome;
using namespace dome::detail;
void rows(const DeformationState &d, const VertexWeights &w) {
  for (auto i : w)
    std::cout << ' ' << i.joint << ':' << d.skeleton->bones[i.joint].name << '='
              << i.weight;
  std::cout << '\n';
}
int main() try {
  Scene scene;
  append_humanoid(scene, humanoid_baseline_defaults("vroid-beta-male"),
                  "Actual male diagnosis", {}, 64);
  std::cout << std::setprecision(10);
  for (const auto &o : scene.objects()) {
    if (o.id != 12 && o.id != 16 && o.id != 18)
      continue;
    const auto &m = o.painted->mesh();
    auto &d = *o.deformation;
    HeatBindingOptions options;
    options.operation = HeatOperator::PositiveGraph;
    options.visibility = HeatVisibility::EntryFaces;
    options.maximum_seed_distance = 1;
    auto profile = imported_heat_profile(*d.skeleton, *o.appearance, options,
                                         {}, &*d.humanoid);
    auto seams = capture_heat_seams(o, options);
    std::cout << "PART " << o.id << " joined=" << seams.joined_edges()
              << " rejected=" << seams.rejected_edges()
              << " ambiguous=" << seams.ambiguous_edges() << '\n';
    if (o.id == 18) {
      std::function<bool()> cancel;
      Budget budget{0, options.maximum_work, cancel};
      auto system = assemble(m, budget, options.operation, seams.groups(o));
      try {
        seed(system, m, *d.skeleton, options, budget, profile.segments,
             profile.eligible);
      } catch (const HeatSeedFailure &e) {
        std::cout << "UNSEEDED root=" << e.component << " rows=" << e.rows
                  << " entries=" << e.entry_hits << " exits=" << e.exit_hits
                  << " own_hits=" << e.own_hits
                  << " foreign_hits=" << e.foreign_hits << '\n';
        VertexWeights first;
        bool uniform = true;
        for (size_t r = 0; r < system.parent.size(); ++r)
          if (system.parent[r] == e.component) {
            auto at = std::find(system.raw_to_row.begin(),
                                system.raw_to_row.end(), r);
            size_t raw = at - system.raw_to_row.begin();
            auto p = m.positions[raw];
            std::cout << "ROW " << r << " raw=" << raw << " p=" << p.x << ','
                      << p.y << ',' << p.z << " ARTIST";
            rows(d, d.skin->vertices[raw]);
            auto row = d.skin->vertices[raw];
            std::sort(row.begin(), row.end(),
                      [](auto a, auto b) { return a.joint < b.joint; });
            if (first.empty())
              first = row;
            else
              uniform &= row == first;
          }
        std::cout << "UNIFORM_ATTACHMENT " << uniform << '\n';
      }
      continue;
    }
    auto weights =
        heat_bind_surface(m, *d.skeleton, options, {}, nullptr,
                          profile.segments, profile.eligible, seams.groups(o));
    auto original = d, next = d;
    next.skin = weights;
    for (auto *x : {&original, &next}) {
      x->pose.clear();
      for (auto &b : x->skeleton->bones)
        x->pose.push_back(b.rest);
      for (auto [role, axis, degrees] :
           std::array<std::tuple<const char *, Vec3, double>, 3>{
               {{"spine", {0, 0, 1}, 15},
                {"hips", {0, 1, 0}, 10},
                {"leftShoulder", {0, 0, 1}, 20}}}) {
        auto i = x->humanoid->joints.find(role);
        if (i == x->humanoid->joints.end())
          continue;
        double a = degrees * std::numbers::pi / 360;
        Quat q{float(std::cos(a)), float(axis.x * std::sin(a)),
               float(axis.y * std::sin(a)), float(axis.z * std::sin(a))};
        x->pose[i->second].rotation = q * x->pose[i->second].rotation;
      }
    }
    auto a = original.preview(m), b = next.preview(m);
    size_t worst = 0;
    double error = 0;
    for (size_t i = 0; i < a.positions.size(); ++i) {
      double value = length(a.positions[i] - b.positions[i]);
      if (value > error) {
        worst = i;
        error = value;
      }
    }
    auto p = m.positions[worst];
    std::cout << "WORST " << worst << " error=" << error << " bind=" << p.x
              << ',' << p.y << ',' << p.z << " ARTIST";
    rows(d, d.skin->vertices[worst]);
    std::cout << "HEAT";
    rows(d, weights.vertices[worst]);
  }
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
