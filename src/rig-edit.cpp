// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig-edit.hpp"
#include "dome/humanoid-creator.hpp"
#include <nlohmann/json.hpp>
namespace dome {
namespace {
void check_cancel(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled()) throw std::runtime_error("Rig edit cancelled");
}
const DeformationState &editable(const SculptObject &o) {
  if (!o.visible || o.locked || !o.mesh_active || !o.painted || !o.deformation)
    throw std::invalid_argument("Select a visible, unlocked rigged mesh");
  return *o.deformation;
}
Quat axis_rotation(Vec3 axis, float degrees) {
  float angle = degrees * .008726646259971648f;
  return {std::cos(angle), axis.x * std::sin(angle),
          axis.y * std::sin(angle), axis.z * std::sin(angle)};
}
uint64_t rig_group(const Scene &scene, const SculptObject &o) {
  auto path = scene.group_path(o.group);
  // A creator group is the shared rig boundary even with nested part groups.
  for (auto it = path.rbegin(); it != path.rend(); ++it)
    if (it->humanoid) return it->id;
  return o.group;
}
} // namespace
void edit_rig(Scene &scene, uint64_t object, const RigEdit &edit,
              const std::function<bool()> &cancelled) {
  check_cancel(cancelled);
  const auto &source = scene.object(object);
  const auto &old = editable(source);
  bool pose_edit = edit.joint.has_value() || edit.reset_pose;
  if (pose_edit && (!old.skeleton || !old.skin))
    throw std::invalid_argument("Pose editing requires a skeleton and skin");
  if (edit.joint) {
    const auto &j = *edit.joint;
    if (j.joint >= old.skeleton->bones.size() || !finite(j.degrees) ||
        !finite(j.translation) ||
        std::max({std::abs(j.degrees.x), std::abs(j.degrees.y), std::abs(j.degrees.z)}) > 180 ||
        std::max({std::abs(j.translation.x), std::abs(j.translation.y), std::abs(j.translation.z)}) > 10)
      throw std::invalid_argument("Invalid bounded local joint adjustment");
  }
  if (edit.morph && (!old.morphs || edit.morph->target >= old.morphs->targets.size() ||
                     !std::isfinite(edit.morph->weight) || std::abs(edit.morph->weight) > 1))
    throw std::invalid_argument("Invalid shapekey target or weight");
  auto group = rig_group(scene, source);
  auto signature = pose_edit ? old.skeleton->encode() : nlohmann::json{};
  auto pose = edit.reset_pose ? std::vector<JointTransform>{} : old.pose;
  if (edit.joint) {
    if (pose.empty())
      for (const auto &bone : old.skeleton->bones) pose.push_back(bone.rest);
    auto &joint = pose.at(edit.joint->joint);
    auto d = edit.joint->degrees;
    auto rotation = axis_rotation({0, 0, 1}, d.z) * axis_rotation({0, 1, 0}, d.y) *
                    axis_rotation({1, 0, 0}, d.x);
    joint.rotation = (joint.rotation * rotation).unit();
    joint.translation += edit.joint->translation;
  }
  std::vector<std::pair<uint64_t, std::shared_ptr<const DeformationState>>> changes;
  for (const auto &o : scene.objects()) {
    check_cancel(cancelled);
    bool selected = o.id == object;
    if (!selected) {
      if (!pose_edit || !edit.linked_pose || !group || rig_group(scene, o) != group ||
          !o.deformation || !o.deformation->skeleton || !o.deformation->skin ||
          o.deformation->skeleton->encode() != signature) continue;
      if (source.appearance && o.appearance &&
          source.appearance->source_joint_nodes != o.appearance->source_joint_nodes)
        throw std::invalid_argument("Grouped parts have different authored joint identities");
    }
    // Hidden parts still share the rig and must follow it; locked parts must
    // reject the whole linked operation rather than silently remaining behind.
    if (o.locked || !o.mesh_active || !o.painted)
      throw std::invalid_argument("A linked rig part is locked or has no rest mesh");
    auto next = std::make_shared<DeformationState>(*o.deformation);
    if (pose_edit) next->pose = pose;
    if (selected && edit.morph) {
      if (next->morph_weights.empty()) next->morph_weights.resize(next->morphs->targets.size());
      next->morph_weights.at(edit.morph->target) = edit.morph->weight;
    }
    next->validate(o.painted->mesh());
    changes.emplace_back(o.id, std::move(next));
  }
  check_cancel(cancelled);
  Scene result = scene;
  for (auto &[id, next] : changes) result.object(id).deformation = std::move(next);
  result.validate(false);
  check_cancel(cancelled);
  scene = std::move(result);
}
} // namespace dome
