// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
namespace dome {
struct JointAdjustment {
  uint32_t joint = 0;
  // Local XYZ rotations, composed in X then Y then Z order after the current
  // local pose. Translation is in the joint parent's metric coordinate frame.
  Vec3 degrees{}, translation{};
};
struct MorphWeightEdit { uint32_t target = 0; float weight = 0; };
struct RigEdit {
  std::optional<JointAdjustment> joint;
  std::optional<MorphWeightEdit> morph;
  bool reset_pose = false;
  // Explicit group operation. Conventional parts require the same native rest
  // rig/order; imported source parts remap authenticated original node IDs across
  // distinct skin bind spaces. Names never reconstruct correspondence.
  bool linked_pose = true;
};
// Detached worker operation. Validate every affected part before publishing any
// replacement; retain rest geometry, paint, source identities and morph deltas.
void edit_rig(Scene &, uint64_t object, const RigEdit &,
              const std::function<bool()> &cancelled = {});
} // namespace dome
