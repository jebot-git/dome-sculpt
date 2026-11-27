// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-skin-link.hpp"
#include "source-rigid-pose.hpp"
#include "skin-affine.hpp"
#include "texture-set-surface.hpp"
#include "import-skin.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>

namespace dome::detail {
namespace {
void stop(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled()) throw std::runtime_error("Linked source skin operation cancelled");
}

bool same_node(const SourceNodePlacement &a, const SourceNodePlacement &b) {
  return same_source_skin_instance(a, b) && a.source_node == b.source_node;
}

bool close_matrix(const RigMatrix &a, const RigMatrix &b) {
  for (size_t i = 0; i < a.size(); ++i)
    if (std::abs(double(a[i]) - b[i]) > 2e-6 * std::max({1., double(std::abs(a[i])), double(std::abs(b[i]))}))
      return false;
  return true;
}

Skeleton accessory_skeleton(const Skeleton &source, const AffineTransform &owner,
                             const AffineTransform &reference, const std::function<bool()> &cancelled) {
  if (owner.matrix() == reference.matrix()) return source;
  SkinGeometryWork work(256, cancelled);
  return skin_reframe(source, owner.inverse() * reference, work);
}

void authenticate(const DeformationState &target, const AffineTransform &target_world,
                  const DeformationState &reference, const AffineTransform &reference_world,
                  const CreatorAppearance &appearance, const std::function<bool()> &cancelled) {
  stop(cancelled);
  if (!target.skeleton || !target.skin || !target.skeleton->space_matrix ||
      !target.skeleton->inverse_bind_override || !reference.skeleton || !reference.skin ||
      !reference.skeleton->space_matrix || !reference.skeleton->inverse_bind_override ||
      target.source_skin_joints != appearance.source_joint_nodes ||
      target.source_skin_joints.size() != target.skeleton->bones.size())
    throw std::invalid_argument("The linked native skin has no matching original joint table");
  const auto &s = *target.skeleton;
  const auto expected = accessory_skeleton(*reference.skeleton, target_world, reference_world, cancelled);
  if (!close_matrix(*s.space_matrix, *expected.space_matrix) ||
      s.local_matrices() != expected.local_matrices() ||
      s.local_matrices(target.pose) != expected.local_matrices(reference.pose))
    throw std::invalid_argument("The native attachment's placement or pose differs from its linked original skin");
  for (size_t i = 0; i < s.bones.size(); ++i) {
    stop(cancelled);
    if (s.bones[i].parent != expected.bones.at(i).parent ||
        !close_matrix(s.inverse_bind_override->at(i), expected.inverse_bind_override->at(i)))
      throw std::invalid_argument("The native attachment's hierarchy or inverse bind differs from the original skin");
  }
}
} // namespace

bool same_source_skin_instance(const SourceNodePlacement &a, const SourceNodePlacement &b) noexcept {
  return a.source_instance == b.source_instance && a.model == b.model &&
         !a.model.owner_before(b.model) && !b.model.owner_before(a.model);
}

std::shared_ptr<const SourceNodePlacement> source_skin_link(
    std::span<const SceneGroup> path, const DeformationState &deformation) {
  if (deformation.source_skin_joints.empty()) return {};
  const auto link = source_accessory_attachment(path);
  if (!link || link->source_instance != deformation.source_skin_instance ||
      link->source_node != deformation.source_skin_node ||
      link->model->asset->nodes.at(link->source_node).skin < 0 ||
      std::ranges::any_of(deformation.source_skin_joints, [&](auto node) {
        return node >= link->model->asset->nodes.size();
      }))
    throw std::invalid_argument("The linked native skin lost its original avatar placement");
  return link;
}

const SculptObject &source_skin_reference(const Scene &scene, const SourceNodePlacement &link) {
  const SculptObject *reference = nullptr;
  for (const auto &candidate : scene.objects()) {
    if (!candidate.deformation || !candidate.deformation->skeleton || !candidate.deformation->skin) continue;
    const auto placement = source_rig_instance(scene, candidate);
    if (!placement || !same_node(*placement, link)) continue;
    if (candidate.visible) return candidate;
    if (!reference) reference = &candidate;
  }
  if (reference) return *reference;
  throw std::invalid_argument("The linked native skin has no original reference part in this avatar instance");
}

