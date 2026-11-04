// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/shapekey-authoring.hpp"
#include "dome/vr-controls.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace dome::detail {
// Normalized sliders retain useful precision near zero while covering the same
// explicit object-local range as the desktop dialog. No object bounds are
// scanned on the input thread and the plane is never inferred from the mesh.
inline double mirror_plane_metres(float slider) {
 const double t=2*double(slider)-1;
 return std::copysign(std::min(1e6,.001*std::expm1(std::abs(t)*std::log1p(1e9))),t);
}
inline float mirror_plane_slider(double metres) {
 if(!std::isfinite(metres)||std::abs(metres)>1e6)throw std::invalid_argument("Mirror plane is outside the supported local range");
 return float(.5+.5*std::copysign(std::log1p(std::abs(metres)/.001)/std::log1p(1e9),metres));
}
inline float mirror_tolerance_metres(float slider) {
 return slider==1.f/3.f?.0001f:float(std::pow(10.,-6.+6.*double(slider)));
}
inline ShapeKeyMirrorFrames mirror_control_method(float slider) {
 const ShapeKeyMirrorFrames modes[]={ShapeKeyMirrorFrames::VertexPositions,ShapeKeyMirrorFrames::ExactAuthored,ShapeKeyMirrorFrames::BaseRelative};
 return modes[std::clamp(int(std::lround(slider*2)),0,2)];
}
inline ShapeKeyEdit mirror_control_request(const VrControlState &c) {
 if(c.parameter!=VrParameter::ShapeMirror||c.components!=3)throw std::invalid_argument("Mirror controls are unavailable");
 for(int i=0;i<3;++i)if(!std::isfinite(c.value[i])||c.value[i]<0||c.value[i]>1)throw std::invalid_argument("Invalid Mirror control value");
 // DeformationState validates at most128 targets. This hidden, immutable fourth
 // value captures the target exactly; pointer events can change only rows0–2.
 if(!std::isfinite(c.value[3])||c.value[3]<0||c.value[3]>=128||std::floor(c.value[3])!=c.value[3])throw std::invalid_argument("Invalid captured Mirror target");
 ShapeKeyEdit result;result.action=ShapeKeyAction::Mirror;result.target=size_t(c.value[3]);result.mirror_frames=mirror_control_method(c.value[0]);result.mirror_plane=mirror_plane_metres(c.value[1]);result.mirror_tolerance=mirror_tolerance_metres(c.value[2]);return result;
}
}
