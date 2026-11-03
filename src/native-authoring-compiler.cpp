// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-authoring-compiler.hpp"
#include "dome/source-paint-bake.hpp"
#include "source-paint-export.hpp"
#include <algorithm>
#include <map>
#include <numeric>
namespace dome::detail {
NativeAuthoringCompilation compile_native_authoring(
    const NativeAuthoringCapture &capture, const SourceUvBakeOptions &policy,
    size_t output_limit, const std::function<bool()> &cancelled) {
  const auto stop = [first=cancelled,second=policy.cancelled] {
    return (first && first()) || (second && second());
  };
  auto check = [&] {
    if (stop()) throw std::runtime_error("Native authoring compilation cancelled");
  };
  check();
  size_t fixed = sizeof(Scene) + sizeof(NativeAuthoringCompilation);
  auto reserve = [&](size_t count, size_t unit) {
    if (fixed > policy.max_working_bytes ||
        count > (policy.max_working_bytes-fixed)/unit)
      throw std::length_error("Native authoring geometry staging exceeds working budget");
    fixed += count*unit;
  };
  reserve(capture.originals().size(),sizeof(ObjectSurface));
  reserve(capture.scene().objects().size(),sizeof(SculptObject));
  reserve(capture.scene().groups().size(),sizeof(SceneGroup));
  bool painted = false;
  for (const auto &surface : capture.originals()) {
    check();
    const auto &mesh = surface.mesh;
    reserve(mesh.positions.size()+mesh.normals.size(),sizeof(Vec3));
    reserve(mesh.uv.size(),sizeof(std::array<float,2>));
    reserve(mesh.indices.size(),sizeof(uint32_t));
    reserve(mesh.uv_pins.size()+mesh.uv_seams.size(),sizeof(uint8_t));
    reserve(surface.name.size()+1,sizeof(char));
    reserve(surface.groups.size(),sizeof(SceneGroup));
    for(const auto &group:surface.groups)reserve(group.name.size()+1,sizeof(char));
    const auto &object = capture.scene().object(surface.id);
    if (object.painted && object.painted->source_sampling_prepared()) {
      const auto &maps = surface.textures;
      if (!maps || !maps->overlay || !object.painted->valid_source_sampling_attachment())
        throw std::invalid_argument("Native authoring lost its original source paint owner");
      painted |= !maps->overlay->empty();
    }
  }
  NativeAuthoringCompilation result;
  if (!painted) {
    result.surfaces = capture.originals();
    for (auto &surface : result.surfaces) {
      check();
      const auto &object = capture.scene().object(surface.id);
      if (object.painted && object.painted->source_sampling_prepared())
        source_paint_export(*object.painted,surface,stop);
    }
    return result;
  }
  // The capture already owns an immutable snapshot. A shallow Scene copy only
  // replaces completed paint/material pointers; it never edits borrowed meshes,
  // volumes, recipes or controls. Reserve vector/string/adoption staging first.
  for(const auto &group:capture.scene().groups())reserve(group.name.size()+1,sizeof(char));
  for (const auto &object : capture.scene().objects()) {
    reserve(object.name.size()+object.material.name.size()+2,sizeof(char));
    if (object.painted) {
      reserve(object.painted->layers().size(),sizeof(PaintLayer));
      for (const auto &layer : object.painted->layers()) {
        reserve(layer.name.size()+1,sizeof(char));
        reserve(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size(),
                sizeof(std::shared_ptr<const PaintTile>));
      }
    }
  }
  auto staged = capture.scene();
  struct View {
    std::shared_ptr<const TextureMaps> textures;
    std::shared_ptr<const SourceMaterialBindings> material;
  };
  std::map<uint64_t,View> views;
  size_t retained_images = 0, samples = 0, raster = 0;
  const size_t maximum_images = std::min(policy.max_output_bytes,output_limit);
  for (const auto &surface : capture.originals()) {
    check();
    const auto &object = capture.scene().object(surface.id);
    if (!object.painted || !object.painted->source_sampling_prepared() ||
        surface.textures->overlay->empty()) continue;
    SourceUvBakeOptions bounded = policy;
    bounded.cancelled = stop;
    if (retained_images >= maximum_images || fixed >= policy.max_working_bytes ||
        retained_images >= policy.max_working_bytes-fixed ||
        samples >= policy.max_sample_operations || raster >= policy.max_raster_samples)
      throw std::length_error("Native authoring cumulative material budget exhausted");
    bounded.max_output_bytes = maximum_images-retained_images;
    bounded.max_working_bytes = policy.max_working_bytes-fixed-retained_images;
    bounded.max_sample_operations = policy.max_sample_operations-samples;
    bounded.max_raster_samples = policy.max_raster_samples-raster;
    SourceUvBakeInput input;
    input.paint = object.painted;
    input.appearance = object.appearance;
    input.current_material = object.material_bindings;
    input.current_deformation = object.deformation;
    input.target = object.painted->mesh();
    input.source_vertices.resize(input.target.positions.size());
    std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);
    auto compiled = bake_source_paint(input,object.material,bounded);
    const auto &sampling = compiled.sampling;
    if (sampling.image_bytes > maximum_images-retained_images ||
        sampling.image_bytes > policy.max_working_bytes-fixed-retained_images ||
        sampling.sample_operations > policy.max_sample_operations-samples ||
        sampling.raster_samples > policy.max_raster_samples-raster)
      throw std::length_error("Native authoring material result exceeds cumulative budget");
    retained_images += sampling.image_bytes;
    samples += sampling.sample_operations;
    raster += sampling.raster_samples;
    auto adopted = adopt_source_uv_bake(sampling,stop);
    auto &next = staged.object(object.id);
    next.painted = std::move(adopted.paint);
    next.deformation = std::move(adopted.deformation);
    next.appearance = std::move(adopted.appearance);
    next.material_bindings = std::move(adopted.material_bindings);
    views.emplace(object.id,View{std::move(compiled.textures),
                                 std::move(compiled.material_bindings)});
    result.diagnostics.push_back("Object "+std::to_string(object.id)+
        ": explicit finite PBR paint view, original editable source retained");
  }
  check();
  staged.validate();
  result.surfaces = staged.export_surfaces(stop);
  for (auto &surface : result.surfaces) {
    check();
    if (auto view=views.find(surface.id);view!=views.end()) {
      surface.textures = view->second.textures;
      surface.material_bindings = view->second.material;
    }
  }
  if (result.surfaces.size()!=capture.ids().size())
    throw std::invalid_argument("Native authoring compilation changed emitted object membership");
  return result;
}
} // namespace dome::detail
