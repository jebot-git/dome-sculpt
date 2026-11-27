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
// Targets share either an explicit native group and rig or one imported avatar
// instance. Hidden and locked parts stay outside the stroke.
std::vector<uint64_t> weight_objects(const Scene &, uint64_t object, bool linked);
std::optional<uint32_t> weight_joint_slot(const SculptObject &source,
                                         const SculptObject &target, uint32_t);
// Brush coordinates use the selected object's local frame for similarity
// placement and scene metres for an affine-placed object.
WeightEditResult paint_scene_weights(Scene &, uint64_t object,
                                    std::span<const WeightBrush>,
                                    const WeightToolSettings &,
                                    const std::function<bool()> &cancelled = {});
} // namespace dome
