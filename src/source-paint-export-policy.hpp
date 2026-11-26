// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-paint-export-policy.hpp"
#include "source-paint-pairing.hpp"
#include "normal-frame-runtime.hpp"
#include "export-corner-coordinates.hpp"
namespace dome::detail {
// Genuine Source8 prepared normals and corner colours own separate shading and
// writable paint domains. Forecast the actual refined export layout and model
// only after validating their independent current/source authorities.
inline size_t source_paint_export_working_bytes(const SculptObject &object,
    size_t maximum_bytes,const std::function<bool()> &cancelled={}) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Source8 paint export budget cancelled");};
  stop();
  const auto binding=object.material_bindings;
  const auto sampling=binding&&binding->color_preparation?binding->color_preparation->paint_sampling:nullptr;
  if(!object.mesh_active||!object.painted||!binding||binding->has_mtoon||!sampling||
      binding->native_paint_sampling||binding->canonical_paint_sampling||binding->native_paint_frames||
      !binding->normal_frames||!binding->normal_preparation||!binding->normal_preparation->sampling_domain||
      !sampling->matches_current(*object.painted))
    throw std::invalid_argument("Source8 export budget requires its genuine prepared normal and current paint owners");
  binding->validate();source_paint_pairing(object.painted.get(),binding.get(),object.appearance.get(),true);
  const auto &paint=*object.painted;const auto &mesh=paint.mesh();
  const uint64_t limit=std::min<size_t>(maximum_bytes,512*1024*1024);
  uint64_t bytes=native_paint_export_working_bytes(mesh);
  auto charge=[&](uint64_t count,uint64_t width){
    if(bytes>limit||count>(limit-bytes)/width)
      throw std::length_error("Source8 paint extraction exceeds working budget before payload allocation");
    bytes+=count*width;
  };
  // The existing geometry envelope covers the bounded layout/hash scratch.
  // Refuse a tiny caller budget before even constructing those layouts.
  charge(0,1);
  sampling->validate(false,cancelled);
  normal_frame_current_authority(*binding,mesh,object.deformation.get(),cancelled);
  PaintSamplingLimits sampling_limits;sampling_limits.cancelled=cancelled;
  sampling->validate_current(mesh,cancelled);
  const auto coordinates=sampling->geometry_attachment()->domain()->corner_coordinates(mesh,sampling_limits);
  const auto base=normal_frame_layout(mesh,*binding->normal_frames,cancelled);
  const auto split=split_frame_corner_coordinates(mesh,base,coordinates,cancelled);
  const uint64_t vertices=split.frames.vertices.size();
  const auto deformation=object.deformation;
  uint64_t entries=0;constexpr uint64_t model_limit=8'000'000;
  auto model=[&](uint64_t count){
    if(entries>model_limit||count>model_limit-entries)
      throw std::length_error("Source8 paint split model exceeds deformation resource policy before payload allocation");
    entries+=count;
  };
  if(deformation){
    model(deformation->pose.size());model(deformation->morph_weights.size());
    for(const auto &pose:deformation->pose)if(pose.full_local_matrix)model(16);
    if(deformation->morphs)for(const auto &target:deformation->morphs->targets){
      stop();model(vertices);charge(1,sizeof(MorphTarget)+target.name.size()+64);
    }
    if(deformation->skin){
      for(size_t n=0;n<split.frames.vertices.size();++n){
        if((n&1023)==0)stop();
        model(deformation->skin->vertices.at(split.frames.vertices[n]).size());
      }
      charge(vertices,sizeof(VertexWeights));
    }
    charge(entries,32);
    if(deformation->skeleton)for(const auto &bone:deformation->skeleton->bones){
      stop();charge(1,sizeof(Bone)+bone.name.size()+256);
    }
  }
  const auto selected=binding->normal_frames->pbr;
  if(!selected)throw std::invalid_argument("Source8 paint export budget has no actual PBR frame");
  uint64_t directions=0;
  for(const auto id:binding->normal_frames->current_targets){
    stop();const auto found=std::find_if(selected->targets.begin(),selected->targets.end(),[&](const auto &target){return target.id==id;});
    if(found==selected->targets.end())throw std::invalid_argument("Source8 paint export budget has no actual target frame identity");
    directions+=!found->normals.empty();directions+=!found->tangents.empty();
  }
  if(directions&&vertices>model_limit/directions)
    throw std::length_error("Source8 paint split directions exceed shading resource policy before payload allocation");
  charge(vertices*directions,32);
  stop();return size_t(bytes);
}
}
