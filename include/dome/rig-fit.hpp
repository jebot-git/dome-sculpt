// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
namespace dome {
// Landmarks and the anatomical body frame are in the immutable painting REST
// coordinate system. Tools present/adjust them through the full object matrix;
// rotations, shear and reflection never change the meaning of left/up/front.
struct RigFitOptions {
  HumanFitOptions fit;
  bool bind=true;
  BindOptions binding;
  // Distances above are WORLD metres, not object/group units.
  void validate() const;
};
struct RigFitResult {size_t bones=0;bool bound=false;std::vector<std::string> notes;};
// Explicit editable initial guides, not recognition or accepted fit. Existing
// rig/mapping identities seed the exact bone heads; an unrigged mesh gets only
// required-role bounding-box guides which the user must review and adjust.
HumanLandmarks rig_fit_guides(const SculptObject &);
// Detached atomic worker operation. Existing rig replacement is deliberately
// separate from new fitting: imported rest edits require original-node/bind
// transport, not replacement by a guessed humanoid hierarchy.
RigFitResult author_humanoid_fit(Scene &,uint64_t,const HumanLandmarks &,
    const RigFitOptions & = {},const std::function<bool()> &cancelled={});
} // namespace dome
