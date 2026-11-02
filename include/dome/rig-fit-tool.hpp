// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rig-fit.hpp"
namespace dome {
// Session-only guide editing. Persistent accepted landmarks/rig live in the
// existing DeformationState; the source lease is owned privately by Document.
struct RigFitView {
 uint64_t generation=0,revision=0,object=0,edit_authority=0;
 HumanLandmarks landmarks;
 RigFitOptions options;
 AffineTransform world;
 size_t role=0,parameter=0;bool settings=false;
 float step=.01f; // World metres.
 bool placing=false,armed=false,pressed=false;
 uint32_t owner=0;
 std::optional<Vec3> before_placement;
};
inline constexpr int rig_fit_page=27;
inline constexpr size_t rig_fit_parameter_count=14;
std::string rig_fit_parameter_text(const RigFitView &);
void adjust_rig_fit_parameter(RigFitView &,int);
std::string rig_fit_role_label(size_t);
std::string rig_fit_tool_label(const RigFitView *,int);
} // namespace dome
