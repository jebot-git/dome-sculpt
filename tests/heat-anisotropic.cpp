// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/heat-binding.cpp"
#include "heat-anisotropic-source.hpp"
#include <iostream>
using namespace dome;
using namespace dome::detail;
int main() try {
  const std::array<D, 17> heads{{{0, 0, 0},
                                 {0, 1, 0},
                                 {0, 2, 0},
                                 {.5, 2.5, 0},
                                 {-.5, 2.5, 0},
                                 {1, 2.5, 0},
                                 {-1, 2.5, 0},
                                 {2, 2.5, 0},
                                 {-2, 2.5, 0},
                                 {0, 3, 0},
                                 {0, 4, 0},
                                 {.4, 0, 0},
                                 {-.4, 0, 0},
                                 {.4, -1, 0},
                                 {-.4, -1, 0},
                                 {3, 1, 0},
                                 {0, 1.5, 0}}};
  const std::array<int, 17> parents{-1, 0, 16, 2, 2,  3,  4, 5, 6,
                                    2,  9, 0,  0, 11, 12, 0, 1};
  Skeleton rig;
  rig.bones.resize(17);
  for (size_t i = 0; i < 17; ++i) {
    rig.bones[i].name = "Unrelated label " + std::to_string(17 - i);
    rig.bones[i].parent = parents[i];
    rig.bones[i].deform = i == 2 || i == 3 || i == 15;
    D local = heads[i];
    if (parents[i] >= 0)
      local = sub(local, heads[size_t(parents[i])]);
    rig.bones[i].rest.translation = vec(local);
  }
  HumanoidMapping roles;
  roles.joints = {
      {"chest", 16},        {"hips", 0},           {"spine", 1},
      {"upperChest", 2},    {"leftShoulder", 3},   {"rightShoulder", 4},
      {"leftUpperArm", 5},  {"rightUpperArm", 6},  {"leftLowerArm", 7},
      {"rightLowerArm", 8}, {"neck", 9},           {"head", 10},
      {"leftUpperLeg", 11}, {"rightUpperLeg", 12}, {"leftLowerLeg", 13},
      {"rightLowerLeg", 14}};
  std::array<HeatSourceSegment, 3> physical{{{heads[2], heads[9], 2},
                                             {heads[3], heads[5], 3},
                                             {heads[15], heads[15], 15}}};
  HeatBindingOptions o;
  o.operation = HeatOperator::BoundedFiniteElements;
  o.distance_rule = HeatDistanceRule::EuclideanSurface;
  o.visibility = HeatVisibility::EntryFaces;
  o.occlusion = HeatOcclusion::ConnectedSurface;
  o.seed_rule = HeatSeedRule::ReferenceSegments;
  o.maximum_seed_distance = 100;
  auto h = make_axial_heat_handles(physical, rig, roles, o);
  auto metric =
      make_anisotropic_heat_metrics(physical, h.handles, rig, roles, o);
  const std::array<double, 17> expected{.4, .7, 1,  .25, .25, .25, .25, .5, .5,
                                        .5, .5, .2, .2,  .5,  .5,  0,   .85};
  long double logs = 0;
  for (size_t i = 0; i < 17; ++i) {
    if (std::abs(metric.radii[i] - expected[i]) > 2e-8)
      throw std::runtime_error("Capsule radii differ from independent "
                               "incident-edge/bilateral oracle");
    if (expected[i])
      logs += std::log(expected[i]);
  }
  const long double reference = std::exp(logs / 16);
  if (std::abs(metric.reference_length / reference - 1) > 2e-8 ||
      metric.metrics[15].kind != HeatSourceMetricKind::Physical)
    throw std::runtime_error(
        "Reference scale or auxiliary physical metric changed");
  Mesh m;
  m.positions = {{.6f, 3.4f, .1f}, {1.6f, 3.4f, .1f}, {.6f, 4.4f, .1f}};
  m.normals.assign(3, {0, 0, 1});
  m.uv.assign(3, {0, 0});
  m.indices = {0, 1, 2};
  std::function<bool()> cancel;
  auto get = [&](const Mesh &mesh, const Skeleton &r, auto src, auto metrics) {
    Budget budget{0, o.maximum_work, cancel};
    auto system = assemble(mesh, budget, o.operation, {});
    seed(system, mesh, r, o, budget, src, {}, metrics);
    return system;
  };
  auto plain = get(m, rig, std::span<const HeatSourceSegment>(h.handles),
                   std::span<const HeatSourceMetric>{});
  auto typed = get(m, rig, std::span<const HeatSourceSegment>(h.handles),
                   std::span<const HeatSourceMetric>(metric.metrics));
  (void)plain;
  if (metric.metrics[2].precision !=
      std::array<double, 9>{1, 0, 0, 0, 16, 0, 0, 0, 1})
    throw std::runtime_error(
        "Finite axial metric differs from analytic diagonal");
  if (!(typed.seeds[0][0] & (1ULL << 2)))
    throw std::runtime_error(
        "Mixed role capsule/source choice differs from analytic distances");
  long double squared =
      static_cast<long double>(m.positions[0].x) * m.positions[0].x +
      static_cast<long double>(m.positions[0].z) * m.positions[0].z;
  squared += 16 * (static_cast<long double>(m.positions[0].y) - 3) *
             (static_cast<long double>(m.positions[0].y) - 3);
  long double heat = (1.L / 6) / (reference * reference * squared);
  if (std::abs(typed.heat[0] / heat - 1) > 2e-7)
    throw std::runtime_error(
        "Capsule H differs from independent physical-area oracle");
  for (double scale : {.01, 10.})
    for (double angle : {0., .37, 1.2}) {
      double c = std::cos(angle), s = std::sin(angle);
      auto transform = [&](D p) {
        return D{scale * (c * p[0] + s * p[2]), scale * p[1],
                 scale * (-s * p[0] + c * p[2])};
      };
      auto next_rig = rig;
      auto next_mesh = m;
      auto src = physical;
      for (auto &b : next_rig.bones)
        b.rest.translation = vec(transform(point(b.rest.translation)));
      for (auto &p : next_mesh.positions)
        p = vec(transform(point(p)));
      for (auto &source : src) {
        source.head = transform(source.head);
        source.tail = transform(source.tail);
      }
      auto next_world = next_rig.world();
      const std::array<uint32_t, 3> tails{9, 5, 15};
      for (size_t i = 0; i < src.size(); ++i) {
        const auto &a = next_world[src[i].bone];
        const auto &b = next_world[tails[i]];
        src[i].head = {a[12], a[13], a[14]};
        src[i].tail = {b[12], b[13], b[14]};
      }
      auto next_h = make_axial_heat_handles(src, next_rig, roles, o);
      auto next_metric = make_anisotropic_heat_metrics(src, next_h.handles,
                                                       next_rig, roles, o);
      auto result = get(next_mesh, next_rig,
                        std::span<const HeatSourceSegment>(next_h.handles),
                        std::span<const HeatSourceMetric>(next_metric.metrics));
      if (result.seeds[0] != typed.seeds[0] ||
          std::abs(result.heat[0] / typed.heat[0] - 1) > 4e-6)
        throw std::runtime_error(
            "Capsule source/H changed under coherent scale/rotation");
      for (size_t i = 0; i < 17; ++i)
        if (expected[i] &&
            std::abs(next_metric.radii[i] / (scale * metric.radii[i]) - 1) >
                2e-6)
          throw std::runtime_error(
              "Capsule radius failed scale/rotation covariance");
    }
  for (int mode = 0; mode < 4; ++mode) {
    auto cap = o;
    auto role_copy = roles;
    bool failed = false;
    if (mode == 0)
      cap.maximum_work = metric.work - 1;
    if (mode == 1)
      cap.maximum_bytes = metric.planned_bytes - 1;
    if (mode == 3)
      role_copy.joints.erase("rightUpperArm");
    try {
      make_anisotropic_heat_metrics(physical, h.handles, rig, role_copy, cap,
                                    [mode] { return mode == 2; });
    } catch (const std::exception &) {
      failed = true;
    }
    if (!failed)
      throw std::runtime_error(
          "Capsule work/byte/cancel/source-role guard failed");
  }
  auto forged = metric.metrics;
  forged[2].source.tail[0] += .01;
  HeatBindingReport report;
  report.work = 173;
  bool refused = false;
  try {
    heat_bind_surface(m, rig, o, {}, &report, h.handles, {}, {}, forged);
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  if (!refused || report.work != 173)
    throw std::runtime_error("Forged metric source published output");
  std::cout << "PASS finite anisotropic axial cap, independent "
               "bilateral/incident radii, geometric-mean "
               "scale, mixed-role source/H, six scales/rotations, "
               "work/bytes/cancel/missing/forged-source controls\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
