// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-paint-export-policy.hpp"
#include "canonical-paint-sampling.hpp"
#include "normal-frame-runtime.hpp"
#include "export-corner-coordinates.hpp"
namespace dome::detail {
// Canonical shading and retained writable paint are independent owners.
// Forecast the real joint corner expansion only after proving both; this
// forecast supplies no authority to a caller or a different export path.
inline size_t canonical_paint_export_working_bytes(const SculptObject &object,
    size_t maximum_bytes,const std::function<bool()> &cancelled={}) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Canonical paint export budget cancelled");};
  stop();
  const auto binding=object.material_bindings;
  const auto sampling=binding?binding->canonical_paint_sampling:nullptr;
  if(!object.mesh_active||!object.painted||object.appearance||!sampling||
      !sampling->valid_owner(sampling)||!sampling->matches_current(*object.painted)||!binding->normal_frames)
    throw std::invalid_argument("Canonical export budget requires genuine current normal and paint owners");
  const auto &paint=*object.painted;const auto &mesh=paint.mesh();
  const uint64_t limit=std::min<size_t>(maximum_bytes,512*1024*1024);
  uint64_t bytes=native_paint_export_working_bytes(mesh);
  auto charge=[&](uint64_t count,uint64_t width){
    if(bytes>limit||count>(limit-bytes)/width)
      throw std::length_error("Canonical paint extraction exceeds working budget before payload allocation");
    bytes+=count*width;
  };
  // The existing geometry envelope covers the bounded layout/hash scratch.
  // Refuse a tiny caller budget before even constructing those layouts.
  charge(0,1);
  sampling->validate_binding(*binding,true);
  normal_frame_current_authority(*binding,mesh,object.deformation.get(),cancelled);
  PaintSamplingLimits sampling_limits;sampling_limits.cancelled=cancelled;
  sampling->validate_current(paint,sampling_limits);
  const auto coordinates=sampling->attachment()->domain()->corner_coordinates(mesh,sampling_limits);
  const auto base=normal_frame_layout(mesh,*binding->normal_frames,cancelled);
  const auto split=split_frame_corner_coordinates(mesh,base,coordinates,cancelled);
  const uint64_t vertices=split.frames.vertices.size();
  const auto deformation=object.deformation;
  uint64_t entries=0;constexpr uint64_t model_limit=8'000'000;
  auto model=[&](uint64_t count){
    if(entries>model_limit||count>model_limit-entries)
      throw std::length_error("Canonical paint split model exceeds deformation resource policy before payload allocation");
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
  if(!selected)throw std::invalid_argument("Canonical paint export budget has no actual PBR frame");
  uint64_t directions=0;
  for(const auto id:binding->normal_frames->current_targets){
    stop();const auto found=std::find_if(selected->targets.begin(),selected->targets.end(),[&](const auto &target){return target.id==id;});
    if(found==selected->targets.end())throw std::invalid_argument("Canonical paint export budget has no actual target frame identity");
    directions+=!found->normals.empty();directions+=!found->tangents.empty();
  }
  if(directions&&vertices>model_limit/directions)
    throw std::length_error("Canonical paint split directions exceed shading resource policy before payload allocation");
  charge(vertices*directions,32);
  stop();return size_t(bytes);
}
}
