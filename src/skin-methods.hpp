// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/skin-authoring.hpp"
#include <algorithm>
#include <cmath>
namespace dome::detail {
inline constexpr int last_skin_method = 4;
inline bool skin_binds_selection(SkinAuthoringMode mode){
 return mode==SkinAuthoringMode::SurfaceHeat||mode==SkinAuthoringMode::VolumeGeodesic;
}
inline float volume_skin_cell_slider(float size){
 return std::clamp(float(std::log(double(size)/minimum_skin_cell_size)/std::log(double(maximum_skin_cell_size)/minimum_skin_cell_size)),0.f,1.f);
}
inline float volume_skin_cell_metres(float value){
 if(value<=0)return minimum_skin_cell_size;
 if(value>=1)return maximum_skin_cell_size;
 return float(double(minimum_skin_cell_size)*std::pow(double(maximum_skin_cell_size)/minimum_skin_cell_size,value));
}
// Existing method indices remain stable.
inline SkinAuthoringMode skin_method_mode(int choice){
 switch(choice){case 0:return SkinAuthoringMode::Automatic;case 1:return SkinAuthoringMode::Transfer;case 2:return SkinAuthoringMode::Anatomical;case 3:return SkinAuthoringMode::SurfaceHeat;case 4:return SkinAuthoringMode::VolumeGeodesic;default:throw std::invalid_argument("Unknown skin method choice");}
}
inline int skin_method_choice(SkinAuthoringMode mode){
 switch(mode){case SkinAuthoringMode::Automatic:return 0;case SkinAuthoringMode::Transfer:return 1;case SkinAuthoringMode::Anatomical:return 2;case SkinAuthoringMode::SurfaceHeat:return 3;case SkinAuthoringMode::VolumeGeodesic:return 4;}throw std::invalid_argument("Unknown skin method");
}
inline const char*skin_method_label(SkinAuthoringMode mode){
 switch(mode){case SkinAuthoringMode::Automatic:return "Legacy automatic";case SkinAuthoringMode::Transfer:return "Weight transfer";case SkinAuthoringMode::Anatomical:return "Anatomical source";case SkinAuthoringMode::SurfaceHeat:return "Automatic (surface heat)";case SkinAuthoringMode::VolumeGeodesic:return "Volume geodesic";}throw std::invalid_argument("Unknown skin method");
}
}
