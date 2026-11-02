// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-rigid-pose.hpp"
#include "import-skin.hpp"
#include <map>
namespace dome::detail {
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
std::vector<SceneGroup> source_rigid_groups(
    const Scene &scene, const SculptObject &object,
    std::vector<SceneGroup> groups, const std::function<bool()> &cancelled) {
  if (object.deformation && object.deformation->skin)
    return groups;
  auto identity = source_rig_instance(scene, object);
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
