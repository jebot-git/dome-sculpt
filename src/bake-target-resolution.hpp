// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/source-uv-bake.hpp"
#include "dome/scene.hpp"
#include "dome/texture-sets.hpp"
#include <numeric>
namespace dome::detail {
// Existing authored/source/prepared field resizing keeps its separate adapter.
// This narrow recovery route owns an ordinary, unpainted target with no image,
// normal, rig or original-asset authority to resample or discard.
inline bool unpainted_bake_resolution_target(const SculptObject &o)noexcept{
  if(!o.mesh_active||!o.painted||o.texture_sets||o.appearance||o.deformation||o.polygon||o.retopology||
      o.painted->in_stroke()||o.painted->has_paint_data()||o.painted->normal_projection()||o.painted->color_projection())return false;
  if(const auto &b=o.material_bindings){
    if(b->normal_preparation||b->normal_frames||b->canonical_attributes||b->canonical_uv||b->native_paint_sampling)return false;
    for(size_t role=0;role<b->roles.size();++role)if(b->roles[role].image||b->material.textures[role].index>=0)return false;
  }
  return true;
}
inline bool resize_unpainted_bake_target(Scene &scene,uint64_t object,uint32_t resolution,
    const TextureSetLimits &limits={}){
  limits.validate();auto stop=[&]{if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Target paint resolution change cancelled");};stop();
  const auto &before=scene.object(object);
  if(!unpainted_bake_resolution_target(before)||resolution<64||resolution>2048||(resolution&(resolution-1)))
    throw std::invalid_argument("Resize an unpainted Bake target; preserve authored or prepared fields through their owning resolution adapter");
  if(before.painted->resolution()==resolution)return false;
  SourceUvBakeInput input;input.paint=before.painted;input.appearance=before.appearance;
  input.current_material=before.material_bindings;input.current_deformation=before.deformation;
  input.target=before.painted->mesh();input.source_vertices.resize(input.target.positions.size());std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);
  if(!input.current_material)input.native_material=SourceNativeMaterial{before.material.color,before.material.emissive,int(before.material.alpha),before.material.alpha_cutoff,before.material.double_sided};
  SourceUvBakeOptions options;options.resolution=resolution;options.padding=0;options.cancelled=limits.cancelled;
  options.max_working_bytes=std::min(options.max_working_bytes,limits.maximum_bytes);
  options.max_output_bytes=std::min(options.max_output_bytes,limits.maximum_bytes);
  options.max_layer_bytes=std::min(options.max_layer_bytes,limits.maximum_bytes);
  options.max_raster_samples=std::min(options.max_raster_samples,limits.maximum_work);
  options.max_sample_operations=std::min(options.max_sample_operations,limits.maximum_work);
  auto adopted=adopt_source_uv_bake(bake_source_uv(input,options),limits.cancelled);
  auto next=scene.snapshot();auto &target=next.object(object);
  target.painted=std::move(adopted.paint);target.material_bindings=std::move(adopted.material_bindings);
  target.appearance=std::move(adopted.appearance);target.deformation=std::move(adopted.deformation);
  next.validate();stop();scene=std::move(next);return true;
}
} // namespace dome::detail
