// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "automatic-humanoid-bind.hpp"
#include "weight-nearest-support.hpp"
#include "weight-surface-system.hpp"
namespace dome::detail {
struct AutomaticHumanoidSupport {
  std::vector<uint8_t> joints, components;
  bool permits(size_t component, size_t bone) const {
    return components.empty() || !components[component] || !joints[bone] ||
           components[component] == joints[bone];
  }
};
// A component can establish a mirrored-leg family only when EVERY captured
// REST vertex has exactly the same nearest declared family. Exact cross-family
// ties or mixed components retain the complete original intrinsic domain.
// No per-vertex Euclidean seed or air shortcut is added to the surface graph.
inline AutomaticHumanoidSupport automatic_humanoid_support(
    std::span<const Vec3> points,
    const std::vector<std::vector<uint32_t>> &components,
    const Skeleton &rig, const HumanoidMapping &mapping,
    std::span<const std::array<Vec3, 2>> segments, BindOptions options,
    size_t &used, const std::function<bool()> &cancelled) {
  AutomaticHumanoidSupport out;
  const auto left = mapping.joints.find("leftUpperLeg");
  const auto right = mapping.joints.find("rightUpperLeg");
  if (left == mapping.joints.end() || right == mapping.joints.end())
    return out;
  mapping.validate(rig, false);
  weight_surface::Budget work{used, options.maximum_work, cancelled};
  work.step_items(rig.bones.size(), 64);
  out.joints.resize(rig.bones.size());
  for (size_t bone = 0; bone < rig.bones.size(); ++bone) {
    for (int ancestor = int(bone); ancestor >= 0;
         ancestor = rig.bones[ancestor].parent) {
      work.step();
      if (uint32_t(ancestor) == left->second) {
        out.joints[bone] = 1;
        break;
      }
      if (uint32_t(ancestor) == right->second) {
        out.joints[bone] = 2;
        break;
      }
    }
  }
  std::vector<HeatSourceSegment> sources;
  std::array<bool, 3> present{};
  std::array<double, 3> low{INFINITY, INFINITY, INFINITY};
  std::array<double, 3> high{-INFINITY, -INFINITY, -INFINITY};
  for (uint32_t bone = 0; bone < rig.bones.size(); ++bone) {
    work.step(16);
    if (!out.joints[bone] || !rig.bones[bone].deform)
      continue;
    present[out.joints[bone]] = true;
    const auto &a = segments[bone][0], &b = segments[bone][1];
    sources.push_back({{a.x, a.y, a.z}, {b.x, b.y, b.z}, bone});
    for (size_t k = 0; k < 3; ++k) {
      low[k] = std::min({low[k], sources.back().head[k], sources.back().tail[k]});
      high[k] = std::max({high[k], sources.back().head[k], sources.back().tail[k]});
    }
  }
  if (!present[1] || !present[2]) {
    used = work.used;
    return {};
  }
  // The exact nearest-source predicate's declared binary32 workspace is the
  // existing bounded mesh metric. An unusually distant/large authored rig is
  // prospectively outside this optional support policy; keep its original
  // intrinsic domain rather than retrying after a predicate or solve failure.
  for (const auto &source : sources)
    for (const auto &point : {source.head, source.tail})
      for (double coordinate : point)
        if (std::abs(coordinate) > 10000) {
          used = work.used;
          return {};
        }
  // Strictly outside this outward-rounded box, no leg source could have
  // received an original radius-bounded seed. Leave those components alone.
  for (size_t k = 0; k < 3; ++k) {
    low[k] = std::nextafter(low[k] - options.maximum_seed_distance, -INFINITY);
    high[k] = std::nextafter(high[k] + options.maximum_seed_distance, INFINITY);
  }
  work.step_items(components.size(), 2);
  out.components.resize(components.size());
  NearestWeightSources nearest(sources, work);
  for (size_t c = 0; c < components.size(); ++c) {
    work.stop();
    std::array<float, 3> component_low{INFINITY, INFINITY, INFINITY};
    std::array<float, 3> component_high{-INFINITY, -INFINITY, -INFINITY};
    for (auto vertex : components[c]) {
      work.step(8);
      const auto p = points[vertex];
      const std::array<float, 3> coordinate{p.x, p.y, p.z};
      for (size_t k = 0; k < 3; ++k) {
        component_low[k] = std::min(component_low[k], coordinate[k]);
        component_high[k] = std::max(component_high[k], coordinate[k]);
      }
    }
    bool outside = false;
    for (size_t k = 0; k < 3; ++k)
      outside |= component_high[k] < low[k] || component_low[k] > high[k];
    if (outside)
      continue;
    uint8_t family = 0;
    for (auto vertex : components[c]) {
      const auto p = points[vertex];
      auto closest = nearest.nearest({p.x, p.y, p.z}, work);
      uint8_t current = 0;
      for (size_t block = 0; block < 4; ++block) {
        auto bits = closest[block];
        while (bits) {
          work.step(4);
          const auto bone = uint32_t(block * 64 + std::countr_zero(bits));
          bits &= bits - 1;
          const auto side = out.joints.at(bone);
          current = current && current != side ? 3 : side;
          if (current == 3)
            break;
        }
        if (current == 3)
          break;
      }
      if (!current || current == 3 || (family && family != current)) {
        family = 0;
        break;
      }
      family = current;
    }
    out.components[c] = family;
  }
  used = work.used;
  return out;
}
}
