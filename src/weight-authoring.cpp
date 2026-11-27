// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/weight-authoring.hpp"
#include "dome/humanoid-creator.hpp"
#include "affine-surface.hpp"
#include "source-rigid-pose.hpp"
#include "source-skin-link.hpp"
#include "source-skin-world.hpp"
#include <nlohmann/json.hpp>
namespace dome {
namespace {
std::span<const uint32_t> joint_nodes(const SculptObject &object) {
  if (!object.deformation->source_skin_joints.empty()) return object.deformation->source_skin_joints;
  if (object.appearance) return object.appearance->source_joint_nodes;
  return {};
}
auto source_placement(const Scene &scene, const SculptObject &object) {
  if (!object.deformation->source_skin_joints.empty())
    return detail::source_skin_link(scene.group_path(object.group), *object.deformation);
  return detail::source_rig_instance(scene, object);
}
}
std::optional<uint32_t> weight_joint_slot(const SculptObject &source, const SculptObject &target, uint32_t slot) {
  const auto from = joint_nodes(source), to = joint_nodes(target);
  if (&source == &target || from.empty() || to.empty()) return slot;
  if (slot >= from.size()) throw std::invalid_argument("Weight source joint table is incomplete");
  const auto found = std::ranges::find(to, from[slot]);
  if (found == to.end()) return {};
  return uint32_t(found - to.begin());
}
void WeightToolSettings::validate(size_t bones) const {
  if (!bones || bones > 256 || mode < WeightBrushMode::Add || mode > WeightBrushMode::Smooth ||
      !std::isfinite(strength) || strength < 0 || strength > 1 ||
      !std::isfinite(value) || value < 0 || value > 1 ||
      !std::isfinite(symmetry_plane) || std::abs(symmetry_plane) > 10000 ||
      (!locks.empty() && locks.size() != bones) ||
      std::any_of(locks.begin(), locks.end(), [](auto v) { return v > 1; }) ||
      !limits.max_influences || limits.max_influences > 32 ||
      !std::isfinite(limits.prune) || limits.prune < 0 || limits.prune >= 1)
    throw std::invalid_argument("Invalid skin weight tool settings or locks");
}
std::vector<uint64_t> weight_objects(const Scene &scene, uint64_t object, bool linked) {
  const auto &source = scene.object(object);
  if (!source.visible || source.locked || !source.mesh_active || !source.painted ||
      !source.deformation || !source.deformation->skeleton || !source.deformation->skin)
    throw std::invalid_argument("Select a visible, unlocked skinned mesh");
  if (!linked) return {object};
  const auto owner = source_placement(scene, source);
  auto group = source.group;
  auto path = scene.group_path(group);
  for (auto it = path.rbegin(); it != path.rend(); ++it)
    if (it->humanoid) { group = it->id; break; }
  const auto rig = owner ? nlohmann::json{} : source.deformation->skeleton->encode();
  std::vector<uint64_t> result;
  for (const auto &part : scene.objects()) {
    if (part.id != object) {
      if (!part.visible || part.locked || !part.mesh_active || !part.painted ||
          !part.deformation || !part.deformation->skeleton || !part.deformation->skin) continue;
      const auto other = source_placement(scene, part);
      if (owner) {
        if (!other || !detail::same_source_skin_instance(*owner, *other) ||
            joint_nodes(part).empty()) continue;
        result.push_back(part.id);
        continue;
      }
      if (other || !group) continue;
      auto groups = scene.group_path(part.group);
      if (std::none_of(groups.begin(), groups.end(), [group](const auto &g) { return g.id == group; }) ||
          part.deformation->skeleton->encode() != rig) continue;
      if (source.appearance && part.appearance &&
          source.appearance->source_joint_nodes != part.appearance->source_joint_nodes)
        throw std::invalid_argument("Linked skin parts have different authored joint identities");
    }
    result.push_back(part.id);
  }
  return result;
}
WeightEditResult paint_scene_weights(Scene &scene, uint64_t object,
                                    std::span<const WeightBrush> brushes,
                                    const WeightToolSettings &settings,
                                    const std::function<bool()> &cancelled) {
  auto ids = weight_objects(scene, object, settings.linked);
  const auto &active = scene.object(object);
  const auto &source = *active.deformation;
  settings.validate(source.skeleton->bones.size());
  if (brushes.empty() || brushes.front().joint >= source.skeleton->bones.size())
    throw std::invalid_argument("Weight gesture needs an eligible source joint and a sample");
  const bool affine = scene.has_affine_placement(object);
  const auto inverse = affine ? CanvasTransform{} : ob::inverse(scene.world_transform(object));
  const auto brush_from_world = affine_similarity(inverse);
  WeightEditOptions options; options.locks = settings.locks; options.limits = settings.limits;
  if (settings.symmetry) {
    if (!source.humanoid) throw std::invalid_argument("Weight symmetry requires explicit humanoid joint mapping");
    options.symmetry = WeightBrushSymmetry{settings.symmetry_plane, source.humanoid->mirror(*source.skeleton),
                                          affine ? std::optional{scene.world_matrix(object)} : std::nullopt};
  }
  const auto joint = brushes.front().joint;
  const auto mirror = options.symmetry ? options.symmetry->joints[joint] : joint;
  std::vector<std::vector<Vec3>> positions;
  positions.reserve(ids.size());
  std::vector<std::vector<uint8_t>> locks;
  locks.reserve(ids.size());
  std::vector<WeightPaintSurface> surfaces;
  size_t vertices = 0;
  for (auto id : ids) {
    if (cancelled && cancelled()) throw std::runtime_error("Weight gesture cancelled");
    const auto &part = scene.object(id);
    const auto direct = weight_joint_slot(active, part, joint);
    const auto reflected = weight_joint_slot(active, part, mirror);
    if (!direct && !reflected) continue;
    const auto &rest = part.painted->mesh();
    if (rest.positions.size() > options.maximum_vertices - vertices)
      throw std::length_error("Weight brush vertex budget exceeded");
    vertices += rest.positions.size();
    const bool exact = affine || scene.has_affine_placement(id);
    std::optional<std::vector<Vec3>> posed;
    if (exact && part.appearance) {
      ObjectSurface surface;
      surface.id = id; surface.mesh = rest; surface.transform = part.transform;
      surface.groups = scene.group_path(part.group); surface.affine_edit = part.affine_edit;
      surface.appearance = part.appearance; surface.deformation = part.deformation;
      surface.rig_source = part.rig_source;
      posed = detail::source_skin_world_positions(surface, cancelled, 256u*1024u*1024u, &brush_from_world);
    }
    if (!posed) {
      posed = part.deformation->preview(rest).positions;
      const auto transform = exact ? CanvasTransform{} : ob::compose(inverse, scene.world_transform(id));
      const auto matrix = exact ? brush_from_world * scene.world_matrix(id) : AffineTransform{};
      for (auto &p : *posed) {
        if (exact) p = detail::affine_position(matrix.point(detail::affine_vector(p)));
        else { const auto q = ob::transform(transform, {p.x, p.y, p.z}); p = {q.x, q.y, q.z}; }
      }
    }
    positions.push_back(std::move(*posed));
    auto &mapped_locks = locks.emplace_back();
    const auto bones = part.deformation->skeleton->bones.size();
    if (!settings.locks.empty()) {
      mapped_locks.resize(bones);
      for (uint32_t i = 0; i < settings.locks.size(); ++i)
        if (const auto mapped = weight_joint_slot(active, part, i)) mapped_locks.at(*mapped) = settings.locks[i];
    }
    surfaces.push_back({id, &rest, &*part.deformation->skin, positions.back(),
                        WeightSurfaceJoints{bones, direct, reflected, mapped_locks}});
  }
  std::vector<WeightBrush> stroke(brushes.begin(), brushes.end());
  for (auto &sample : stroke) {
    sample.mode = settings.mode; sample.value = settings.value;
    sample.strength *= settings.strength;
  }
  auto result = edit_weights(surfaces, source.skeleton->bones.size(), stroke, options, cancelled);
  Scene next = scene;
  for (const auto &edited : result.surfaces) {
    if (!edited.changed) continue;
    auto &part = next.object(edited.object);
    if (edited.topology != morph_topology_id(part.painted->mesh()))
      throw std::runtime_error("Skin weight result has stale rest topology");
    auto deformation = std::make_shared<DeformationState>(*part.deformation);
    deformation->skin = edited.weights;
    part.deformation = std::move(deformation);
  }
  next.validate(false);
  if (cancelled && cancelled()) throw std::runtime_error("Weight gesture cancelled");
  scene = std::move(next);
  return result;
}
} // namespace dome
