// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "humanoid-creator.hpp"
namespace dome {
// Session-only creator draft. Geometry is generated only by explicit Apply;
// persisted Scene groups keep the existing HumanoidControls schema unchanged.
struct HumanoidCreatorSettings {
  HumanoidControls controls{};
  std::string name="Humanoid";
  std::optional<HumanoidPreset> preset=HumanoidPreset::Neutral;
  void validate() const;
};
struct HumanoidCreatorView {
  HumanoidCreatorSettings settings;
  size_t parameter=0;
  uint64_t session=0,generation=0,revision=0,object=0,edit_authority=0;
  bool can_copy_selected=false;
  std::optional<HumanoidCreatorSettings> slider_before;
  std::vector<HumanoidCreatorSettings> undo,redo;
};
inline constexpr size_t humanoid_creator_parameter_count=27;
inline constexpr int humanoid_creator_page=33;
struct HumanoidCreatorParameter {
  const char *label;
  float minimum,maximum,step,value;
};
HumanoidCreatorParameter humanoid_creator_parameter(const HumanoidControls &,size_t);
void set_humanoid_creator_parameter(HumanoidControls &,size_t,float);
std::string humanoid_creator_parameter_text(const HumanoidCreatorView &);
// These functions operate on a detached draft and never mutate a Scene.
void update_humanoid_creator(HumanoidCreatorView &,HumanoidCreatorSettings);
bool humanoid_creator_history(HumanoidCreatorView &,bool redo);
} // namespace dome
