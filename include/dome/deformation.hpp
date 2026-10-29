// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "avatar.hpp"
#include <optional>
namespace dome {
// Detached editable value. Scene owns only const instances, so history and
// worker snapshots can share authoring data without exposing mutable aliases.
struct DeformationState {
  uint64_t source = 0;
  std::optional<Skeleton> skeleton;
  std::optional<SkinWeights> skin;
  std::optional<MorphSet> morphs;
  std::optional<HumanoidMapping> humanoid;
  std::optional<HumanLandmarks> landmarks;
  std::optional<AvatarExpressions> expressions;
  // Empty vectors mean rest pose/zero morphs. These never replace rest
  // geometry.
  std::vector<JointTransform> pose;
  std::vector<float> morph_weights;
  DeformationState() = default;
  explicit DeformationState(const Mesh &rest);
  void validate(const Mesh &rest) const;
  size_t resource_entries() const;
  Mesh preview(const Mesh &rest) const;
  nlohmann::json encode(const Mesh &rest) const;
  static DeformationState decode(const nlohmann::json &, const Mesh &rest);
};
} // namespace dome
