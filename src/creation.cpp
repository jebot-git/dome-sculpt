// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
namespace dome {
uint64_t append_humanoid(Scene &scene, const HumanoidControls &controls,
                         std::string name, CanvasTransform transform,
                         uint32_t resolution, const std::function<bool()> &cancelled) {
  if (name.empty() || name.size() > 128 || name.find('\0') != std::string::npos)
    throw std::invalid_argument("Humanoid name must contain 1 to 128 bytes");
  if (resolution < 64 || resolution > 2048 || (resolution & (resolution - 1)))
    throw std::invalid_argument("Invalid humanoid paint resolution");
  validate_transform(transform);
  auto created = create_humanoid(controls, cancelled);
  // Work on a value, leaving the supplied scene exact on any failed part/map.
  Scene result = scene;
  auto group = result.add_group(name, transform, 0,
      std::make_shared<const HumanoidControls>(controls));
  std::vector<uint64_t> parts;
  for (auto &part : created.parts) {
    if (cancelled && cancelled()) throw std::runtime_error("Humanoid creation cancelled");
    PaintSurfaceOptions surface_options;
    surface_options.policy = part.appearance ? PaintUvPolicy::SourceAuthored : PaintUvPolicy::Strict;
    surface_options.cancelled = cancelled;
    auto painted = part.source_maps
        ? PaintedMesh::from_maps(std::move(part.mesh), part.source_maps, resolution, surface_options)
        : std::make_shared<PaintedMesh>(std::move(part.mesh), resolution);
    auto id = result.add(name + " " + part.name, Volume{});
    parts.push_back(id);
    auto &object = result.object(id);
    object.group = group;
    object.material.name = part.name;
    object.material.color = part.color;
    object.material.roughness = part.roughness;
    object.material.toon = part.toon;
    object.material.unlit = part.unlit;
    object.appearance = part.appearance;
    if (part.appearance) {
      const auto &source = part.appearance->asset->materials.at(size_t(part.appearance->material));
      object.material.metallic = source.metallic;
      object.material.emissive = source.emissive;
      object.material.double_sided = source.double_sided;
      object.material.unlit = source.unlit;
      object.material.alpha_cutoff = source.alpha_cutoff;
      object.material.alpha = source.alpha == ImportedMaterial::AlphaMode::Mask ? Material::AlphaMode::Mask
          : source.alpha == ImportedMaterial::AlphaMode::Blend ? Material::AlphaMode::Blend : Material::AlphaMode::Opaque;
    }
    object.painted = std::move(painted);
    object.mesh_active = true;
    result.set_deformation(id, std::move(part.deformation));
  }
  if (created.primary_part >= parts.size()) throw std::runtime_error("Invalid creator primary part");
  result.select(parts[created.primary_part]);
  result.validate();
  scene = std::move(result);
  return group;
}
} // namespace dome
