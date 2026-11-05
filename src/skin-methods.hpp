// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/skin-authoring.hpp"
namespace dome::detail {
// Preserve all previous controller/desktop choice indices;5 is additive.
inline SkinAuthoringMode skin_method_mode(int choice){
 switch(choice){case 0:return SkinAuthoringMode::Automatic;case 1:return SkinAuthoringMode::Transfer;case 2:return SkinAuthoringMode::Anatomical;case 3:return SkinAuthoringMode::SurfaceHeat;default:throw std::invalid_argument("Unknown skin method choice");}
}
inline int skin_method_choice(SkinAuthoringMode mode){
 switch(mode){case SkinAuthoringMode::Automatic:return 0;case SkinAuthoringMode::Transfer:return 1;case SkinAuthoringMode::Anatomical:return 2;case SkinAuthoringMode::SurfaceHeat:return 3;}throw std::invalid_argument("Unknown skin method");
}
inline const char*skin_method_label(SkinAuthoringMode mode){
 switch(mode){case SkinAuthoringMode::Automatic:return "Legacy automatic";case SkinAuthoringMode::Transfer:return "Weight transfer";case SkinAuthoringMode::Anatomical:return "Anatomical source";case SkinAuthoringMode::SurfaceHeat:return "Automatic (surface heat)";}throw std::invalid_argument("Unknown skin method");
}
}
