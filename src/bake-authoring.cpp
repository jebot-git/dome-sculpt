// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-authoring.hpp"
#include "dome/source-maps.hpp"
namespace dome {
ObjectBakeReport bake_object(Scene &scene, uint64_t source_id,
                              ObjectBakeSettings settings,
                              const std::function<bool()> &cancelled,
                              const std::function<void(float)> &progress) {
  auto check = [&] {
    if (cancelled && cancelled()) throw std::runtime_error("Object baking cancelled");
  };
  check();
  if(progress)progress(0);
  scene.validate();
  auto target = scene.active();
  auto source = scene.object(source_id);
  if (source.id == target.id)
    throw std::invalid_argument("Choose a different source object for baking");
  if (!target.visible || target.locked || !target.mesh_active || !target.painted)
    throw std::logic_error("Show, unlock and prepare the target painting mesh before baking");
  for (auto mm : {settings.front_mm, settings.back_mm, settings.ao_distance_mm})
    if (!std::isfinite(mm) || mm < 0 || mm > 100'000)
      throw std::invalid_argument("Invalid world-millimetre bake distance");
  if (target.deformation) {
    auto preview = target.deformation->preview(target.painted->mesh());
    const auto &rest = target.painted->mesh();
    for (size_t i = 0; i < rest.positions.size(); ++i)
      if (length(preview.positions[i]-rest.positions[i]) > 1e-6f ||
          length(preview.normals[i]-rest.normals[i]) > 1e-5f)
        throw std::logic_error("Return the target to its rest pose and zero morphs before baking");
  }
  BakeSource high;
  if (source.mesh_active) {
    high.mesh = source.deformation ? source.deformation->preview(source.painted->mesh())
                                   : source.painted->mesh();
    high.materials[0].maps = source.painted->preview(source.material);
  } else
    high.mesh = surface_mesh(*source.volume, cancelled);
  high.materials[0].material = source.material;
  auto target_world = scene.world_transform(target.id);
  auto relative = ob::compose(ob::inverse(target_world), scene.world_transform(source.id));
  for (auto &p : high.mesh.positions) {
    auto world = ob::transform(relative, {p.x,p.y,p.z});
    p = {world.x,world.y,world.z};
  }
  for (auto &n : high.mesh.normals) {
    auto world = ob::rotate(relative.orientation, {n.x,n.y,n.z});
    n = {world.x,world.y,world.z};
  }
  BakeOptions options;
  options.resolution = target.painted->resolution();
  options.front_distance = settings.front_mm * .001f / target_world.scale;
  options.back_distance = settings.back_mm * .001f / target_world.scale;
  options.ao_distance = settings.ao_distance_mm * .001f / target_world.scale;
  options.ray_bias = .0001f / target_world.scale;
  options.ao_samples = settings.ao_samples;
  options.padding = settings.padding;
  options.fail_on_miss = !settings.allow_partial;options.allow_backfaces=settings.allow_backfaces;
  options.cancelled = cancelled;
  if(progress)options.progress=[&](float value){progress(.1f+.8f*value);};
  auto result = bake_mesh(target.painted->mesh(), high, options);
  check();
  if(!result.projected)throw std::runtime_error("No target texels project onto the selected source");
  ObjectBakeReport report{result.projected,result.padded,result.rays,result.missed};
  if(auto prior=target.painted->baked())
    for(size_t i=0;i<result.coverage.size();++i) {
      if((i&1023)==0)check();
      if(result.coverage[i]==3 && prior->coverage[i])++report.preserved_misses;
    }
  BakedTextures textures;
  textures.maps = std::move(result.maps);
  textures.maps.normal = std::move(result.normal);
  textures.coverage.reserve(result.coverage.size());
  for (auto c : result.coverage) textures.coverage.push_back(c==1 || c==2 ? 255 : 0);
  auto painting = target.painted->snapshot();
  painting->install_bake(std::move(textures));
  auto next = scene;
  next.active().painted = std::move(painting);
  next.validate();
  check();
  scene = std::move(next);
  if(progress)progress(1);
  return report;
}
} // namespace dome
