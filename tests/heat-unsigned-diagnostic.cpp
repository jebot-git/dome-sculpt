// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only direct observation of private production matrix/source control
// flow.
#include "../src/heat-binding.cpp"
#include "dome/creation.hpp"
#include "heat-midpoint-source.hpp"
#include "heat-seams.hpp"
#include "heat-source.hpp"
#include <iomanip>
#include <iostream>
#include <set>
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
    if (o.id != 12 && o.id != 16 && o.id != 17)
      continue;
    const auto &m = o.painted->mesh();
    auto &d = *o.deformation;
    HeatBindingOptions options;
    options.operation = HeatOperator::BoundedFiniteElements;
    options.visibility = HeatVisibility::EntryFaces;
    options.maximum_seed_distance = 1;
    options.occlusion = HeatOcclusion::ConnectedSurface;
    options.geometry_normals = HeatNormals::UnitFaces;
    options.mass = HeatMass::MixedVoronoi;
    options.edges = HeatEdges::CotangentHalf;
    options.heat_strength = 4;
    options.seed_rule = HeatSeedRule::ReferenceSegments;
    options.distance_rule = HeatDistanceRule::EuclideanSurface;
    auto profile = imported_heat_profile(*d.skeleton, *o.appearance, options,
                                         {}, &*d.humanoid,
                                         ImportedHeatPolicy::OriginalPrimitive);
    std::set<uint32_t> ids;
    for (const auto &[role, joint] : d.humanoid->joints) {
      (void)role;
      ids.insert(joint);
    }
    std::vector<uint32_t> primary(ids.begin(), ids.end());
    auto construction =
        make_heat_handles(profile.segments, primary, d.skeleton->bones.size(),
                          HeatHandlePolicy::PrimaryMidpoints, options);
    auto seams = capture_heat_seams(o, options);
    std::cout << "PART " << o.id << " joined=" << seams.joined_edges()
              << " rejected=" << seams.rejected_edges()
              << " ambiguous=" << seams.ambiguous_edges() << '\n';
    if (o.id == 18) {
      std::function<bool()> cancel;
      Budget budget{0, options.maximum_work, cancel};
      auto system = assemble(m, budget, options.operation, seams.groups(o));
      try {
        seed(system, m, *d.skeleton, options, budget, construction.handles,
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
    auto weights = heat_bind_surface(m, *d.skeleton, options, {}, nullptr,
                                     construction.handles, profile.eligible,
                                     seams.groups(o));
    auto original = d, next = d;
    next.skin = weights;
    for (auto *x : {&original, &next}) {
      x->pose.clear();
      for (auto &b : x->skeleton->bones)
        x->pose.push_back(b.rest);
      auto rotations =
          o.id == 17
              ? std::vector<std::tuple<const char *, Vec3, double>>{{"head",
                                                                     {0, 0, 1},
                                                                     30},
                                                                    {"neck",
                                                                     {0, 1, 0},
                                                                     -15}}
              : std::vector<std::tuple<const char *, Vec3, double>>{
                    {"spine", {0, 0, 1}, 15},
                    {"hips", {0, 1, 0}, 10},
                    {"leftShoulder", {0, 0, 1}, 20}};
      for (auto [role, axis, degrees] : rotations) {
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
    std::function<bool()> cancel;
    Budget budget{0, options.maximum_work, cancel};
    auto system =
        assemble(m, budget, options.operation, seams.groups(o),
                 options.geometry_normals, options.mass, options.edges);
    seed(system, m, *d.skeleton, options, budget, construction.handles,
         profile.eligible);
    size_t r = system.raw_to_row[worst];
    std::cout << "ROW id=" << r << " mass=" << system.mass[r]
              << " heat=" << system.heat[r] << " ties=" << system.ties[r]
              << " normal=" << system.normals[r][0] << ','
              << system.normals[r][1] << ',' << system.normals[r][2] << '\n';
    struct Candidate {
      long double distance, physical, cosine, t;
      uint32_t bone;
      std::array<double, 3> head, tail;
    };
    std::vector<Candidate> candidates;
    for (const auto &source : construction.handles) {
      std::array<long double, 3> v, e, q, delta;
      long double l2 = 0, numerator = 0;
      for (int k = 0; k < 3; ++k) {
        v[k] = system.positions[r][k];
        e[k] = (long double)source.tail[k] - source.head[k];
        l2 += e[k] * e[k];
        numerator += (v[k] - source.head[k]) * e[k];
      }
      long double t = l2 ? std::clamp(numerator / l2, 0.L, 1.L) : 0,
                  physical = 0, cosine = 0;
      for (int k = 0; k < 3; ++k) {
        q[k] = source.head[k] + t * e[k];
        delta[k] = v[k] - q[k];
        physical += delta[k] * delta[k];
        cosine += delta[k] * system.normals[r][k];
      }
      physical = std::sqrt(physical);
      cosine = physical ? cosine / physical : 0;
      candidates.push_back({physical, physical, cosine, t, source.bone,
                            source.head, source.tail});
    }
    std::sort(
        candidates.begin(), candidates.end(),
        [](const auto &a, const auto &b) { return a.distance < b.distance; });
    for (size_t k = 0; k < std::min(size_t(12), candidates.size()); ++k) {
      const auto &c = candidates[k];
      std::cout << "SOURCE " << c.bone << ':' << d.skeleton->bones[c.bone].name
                << " adjusted=" << c.distance << " physical=" << c.physical
                << " cosine=" << c.cosine << " t=" << c.t
                << " head=" << c.head[0] << ',' << c.head[1] << ',' << c.head[2]
                << " tail=" << c.tail[0] << ',' << c.tail[1] << ',' << c.tail[2]
                << " selected="
                << bool(system.seeds[r][c.bone / 64] &
                        (uint64_t(1) << (c.bone % 64)))
                << '\n';
    }
    std::vector<double> rhs(system.positions.size()), field(rhs.size());
    for (uint32_t bone = 0; bone < d.skeleton->bones.size(); ++bone)
      if (profile.eligible[bone]) {
        size_t hits = 0;
        long double integrated = 0;
        for (size_t row = 0; row < rhs.size(); ++row) {
          bool chosen =
              system.seeds[row][bone / 64] & (uint64_t(1) << (bone % 64));
          rhs[row] = chosen ? system.heat[row] / system.ties[row] : 0;
          hits += chosen;
          integrated += rhs[row];
        }
        if (!hits)
          continue;
        auto measured = solve(system, rhs, field, options, budget);
        auto bounded = solve_bounds(system, rhs, field, options, budget);
        (void)bounded;
        if (field[r] > .001)
          std::cout << "FIELD " << bone << ':' << d.skeleton->bones[bone].name
                    << " raw=" << field[r] << " source_rows=" << hits
                    << " integrated_heat=" << integrated
                    << " residual=" << measured.residual << '\n';
      }
  }
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
