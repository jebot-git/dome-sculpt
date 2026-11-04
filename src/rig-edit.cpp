// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig-edit.hpp"
#include "dome/humanoid-creator.hpp"
#include <nlohmann/json.hpp>
#include "source-rigid-pose.hpp"
#include "source-rest-scene.hpp"
#include "native-rig-pose-scope.hpp"
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
  if(pose_edit&&source.rig_source&&detail::source_rest_identity(scene,source)){
    detail::edit_source_rest_pose(scene,object,edit,cancelled);return;
  }
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
  auto imported = detail::source_rig_instance(scene, source);
  detail::weight_surface::Budget native_work{0,500'000'000,cancelled};
  auto native_scope=pose_edit&&edit.linked_pose&&!imported
      ?detail::native_rig_pose_scope(scene,source,native_work)
      :std::optional<detail::NativeRigPoseScope>{};
  if(native_scope){
    // Forecast every real mutable recipient before the first deformation copy.
    detail::geometry_weight_authoring::Bytes bytes{65536,256u*1024u*1024u};
    bytes.add(scene.objects().size(),sizeof(SculptObject));
    bytes.add(scene.groups().size(),sizeof(SceneGroup));
    for(const auto&o:scene.objects()){
      native_work.step(o.name.size()+o.material.name.size()+1);
      bytes.text(o.name);bytes.text(o.material.name);
    }
    for(const auto&g:scene.groups()){
      native_work.step(g.name.size()+1);bytes.text(g.name);
    }
    for(auto id:native_scope->objects){const auto&o=scene.object(id);
      if(!o.deformation||!o.deformation->skeleton||!o.deformation->skin)continue;
      if(o.locked||!o.mesh_active||!o.painted)throw std::invalid_argument("A linked native source part is locked or has no rest mesh");
      detail::require_native_pose_rig(scene,source,o,edit.reset_pose,native_work);
      detail::geometry_weight_authoring::deformation_copy_bytes(*o.deformation,bytes,native_work);
      native_work.step_items(o.painted->mesh().positions.size()+o.painted->mesh().indices.size(),16);
    }
  }
  auto signature = pose_edit && !imported && !native_scope ? old.skeleton->encode() : nlohmann::json{};
  auto pose = edit.reset_pose ? std::vector<JointTransform>{} : old.pose;
  if (edit.joint) {
    if (pose.empty())
      for (const auto &bone : old.skeleton->bones) pose.push_back(bone.rest);
    auto &joint = pose.at(edit.joint->joint);
    auto d = edit.joint->degrees;
    auto rotation = axis_rotation({0, 0, 1}, d.z) * axis_rotation({0, 1, 0}, d.y) *
                    axis_rotation({1, 0, 0}, d.x);
    joint = adjust_joint_pose(std::move(joint),rotation,edit.joint->translation);
  }
  std::vector<std::pair<uint64_t, std::shared_ptr<const DeformationState>>> changes;
  for (const auto &o : scene.objects()) {
    check_cancel(cancelled);
    bool selected = o.id == object;
    if (!selected) {
      if (!pose_edit || !edit.linked_pose) continue;
      if (imported) {
        auto other = detail::source_rig_instance(scene, o);
        if (!other || other->model != imported->model ||
            other->source_instance != imported->source_instance) continue;
        if (!o.deformation || !o.deformation->skeleton || !o.deformation->skin) {
          // A locked rigid descendant is protected even though it has no skin.
          if (o.locked && edit.joint) {
            auto joint = source.appearance->source_joint_nodes.at(edit.joint->joint);
            for (const auto &g : scene.group_path(o.group))
              if (g.source_placement && g.source_placement->model == imported->model &&
                  g.source_placement->source_node == joint)
                throw std::invalid_argument("A rigid source-rig descendant is locked");
          }
          continue;
        }
      } else if(native_scope) {
        if(!native_scope->contains(o.id)||!o.deformation||!o.deformation->skeleton||!o.deformation->skin)continue;
      } else {
        if (!group || rig_group(scene, o) != group || !o.deformation ||
            !o.deformation->skeleton || !o.deformation->skin ||
            o.deformation->skeleton->encode() != signature) continue;
        if (source.appearance && o.appearance &&
            source.appearance->source_joint_nodes != o.appearance->source_joint_nodes)
          throw std::invalid_argument("Grouped parts have different authored joint identities");
      }
    }
    // Hidden parts still share the rig and must follow it; locked parts must
    // reject the whole linked operation rather than silently remaining behind.
    if (o.locked || !o.mesh_active || !o.painted)
      throw std::invalid_argument("A linked rig part is locked or has no rest mesh");
    auto next = std::make_shared<DeformationState>(*o.deformation);
    if (pose_edit) {
      if (imported && !selected) {
        const auto &from = source.appearance->source_joint_nodes;
        const auto &to = o.appearance->source_joint_nodes;
        if (to.size() != next->skeleton->bones.size())
          throw std::invalid_argument("Linked source-rig joint identity table is stale");
        const auto before_local = old.skeleton->local_matrices(old.pose);
        const auto other_local = next->skeleton->local_matrices(next->pose);
        auto remapped = next->pose;
        if (remapped.empty())
          for (const auto &bone : next->skeleton->bones) remapped.push_back(bone.rest);
        for (size_t slot = 0; slot < to.size(); ++slot) {
          auto at = std::find(from.begin(), from.end(), to[slot]);
          if (at == from.end())
            throw std::invalid_argument("Linked source-rig lacks a joint identity");
          auto original = size_t(at-from.begin());
          if (before_local.at(original) != other_local.at(slot)) {
            const auto &baseline = imported->model->asset->nodes.at(to[slot]).local;
            if (before_local.at(original) == baseline || other_local.at(slot) == baseline)
              throw std::invalid_argument("Linked source rig has an unsynchronised authored pose; resolve the source views explicitly");
            throw std::invalid_argument("Linked source rig has conflicting authored poses across skin views");
          }
          const auto &before = old.skeleton->bones.at(original);
          const auto &after = next->skeleton->bones.at(slot);
          auto reference=[](const JointTransform &t) {
            return std::array<float,10>{t.translation.x,t.translation.y,t.translation.z,
              t.rotation.w,t.rotation.x,t.rotation.y,t.rotation.z,
              t.scale.x,t.scale.y,t.scale.z};
          };
          if (before.rest_matrix != after.rest_matrix || !before.rest_matrix ||
              reference(before.rest) != reference(after.rest))
            throw std::invalid_argument("Linked source-rig rest axes differ");
          remapped[slot] = pose.empty() ? after.rest : pose.at(original);
        }
        next->pose = edit.reset_pose ? std::vector<JointTransform>{} : std::move(remapped);
      } else if(native_scope&&!selected) {
        // Preserve every untouched local pose value/representation. Only the
        // requested joint is shared; reset is an explicit whole-pose action.
        if(edit.reset_pose)next->pose.clear();
        if(edit.joint){if(next->pose.empty())for(const auto&bone:next->skeleton->bones)next->pose.push_back(bone.rest);next->pose.at(edit.joint->joint)=pose.at(edit.joint->joint);}
      } else next->pose = pose;
    }
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
  if (pose_edit && edit.linked_pose && imported) {
    for (const auto &o : scene.objects()) {
      check_cancel(cancelled);
      if (o.deformation && o.deformation->skin) continue;
      auto other = detail::source_rig_instance(scene, o);
      if (!other || other->model != imported->model ||
          other->source_instance != imported->source_instance) continue;
      auto frame = [&](const Scene &state) {
        AffineTransform value;
        for (const auto &g : detail::source_rigid_groups(
                 state,state.object(o.id),state.group_path(o.group),cancelled))
          value = value * group_local_matrix(g);
        return value;
      };
      const auto before = frame(scene), after = frame(result);
      if (before.matrix() == after.matrix()) continue;
      if (o.locked)
        throw std::invalid_argument("A rigid source-rig descendant is locked");
      const auto world = after * before.inverse() * scene.world_matrix(o.id);
      auto &next = result.object(o.id);
      next.affine_edit = std::make_shared<const NativeAffineEdit>(
          result.parent_matrix(o.id).inverse() * world *
          affine_similarity(next.transform).inverse());
    }
  }
  result.validate(false);
  check_cancel(cancelled);
  scene = std::move(result);
}
} // namespace dome
