// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/weight-authoring.hpp"
#include "dome/humanoid-creator.hpp"
#include <nlohmann/json.hpp>
namespace dome {
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
  auto group = source.group;
  auto path = scene.group_path(group);
  for (auto it = path.rbegin(); it != path.rend(); ++it)
    if (it->humanoid) { group = it->id; break; }
  auto rig = source.deformation->skeleton->encode();
  std::vector<uint64_t> result;
  for (const auto &part : scene.objects()) {
    if (part.id != object) {
      if (!linked || !group || !part.visible || part.locked || !part.mesh_active || !part.painted ||
          !part.deformation || !part.deformation->skeleton || !part.deformation->skin) continue;
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
  const auto &source = *scene.object(object).deformation;
  settings.validate(source.skeleton->bones.size());
  auto inverse = ob::inverse(scene.world_transform(object));
  std::vector<std::vector<Vec3>> positions;
  positions.reserve(ids.size());
  std::vector<WeightPaintSurface> surfaces;
  for (auto id : ids) {
    if (cancelled && cancelled()) throw std::runtime_error("Weight gesture cancelled");
    const auto &part = scene.object(id);
    auto preview = part.deformation->preview(part.painted->mesh());
    auto transform = ob::compose(inverse, scene.world_transform(id));
    for (auto &p : preview.positions) {
      auto q = ob::transform(transform, {p.x, p.y, p.z}); p = {q.x, q.y, q.z};
    }
    positions.push_back(std::move(preview.positions));
    surfaces.push_back({id, &part.painted->mesh(), &*part.deformation->skin, positions.back()});
  }
  WeightEditOptions options; options.locks = settings.locks; options.limits = settings.limits;
  if (settings.symmetry) {
    if (!source.humanoid) throw std::invalid_argument("Weight symmetry requires explicit humanoid joint mapping");
    options.symmetry = WeightBrushSymmetry{settings.symmetry_plane, source.humanoid->mirror(*source.skeleton)};
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
