// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
#include "weight-edit.hpp"
namespace dome {
struct WeightToolSettings {
  bool enabled = false, linked = true, symmetry = false;
  WeightBrushMode mode = WeightBrushMode::Add;
  float strength = .25f, value = 1, symmetry_plane = 0;
  WeightLimits limits;
  std::vector<uint8_t> locks;
  void validate(size_t bones) const;
};
// Exact native rig/order within the explicit group. Hidden/locked siblings do
// not participate. No source IDs, rest geometry or morph targets are changed.
std::vector<uint64_t> weight_objects(const Scene &, uint64_t object, bool linked);
WeightEditResult paint_scene_weights(Scene &, uint64_t object,
                                    std::span<const WeightBrush>,
                                    const WeightToolSettings &,
                                    const std::function<bool()> &cancelled = {});
} // namespace dome
