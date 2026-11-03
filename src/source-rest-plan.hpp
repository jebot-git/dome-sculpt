// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "source-rest-graph.hpp"
namespace dome::detail {
// Existing original node IDs, authenticated against actual captured skin views.
struct SourceRestParentEdit {uint32_t node=0;int parent=-1;};
struct SourceRestSkinPlan {
 uint64_t object=0;
 Skeleton skeleton;
 std::vector<JointTransform> pose;
 std::vector<uint32_t> source_nodes;
};
struct SourceRestPlan {
 std::vector<int> parents;
 std::vector<RigMatrix> rest_local,rest_world,pose_local;
 std::vector<SourceRestSkinPlan> skins;
 bool changed=false;
};
// Original-node identities, canonical local REST/pose tables and each skin's
// own binds are kept separate. An unproved pose consensus is never replaced by
// selecting the first skin. This first actual route requires a shared pose.
SourceRestPlan plan_source_rest(const SourceRestGraph &,uint64_t,
 std::span<const RigRestJointEdit>,RigRestOptions={},
 const std::function<bool()> &cancelled={},
 std::span<const SourceRestParentEdit> source_parents={});
} // namespace dome::detail
