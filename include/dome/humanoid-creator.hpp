// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "deformation.hpp"
namespace dome {
struct HumanoidControls {
  float height = 1.7f, anime = .65f;
  // Signed proportions are bounded to [-1,1]; neutral means the authored base.
  float build = 0, shoulder_width = 0, waist_width = 0;
  float leg_length = 0, arm_length = 0, head_width = 0, head_height = 0;
  float eye_size = .65f, eye_spacing = 0, nose_size = -.3f;
  float mouth_width = -.15f, jaw_width = -.2f;
  float hair_length = .3f, hair_volume = .4f, fringe = .35f;
  std::array<float, 4> skin_color{.78f, .56f, .43f, 1};
  std::array<float, 4> iris_color{.12f, .32f, .65f, 1};
  std::array<float, 4> hair_color{.09f, .035f, .025f, 1};
  void validate() const;
  nlohmann::json encode() const;
  static HumanoidControls decode(const nlohmann::json &);
};
struct HumanoidPart {
  std::string name;
  Mesh mesh;
  DeformationState deformation;
  std::array<float, 4> color{1, 1, 1, 1};
  bool toon = true;
};
struct CreatedHumanoid {
  HumanoidControls controls;
  // Body, sclera, iris, pupil and hair have stable independent UV/topology.
  // All share the same metric rest skeleton and semantic joint indices.
  std::vector<HumanoidPart> parts;
  std::vector<std::string> notes;
  void validate() const;
};
// Native generator using embedded, explicitly CC0 MakeHuman geometry/targets/
// rigging data. Additional eyes and groom geometry are original implementation.
CreatedHumanoid create_humanoid(const HumanoidControls & = {},
                                const std::function<bool()> &cancelled = {});
} // namespace dome