bool link_native_source_skin(Scene &scene, uint64_t target_id, uint64_t reference_id,
                             const std::function<bool()> &cancelled) {
  const auto &target = scene.object(target_id);
  if (target.appearance || !target.texture_sets || !target.deformation || !target.deformation->skin)
    return false;
  stop(cancelled);
  const auto existing = source_skin_link(scene.group_path(target.group), *target.deformation);
  const auto &reference = existing && target_id == reference_id ? source_skin_reference(scene, *existing) :
                                                                  scene.object(reference_id);
  const auto identity = source_rig_instance(scene, reference);
  if (existing && (!identity || !same_node(*identity, *existing)))
    throw std::invalid_argument("Restore the original accessory source before binding it to another skin");
  if (!identity || !reference.visible || identity->model->asset->avatar_version != ImportedAvatarVersion::Vrm1 ||
      !reference.deformation || !reference.deformation->skin || !reference.deformation->skeleton ||
      !texture_set_shape_editable(target)) return false;
  if (source_accessory_attachment(scene.group_path(reference.group)) != identity ||
      reference.appearance->source_joint_nodes.size() != reference.deformation->skeleton->bones.size() ||
      !reference.deformation->skeleton->inverse_bind_override ||
      reference.deformation->skeleton->space_matrix != imported_inverse(identity->model->asset->nodes.at(identity->source_node).world))
    throw std::invalid_argument("The reference does not have its original imported skin space");
  if (existing) {
    authenticate(*target.deformation, scene.world_matrix(target_id), *reference.deformation,
                 scene.world_matrix(reference.id), *reference.appearance, cancelled);
    return false;
  }
  const auto world = scene.world_matrix(target_id);
  const auto reference_world = scene.world_matrix(reference.id);
  const auto parent = scene.parent_matrix(reference.id);
  const auto similarity = affine_similarity(target.transform);
  auto edit = std::make_shared<const NativeAffineEdit>(parent.inverse() * world * similarity.inverse());
  const auto attached_world = parent * (edit->matrix() * similarity);
  auto deformation = std::make_shared<DeformationState>(*target.deformation);
  deformation->source_skin_instance = identity->source_instance;
  deformation->source_skin_node = identity->source_node;
  deformation->source_skin_joints = reference.appearance->source_joint_nodes;
  deformation->skeleton = accessory_skeleton(*reference.deformation->skeleton, attached_world, reference_world, cancelled);
  deformation->pose = reference.deformation->pose;
  deformation->humanoid = reference.deformation->humanoid;
  deformation->validate(target.painted->mesh());
  stop(cancelled);
  auto &attached = scene.object(target_id);
  attached.group = reference.group;
  attached.affine_edit = std::move(edit);
  attached.deformation = std::move(deformation);
  return true;
}

void update_native_source_skins(const Scene &before, Scene &after, const SourceNodePlacement &instance,
                                const std::function<bool()> &cancelled) {
  for (const auto &target : before.objects()) {
    stop(cancelled);
    if (!target.deformation || target.deformation->source_skin_joints.empty()) continue;
    const auto link = source_skin_link(before.group_path(target.group), *target.deformation);
    if (!same_source_skin_instance(*link, instance)) continue;
    if (target.locked || !target.mesh_active || !target.painted)
      throw std::invalid_argument("A linked native skin is locked or has no rest mesh");
    const auto &old = source_skin_reference(before, *link);
    const auto &now = after.object(old.id);
    if (old.deformation == now.deformation && old.appearance == now.appearance) continue;
    const auto world = after.world_matrix(target.id);
    authenticate(*target.deformation, before.world_matrix(target.id), *old.deformation,
                 before.world_matrix(old.id), *old.appearance, cancelled);
    auto deformation = std::make_shared<DeformationState>(*target.deformation);
    auto &nodes = deformation->source_skin_joints;
    const auto &next_nodes = now.appearance->source_joint_nodes;
    if (nodes != next_nodes) {
      std::vector<uint32_t> remap;
      remap.reserve(nodes.size());
      for (const auto node : nodes) {
        const auto at = std::ranges::find(next_nodes, node);
        remap.push_back(at == next_nodes.end() ? UINT32_MAX : uint32_t(at - next_nodes.begin()));
      }
      for (auto &row : deformation->skin->vertices) {
        stop(cancelled);
        for (auto &entry : row) {
          entry.joint = remap.at(entry.joint);
          if (entry.joint == UINT32_MAX)
            throw std::invalid_argument("The edited source rig removes a joint used by the native skin");
        }
      }
      nodes = next_nodes;
    }
    if (old.deformation->skeleton->encode() != now.deformation->skeleton->encode() || old.appearance != now.appearance) {
      deformation->skeleton = accessory_skeleton(*now.deformation->skeleton, world, after.world_matrix(now.id), cancelled);
      deformation->humanoid = now.deformation->humanoid;
      if (now.deformation->landmarks) {
        SkinGeometryWork work(256, cancelled);
        deformation->landmarks = skin_reframe_landmarks(*now.deformation->landmarks,
            world.inverse() * after.world_matrix(now.id), work);
      }
    }
    deformation->pose = now.deformation->pose;
    deformation->validate(target.painted->mesh());
    after.object(target.id).deformation = std::move(deformation);
  }
}

void authenticate_native_source_skin(const ObjectSurface &target, const ObjectSurface &reference,
                                     const std::function<bool()> &cancelled) {
  authenticate(*target.deformation, target.world_matrix(), *reference.deformation,
               reference.world_matrix(), *reference.appearance, cancelled);
}
} // namespace dome::detail
