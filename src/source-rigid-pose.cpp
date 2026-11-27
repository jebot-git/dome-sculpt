// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-rigid-pose.hpp"
#include "import-skin.hpp"
#include <algorithm>
#include <map>
#include <ranges>
namespace dome::detail {
bool native_texture_set_accessory(const SculptObject &object) noexcept {
  return object.mesh_active && object.painted && object.texture_sets &&
      !object.appearance && !object.deformation && !object.rig_source;
}

std::shared_ptr<const SourceNodePlacement>
source_accessory_attachment(std::span<const SceneGroup> groups) {
  auto leaf = std::find_if(groups.rbegin(), groups.rend(),
      [](const auto &group) { return bool(group.source_placement); });
  if (leaf == groups.rend()) return {};
  const auto &placement = leaf->source_placement;
  if (!placement->model || !placement->model->asset ||
      placement->model->asset->avatar_version != ImportedAvatarVersion::Vrm1)
    return {};
  const auto root = std::ranges::find(groups, placement->source_instance, &SceneGroup::id);
  if (root == groups.end() || root >= leaf.base() - 1 || root->source_placement)
    return {};
  const auto &nodes = placement->model->asset->nodes;
  int parent = -1;
  for (auto group = root + 1; group != leaf.base(); ++group) {
    const auto &part = group->source_placement;
    if (!part || group->humanoid || part->source_instance != placement->source_instance ||
        part->model != placement->model || part->model.owner_before(placement->model) ||
        placement->model.owner_before(part->model) || part->source_node >= nodes.size() ||
        nodes[part->source_node].parent != parent)
      return {};
    parent = int(part->source_node);
  }
  return placement;
}

std::shared_ptr<const SourceNodePlacement>
source_rig_instance(const Scene &scene, const SculptObject &object) {
  if (!object.mesh_active || !object.painted || !object.appearance ||
      !object.appearance->asset || !object.group)
    return {};
  const auto &p = scene.group(object.group).source_placement;
  const auto &a = *object.appearance;
  if (!p || !p->model || p->model->asset != a.asset ||
      p->model->original_bytes != a.original_bytes ||
      a.source_node < 0 || p->source_node != uint32_t(a.source_node))
    return {};
  return p;
}

bool attach_source_accessory(Scene &scene, uint64_t accessory, uint64_t rig,
                             uint32_t joint, const std::function<bool()> &cancelled) {
  const auto stop = [&] {
    if (cancelled && cancelled()) throw std::runtime_error("Avatar accessory attachment cancelled");
  };
  stop();
  const auto &object = scene.object(accessory);
  const auto &source = scene.object(rig);
  const auto identity = source_rig_instance(scene, source);
  if (!native_texture_set_accessory(object) || !object.visible || object.locked)
    throw std::invalid_argument("Select an unlocked native texture-set accessory");
  if (!source.visible || !identity || !source.deformation || !source.deformation->skin || !source.deformation->skeleton ||
      identity->model->asset->avatar_version != ImportedAvatarVersion::Vrm1 ||
      joint >= source.appearance->source_joint_nodes.size())
    throw std::invalid_argument("Select an original joint of a visible VRM 1.0 avatar");
  const auto joint_node = source.appearance->source_joint_nodes[joint];
  const auto &nodes = identity->model->asset->nodes;
  if (joint_node >= nodes.size()) throw std::invalid_argument("Invalid original avatar joint");
  const auto source_path = scene.group_path(source.group);
  if (source_accessory_attachment(source_path) != identity)
    throw std::invalid_argument("Avatar source placement is outside its original hierarchy");

  std::vector<uint32_t> chain;
  for (int node = int(joint_node); node >= 0; node = nodes.at(size_t(node)).parent) {
    stop();
    if (chain.size() >= 128) throw std::length_error("Avatar bone placement exceeds native group depth");
    chain.push_back(uint32_t(node));
  }
  const auto before = scene.world_matrix(accessory);
  auto next = scene;
  auto parent = identity->source_instance;
  for (auto node : chain | std::views::reverse) {
    stop();
    uint64_t group_id = 0;
    for (const auto &group : next.groups()) {
      const auto &p = group.source_placement;
      if (group.parent != parent || !p || p->model != identity->model ||
          p->source_instance != identity->source_instance || p->source_node != node) continue;
      if (group_id) throw std::invalid_argument("Avatar bone has multiple native placement groups");
      group_id = group.id;
    }
    if (!group_id) {
      group_id = next.add_group(imported_bone_name(nodes[node], node), {}, parent);
      next.group(group_id).source_placement = std::make_shared<const SourceNodePlacement>(
          SourceNodePlacement{identity->model, node, identity->source_instance});
    }
    parent = group_id;
  }
  if (object.group == parent) return false;
  auto &attached = next.object(accessory);
  attached.group = parent;
  attached.affine_edit = std::make_shared<const NativeAffineEdit>(
      next.parent_matrix(accessory).inverse() * before * affine_similarity(attached.transform).inverse());
  next.validate(false);
  stop();
  scene = std::move(next);
  return true;
}

std::vector<SceneGroup> source_rigid_groups(
    const Scene &scene, const SculptObject &object,
    std::vector<SceneGroup> groups, const std::function<bool()> &cancelled) {
  if (object.deformation && object.deformation->skin)
    return groups;
  auto identity = source_rig_instance(scene, object);
  if (!identity && native_texture_set_accessory(object))
    identity = source_accessory_attachment(groups);
  if (!identity)
    return groups;
  auto stop = [&] {
    if (cancelled && cancelled())
      throw std::runtime_error("Source rigid pose evaluation cancelled");
  };
  const auto &asset = *identity->model->asset;
  std::map<uint32_t, std::optional<ImportedMatrix>> pose;
  for (const auto &g : groups)
    if (g.source_placement && g.source_placement->model == identity->model &&
        g.source_placement->source_instance == identity->source_instance)
      pose.emplace(g.source_placement->source_node, std::nullopt);
  for (const auto &candidate : scene.objects()) {
    stop();
    if (!candidate.deformation || !candidate.deformation->skin ||
        !candidate.deformation->skeleton)
      continue;
    auto other = source_rig_instance(scene, candidate);
    if (!other || other->model != identity->model ||
        other->source_instance != identity->source_instance)
      continue;
    const auto &d = *candidate.deformation;
    const auto &a = *candidate.appearance;
    const auto &s = *d.skeleton;
    if (a.source_joint_nodes.size() != s.bones.size() || !s.space_matrix ||
        *s.space_matrix != imported_inverse(asset.nodes.at(other->source_node).world))
      throw std::invalid_argument("Rigid source pose requires exact imported joint/bind-space authority");
    const auto local = s.local_matrices(d.pose);
    for (size_t bone = 0; bone < a.source_joint_nodes.size(); ++bone) {
      auto source = a.source_joint_nodes[bone];
      auto found = pose.find(source);
      if (found == pose.end())
        continue;
      const auto &rest = asset.nodes.at(source).local;
      if (!s.bones[bone].rest_matrix || *s.bones[bone].rest_matrix != rest)
        throw std::invalid_argument("Rigid source attachment needs explicit transport of edited joint rest axes");
      auto &value = found->second;
      if (value && *value != local[bone]) {
        if (*value == rest || local[bone] == rest)
          throw std::invalid_argument("Source rigid attachment has an unsynchronised authored pose; apply a linked source-rig pose before using it");
        throw std::invalid_argument("Source rigid attachment has conflicting authored poses across skin views");
      }
      value = local[bone];
    }
  }
  for (auto &g : groups) {
    stop();
    if (!g.source_placement || g.source_placement->model != identity->model ||
        g.source_placement->source_instance != identity->source_instance)
      continue;
    auto node = g.source_placement->source_node;
    const auto &value = pose.at(node);
    if (!value || *value == asset.nodes.at(node).local)
      continue;
    const auto native = affine_similarity(g.transform);
    const auto edit = g.affine_edit ? g.affine_edit->matrix() : AffineTransform{};
    // E*T*P = (E*T*P*A^-1*T^-1)*T*A. This immutable evaluated copy
    // leaves persisted original A and the native similarity editor unchanged.
    g.affine_edit = std::make_shared<const NativeAffineEdit>(
        edit * native * AffineTransform(*value) *
        AffineTransform(asset.nodes.at(node).local).inverse() * native.inverse());
  }
  return groups;
}
} // namespace dome::detail
