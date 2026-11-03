// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "heat-axial-source.hpp"
namespace dome::detail {
struct HeatCapsuleMetrics {
  std::vector<HeatSourceMetric> metrics;
  std::vector<double> radii;
  size_t work = 0, planned_bytes = 0;
  double reference_length = 0;
};
// Explicit radius-normalized distance to algorithmic source segments. These
// capsules are skeletal handles, NOT a closed-material/volume certificate.
// Torso width comes from authenticated bilateral hip/shoulder chains; other
// primary radii are half the shortest nonzero incident skeletal edge. Genuine
// auxiliary sources keep the physical metric. No artist values/surface fit.
inline HeatCapsuleMetrics
make_heat_capsule_metrics(std::span<const HeatSourceSegment> sources,
                          const Skeleton &rig, const HumanoidMapping &mapping,
                          const HeatBindingOptions &options,
                          const std::function<bool()> &cancelled = {}) {
  if (cancelled && cancelled())
    throw std::runtime_error("Heat capsule capture cancelled");
  const size_t b = rig.bones.size();
  if (!b || b > 256 || mapping.joints.empty() || mapping.joints.size() > 55 ||
      sources.empty() || sources.size() > b)
    throw std::invalid_argument("Invalid bounded skeleton capsule domain");
  const size_t work = b * b * 64 + b * 256,
               bytes = b * (sizeof(HeatSourceMetric) + sizeof(double) +
                            sizeof(RigMatrix) + 512) +
                       8192;
  if (work > options.maximum_work || bytes > options.maximum_bytes)
    throw std::length_error("Heat capsule construction exceeds work/byte cap");
  mapping.validate(rig, false);
  auto world = rig.world();
  using Point = std::array<double, 3>;
  auto head = [&](uint32_t joint) {
    return Point{world.at(joint)[12], world.at(joint)[13], world.at(joint)[14]};
  };
  auto minus = [](Point x, Point y) {
    return Point{x[0] - y[0], x[1] - y[1], x[2] - y[2]};
  };
  auto dot = [](Point x, Point y) {
    return x[0] * y[0] + x[1] * y[1] + x[2] * y[2];
  };
  auto role = [&](const char *name) {
    auto at = mapping.joints.find(name);
    if (at == mapping.joints.end())
      throw std::invalid_argument("Capsule metric lacks bilateral axial roles");
    return head(at->second);
  };
  const auto hips = role("hips"), upper = role("upperChest"),
             axis = minus(upper, hips);
  const double axis2 = dot(axis, axis),
               hip_width =
                   std::sqrt(dot(
                       minus(role("leftUpperLeg"), role("rightUpperLeg")),
                       minus(role("leftUpperLeg"), role("rightUpperLeg")))) /
                   2,
               shoulder_width =
                   std::sqrt(dot(
                       minus(role("leftUpperArm"), role("rightUpperArm")),
                       minus(role("leftUpperArm"), role("rightUpperArm")))) /
                   2;
  if (!(axis2 > 0) || !(hip_width > 0) || !(shoulder_width > 0) ||
      !std::isfinite(axis2 + hip_width + shoulder_width))
    throw std::invalid_argument("Degenerate bilateral capsule geometry");
  HeatCapsuleMetrics out;
  out.work = work;
  out.planned_bytes = bytes;
  out.radii.resize(b);
  out.metrics.resize(b);
  long double log_sum = 0;
  for (const auto &[name, joint] : mapping.joints) {
    if (cancelled && cancelled())
      throw std::runtime_error("Heat capsule capture cancelled");
    double radius = INFINITY;
    const bool torso = name == "hips" || name == "spine" || name == "chest" ||
                       name == "upperChest";
    if (torso) {
      double t =
          std::clamp(dot(minus(head(joint), hips), axis) / axis2, 0., 1.);
      radius = (1 - t) * hip_width + t * shoulder_width;
    } else {
      for (uint32_t other = 0; other < b; ++other)
        if (rig.bones[joint].parent == int(other) ||
            rig.bones[other].parent == int(joint)) {
          auto delta = minus(head(other), head(joint));
          double length = std::sqrt(dot(delta, delta));
          if (length > 0)
            radius = std::min(radius, length / 2);
        }
    }
    if (!(radius > 0) || !std::isfinite(radius))
      throw std::invalid_argument(
          "Primary capsule lacks finite incident geometry");
    out.radii[joint] = radius;
    log_sum += std::log(radius);
  }
  out.reference_length = std::exp(double(log_sum / mapping.joints.size()));
  if (!(out.reference_length > 0) || !std::isfinite(out.reference_length))
    throw std::invalid_argument("Capsule common reference scale is invalid");
  std::array<bool, 256> seen{};
  for (const auto &source : sources) {
    if (source.bone >= b || seen[source.bone])
      throw std::invalid_argument(
          "Capsule source identities are invalid/repeated");
    seen[source.bone] = true;
    auto &metric = out.metrics[source.bone];
    metric.source = source;
    if (out.radii[source.bone]) {
      const double radius = out.radii[source.bone],
                   precision = 1 / (radius * radius);
      if (!std::isfinite(precision) || !(precision > 0))
        throw std::invalid_argument("Capsule precision is not representable");
      metric.kind = HeatSourceMetricKind::Envelope;
      metric.reference_length = out.reference_length;
      metric.precision = {precision, 0, 0, 0, precision, 0, 0, 0, precision};
    }
  }
  return out;
}
} // namespace dome::detail
