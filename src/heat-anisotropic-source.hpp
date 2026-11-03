// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "heat-capsule-source.hpp"
namespace dome::detail {
// A distinct finite-handle geometry, never selected from observed fit error.
// Transverse radius retains the bilateral/incident skeletal rule. A midpoint
// handle's longitudinal extent is half its authentic physical edge length;
// an axial segment's end cap uses the shortest incident half-edge. Genuine
// original point handles stay isotropic, and auxiliary metrics stay physical.
inline HeatCapsuleMetrics make_anisotropic_heat_metrics(
    std::span<const HeatSourceSegment> physical,
    std::span<const HeatSourceSegment> constructed, const Skeleton &rig,
    const HumanoidMapping &mapping, const HeatBindingOptions &options,
    const std::function<bool()> &cancelled = {}) {
  const size_t b = rig.bones.size();
  if (!b || b > 256 || physical.size() != constructed.size() ||
      physical.size() > b)
    throw std::invalid_argument("Invalid finite skeletal handle geometry");
  const size_t work = b * b * 32 + b * 128,
               bytes = b * (sizeof(RigMatrix) + sizeof(uint32_t)) + 4096;
  if (work >= options.maximum_work || bytes >= options.maximum_bytes)
    throw std::length_error("Finite handle construction exceeds work/bytes");
  auto remaining = options;
  remaining.maximum_work -= work;
  remaining.maximum_bytes -= bytes;
  auto expected =
      make_axial_heat_handles(physical, rig, mapping, remaining, cancelled);
  if (expected.handles.size() != constructed.size())
    throw std::invalid_argument("Constructed handle count changed");
  for (size_t i = 0; i < constructed.size(); ++i)
    if (expected.handles[i].bone != constructed[i].bone ||
        expected.handles[i].head != constructed[i].head ||
        expected.handles[i].tail != constructed[i].tail)
      throw std::invalid_argument(
          "Constructed finite handle differs from its role geometry");
  if (expected.work >= remaining.maximum_work ||
      expected.planned_bytes >= remaining.maximum_bytes)
    throw std::length_error(
        "Finite handle capture exceeds total resource caps");
  remaining.maximum_work -= expected.work;
  remaining.maximum_bytes -= expected.planned_bytes;
  auto out = make_heat_capsule_metrics(constructed, rig, mapping, remaining,
                                       cancelled);
  out.work += work + expected.work;
  out.planned_bytes += bytes + expected.planned_bytes;
  auto world = rig.world();
  using Point = std::array<double, 3>;
  auto head = [&](size_t i) {
    return Point{world.at(i)[12], world.at(i)[13], world.at(i)[14]};
  };
  auto subtract = [](Point x, Point y) {
    return Point{x[0] - y[0], x[1] - y[1], x[2] - y[2]};
  };
  auto length = [](Point x) { return std::hypot(x[0], x[1], x[2]); };
  for (size_t i = 0; i < physical.size(); ++i) {
    if (cancelled && cancelled())
      throw std::runtime_error("Finite handle capture cancelled");
    const auto &source = physical[i];
    if (source.bone >= b || source.head != head(source.bone))
      throw std::invalid_argument(
          "Physical handle lacks its exact current joint head");
    bool tail = source.tail == source.head;
    for (size_t j = 0; j < b; ++j)
      tail |= rig.bones[j].parent == int(source.bone) && source.tail == head(j);
    if (!tail)
      throw std::invalid_argument(
          "Physical handle tail lacks its current child identity");
    auto &metric = out.metrics[source.bone];
    if (metric.kind == HeatSourceMetricKind::Physical)
      continue;
    auto axis = subtract(source.tail, source.head);
    double extent = length(axis);
    if (!extent)
      continue; // Deliberately isotropic actual point-handle class.
    for (double &x : axis)
      x /= extent;
    double parallel = extent / 2;
    if (constructed[i].head != constructed[i].tail)
      for (size_t j = 0; j < b; ++j)
        if (rig.bones[source.bone].parent == int(j) ||
            rig.bones[j].parent == int(source.bone)) {
          double incident = length(subtract(head(j), head(source.bone)));
          if (incident > 0)
            parallel = std::min(parallel, incident / 2);
        }
    const double transverse = out.radii[source.bone],
                 a = 1 / (transverse * transverse),
                 d = 1 / (parallel * parallel) - a;
    if (!(parallel > 0) || !std::isfinite(a + d))
      throw std::invalid_argument("Finite source metric is not representable");
    for (int row = 0; row < 3; ++row)
      for (int col = row; col < 3; ++col) {
        double value = (row == col ? a : 0) + d * axis[row] * axis[col];
        metric.precision[row * 3 + col] = metric.precision[col * 3 + row] =
            value;
      }
  }
  return out;
}
} // namespace dome::detail
