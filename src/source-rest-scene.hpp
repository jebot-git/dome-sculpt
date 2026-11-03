// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "source-rest-plan.hpp"
#include "dome/rig-edit.hpp"
namespace dome::detail {
bool author_source_rig_rest(Scene &,uint64_t,std::span<const RigRestJointEdit>,
 RigRestOptions,const std::function<bool()> &,
 std::span<const SourceRestParentEdit> source_parents={});
void restore_source_rig_rest(Scene &,uint64_t,RigRestOptions,
 const std::function<bool()> &);
void edit_source_rest_pose(Scene &,uint64_t,const RigEdit &,
 const std::function<bool()> &);
}
