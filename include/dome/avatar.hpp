// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rig.hpp"
#include <map>
#include <string_view>
namespace dome {
struct HumanRole {
  std::string name, parent;
  bool required = false, needs_parent = false;
};
// VRM 1.0 semantic vocabulary; intermediate non-humanoid joints are allowed.
const std::vector<HumanRole> &human_roles();
struct HumanoidMapping {
  std::map<std::string, uint32_t> joints;
  void validate(const Skeleton &, bool require_complete = true) const;
  // Incomplete left/right pairs are an error; unassigned joints map to self.
  std::vector<uint32_t> mirror(const Skeleton &) const;
  nlohmann::json encode() const;
  static HumanoidMapping decode(const nlohmann::json &, const Skeleton &,
                                bool require_complete = true);
};
struct HumanLandmarks {
  std::map<std::string, Vec3> points;
  // Explicit model-space body frame. Default: +X character left, +Y up, +Z
  // front.
  Vec3 left{1, 0, 0}, up{0, 1, 0}, front{0, 0, 1};
  nlohmann::json encode() const;
  static HumanLandmarks decode(const nlohmann::json &);
};
struct HumanFitOptions {
  float maximum_surface_distance = .35f;
  float minimum_segment_length = .005f;
  float terminal_length = .08f;
  size_t maximum_work = 20'000'000;
};
struct HumanFit {
  Skeleton skeleton;
  HumanoidMapping mapping;
  std::vector<std::string> notes;
};
// Fits only explicitly marked humanoids; no silhouette/character recognition.
// Every required role must have an editable landmark near the supplied surface.
HumanFit fit_humanoid(const Mesh &, const HumanLandmarks &,
                      HumanFitOptions = {},
                      const std::function<bool()> &cancelled = {});
struct AvatarCompaction {
  Skeleton skeleton;
  SkinWeights weights;
  HumanoidMapping mapping;
  // UINT32_MAX denotes removed joints. Weighted/mapped joints and ancestors
  // stay.
  std::vector<uint32_t> old_to_new;
};
AvatarCompaction compact_avatar(const Skeleton &, const SkinWeights &,
                                const HumanoidMapping &);
struct VisemeBinding {
  std::string shape;
  float weight = 1;
};
struct AvatarExpressions {
  // VRM vowel semantics aa, ih, ou, ee, oh. Shape names remain stable
  // identities.
  std::map<std::string, std::vector<VisemeBinding>> visemes;
  void validate(const MorphSet &, bool require_all_vowels = false) const;
  void rename_shape(MorphSet &, std::string_view before, std::string after);
  nlohmann::json encode() const;
  static AvatarExpressions decode(const nlohmann::json &, const MorphSet &);
};
} // namespace dome
