// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
namespace dome {
struct RigRestJointEdit {
 uint32_t joint=0;
 std::optional<int> parent; // -1 is an explicit root.
 std::optional<Vec3> world_position;
 std::optional<Quat> world_rotation_delta; // About this joint's head.
 std::optional<float> world_length;
};
struct RigRestOptions {
 bool linked=true;
 float maximum_world_error=.00002f;
 // An explicit native selection, captured with the Scene transaction lease.
 // Empty means only authenticated shared source/first-rig owners may link.
 std::vector<uint64_t> linked_objects;

 void validate() const;
};
struct RigRestTransport {
 Skeleton skeleton;
 std::vector<JointTransform> pose;
 bool changed=false;
};
// Full affine native object-to-world metric. Original REST geometry is never
// transformed. New binds preserve the original REST skin product, and exact
// local pose transport preserves the current skin product. No TRS guess/reset.
RigRestTransport transport_rig_rest(const Skeleton &,std::span<const JointTransform>,
 const AffineTransform &world,std::span<const RigRestJointEdit>,RigRestOptions = {},
 const std::function<bool()> &cancelled = {});
// Atomic scene-level source/group adapter. Every affected part is validated
// before publication; exact original data is retained in first rig_source.
std::vector<uint64_t> rig_rest_linked_objects(const Scene &,uint64_t);
bool author_rig_rest(Scene &,uint64_t,std::span<const RigRestJointEdit>,
 RigRestOptions = {},const std::function<bool()> &cancelled = {});
void restore_rig_source(Scene &,uint64_t,RigRestOptions,const std::function<bool()> &cancelled = {});
void restore_rig_source(Scene &,uint64_t,bool linked=true,const std::function<bool()> &cancelled = {});
} // namespace dome
