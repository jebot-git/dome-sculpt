// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "normal-frame-export.hpp"
#include "vrm1-authoring.hpp"
namespace dome::detail {
inline NormalFrameExport normal_frame_export_vrm1(const Vrm1AuthoringCompilation &owner,
 const ObjectSurface &surface,const std::function<bool()> &cancelled) {
 if(!owner.owns(surface))throw std::invalid_argument("VRM1 normal export requires its actual sealed compiled surface");
 const auto &binding=surface.material_bindings;
 const auto maps=surface.textures?surface.textures->source_maps:nullptr;
 const bool typed=binding&&binding->has_mtoon&&binding->roles[size_t(MToonSlot::Normal)].image;
 const bool transformed_core=binding&&!binding->has_mtoon&&maps&&maps->normal.image&&maps->normal_scale!=0&&
     maps->normal.uv_transform!=std::array<float,6>{1,0,0,1,0,0};
 if(!typed&&!transformed_core)
  return normal_frame_export(surface,SourceMorphSpace::Native,cancelled,true);
 const auto &source=owner.capture().scene().object(surface.id);
 if(surface.material_bindings!=source.material_bindings||surface.deformation!=source.deformation||surface.appearance!=source.appearance||
    !surface.textures||!surface.textures->source_only||surface.textures->overlay||!maps)
  throw std::invalid_argument("VRM1 normal export lost its original object/attribute/sampling owner");
 if(typed){
  require_vrm1_authored_normal_frame(source,cancelled);
  if(maps->normal.image!=binding->roles[size_t(MToonSlot::Normal)].image)
   throw std::invalid_argument("VRM1 normal export lost its shared Core PBR/MToon original image");
 }else{
  const auto &primitive=require_vrm1_core_normal_frame(source,cancelled);
  verify_vrm1_generated_normal_frame(source,primitive,cancelled);
  const auto original=source.painted->source_sampling_original();
  const auto expected=original?original->source_maps():source.painted->source_maps();
  // The sealed compiler reconstructs atlas coordinates from their original
  // witnesses. The normal role must still use its actual original sampler.
  if(!expected||(!original&&expected!=maps)||maps->normal.image!=expected->normal.image||
     maps->normal.texcoord!=expected->normal.texcoord||maps->normal.uv_transform!=expected->normal.uv_transform||
     maps->normal.wrap_s!=expected->normal.wrap_s||maps->normal.wrap_t!=expected->normal.wrap_t||
     maps->normal.min_filter!=expected->normal.min_filter||maps->normal.mag_filter!=expected->normal.mag_filter||
     maps->normal_scale!=expected->normal_scale)
   throw std::invalid_argument("VRM1 Core PBR normal export lost its actual original sampler and transform");
 }
 Vrm1NormalFrameExportProof proof(surface);
 return normal_frame_export_impl(surface,SourceMorphSpace::Native,cancelled,true,&proof);
}
} // namespace dome::detail
