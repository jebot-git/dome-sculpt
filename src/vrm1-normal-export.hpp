// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "normal-frame-export.hpp"
#include "vrm1-authoring.hpp"
namespace dome::detail {
inline NormalFrameExport normal_frame_export_vrm1(const Vrm1AuthoringCompilation &owner,
 const ObjectSurface &surface,const std::function<bool()> &cancelled) {
 if(!owner.owns(surface))throw std::invalid_argument("VRM1 normal export requires its actual sealed compiled surface");
 if(!surface.material_bindings||!surface.material_bindings->roles[size_t(MToonSlot::Normal)].image)
  return normal_frame_export(surface,SourceMorphSpace::Native,cancelled,true);
 const auto &source=owner.capture().scene().object(surface.id);
 require_vrm1_authored_normal_frame(source,cancelled);
 if(surface.material_bindings!=source.material_bindings||surface.deformation!=source.deformation||surface.appearance!=source.appearance||
    !surface.textures||!surface.textures->source_only||surface.textures->overlay||!surface.textures->source_maps||
    surface.textures->source_maps->normal.image!=source.material_bindings->roles[size_t(MToonSlot::Normal)].image)
  throw std::invalid_argument("VRM1 normal export lost its original object/attribute/sampling owner");
 Vrm1NormalFrameExportProof proof(surface);
 return normal_frame_export_impl(surface,SourceMorphSpace::Native,cancelled,true,&proof);
}
} // namespace dome::detail
