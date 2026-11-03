// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rig-fit.hpp"
#include "rig-rest.hpp"
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
 // Existing-rig guide authority is a captured immutable deformation endpoint.
 // Editable heads remain REST/model local; length controls are WORLD metres.
 std::shared_ptr<const DeformationState> rest_source;
 std::vector<Vec3> bone_heads;
 std::vector<int> bone_parents;
 std::vector<float> bone_lengths;
 std::vector<uint64_t> linked_objects;
 bool linked_rest=true,restore_source=false;

};
inline constexpr int rig_fit_page=27;
inline constexpr size_t rig_fit_parameter_count=14;
size_t rig_fit_selection_count(const RigFitView &);
size_t rig_fit_options_count(const RigFitView &);
std::string rig_fit_selection_label(const RigFitView &);
std::optional<Vec3> rig_fit_selection_point(const RigFitView &);
void rig_fit_selection_point(RigFitView &,std::optional<Vec3>);
std::vector<RigRestJointEdit> rig_fit_rest_edits(const RigFitView &);
std::string rig_fit_parameter_text(const RigFitView &);
void adjust_rig_fit_parameter(RigFitView &,int);
std::string rig_fit_role_label(size_t);
std::string rig_fit_tool_label(const RigFitView *,int);
} // namespace dome
