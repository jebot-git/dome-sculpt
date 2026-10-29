// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh-authoring.hpp"
#include "dome/mesh-processing.hpp"
namespace dome {
uint64_t create_voxel_sculpt(Scene &scene, float world_voxel_mm,
                             const std::function<bool()> &cancelled) {
  if (!std::isfinite(world_voxel_mm) || world_voxel_mm < .01f ||
      world_voxel_mm > 1000)
    throw std::invalid_argument("Voxel size must be between 0.01 and 1000 mm");
  scene.validate();
  auto source = scene.active();
  if (!source.visible || source.locked)
    throw std::logic_error("Show and unlock the source before voxel remeshing");
  if (scene.objects().size() >= 256)
    throw std::length_error("The scene object limit is reached");
  size_t used = 0;
  for (auto &object : scene.objects())
    used += object.volume->stored_samples();
  if (used >= 4'000'000)
    throw std::length_error("No sculpt sample budget remains for a new object");
  Mesh mesh;
  if (source.mesh_active) {
    if (!source.painted)
      throw std::logic_error("The source has no prepared mesh");
    mesh = source.deformation ? source.deformation->preview(source.painted->mesh())
                              : source.painted->mesh();
  } else
    mesh = surface_mesh(*source.volume, cancelled);
  if (mesh.indices.empty())
    throw std::runtime_error("The source has no surface to remesh");
  VoxelizeOptions options;
  options.voxel_size = world_voxel_mm * .001f / source.transform.scale;
  options.max_grid_points = 4'000'000 - used;
  options.cancelled = cancelled;
  auto volume = mesh_to_volume(mesh, options);
  if (std::none_of(volume.voxels().begin(), volume.voxels().end(),
                   [](const auto &sample) { return sample.second < 0; }))
    throw std::runtime_error("Voxel size is too coarse for this shape; choose a finer size");
  if (cancelled && cancelled())
    throw std::runtime_error("Voxel remeshing cancelled");
  auto name = source.name.size() <= 250 ? source.name + " voxel" : "Voxel sculpt";
  auto id = scene.add(std::move(name), std::move(volume), source.transform);
  scene.object(id).material = source.material;
  scene.object(source.id).visible = false;
  return id;
}
} // namespace dome
