// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh.hpp"
#include "dome/deformation.hpp"
#include <algorithm>
#include <stdexcept>
namespace dome::detail {
// Peak conservative storage for the real current-corner coordinate compiler:
// live input, identity/remapped layouts, hash nodes/buckets and emitted geometry.
inline size_t native_paint_export_working_bytes(const Mesh &mesh,const DeformationState *deformation=nullptr){
 if(mesh.positions.size()>2'000'000||mesh.indices.size()>6'000'000||mesh.indices.size()%3)
  throw std::length_error("Native paint coordinate export exceeds geometry policy");
 const uint64_t vertices=std::min<size_t>(mesh.indices.size(),2'000'000);
 uint64_t bytes=uint64_t(mesh.positions.size())*36+uint64_t(mesh.indices.size())*13+vertices*128+4096;
 if(deformation){
  deformation->validate(mesh);
  if(deformation->morphs&&!deformation->morphs->targets.empty())
   throw std::invalid_argument("Native paint morph export requires its complete shading adapter");
  // Reserve conservative split-skin storage before any coordinate/layout
  // allocation. The normal remapper subsequently verifies exact output entries.
  size_t maximum_influences=0;
  if(deformation->skin)for(const auto &v:deformation->skin->vertices)
   maximum_influences=std::max(maximum_influences,v.size());
  const uint64_t entries=vertices*maximum_influences+deformation->pose.size()*17;
  if(entries>8'000'000)throw std::length_error("Native paint split skin exceeds deformation resource policy before allocation");
  bytes+=entries*32+vertices*sizeof(VertexWeights);
  if(deformation->skeleton)for(const auto &bone:deformation->skeleton->bones)
   bytes+=sizeof(Bone)+bone.name.size()+256;
 }
 if(bytes>512*1024*1024)throw std::length_error("Native paint coordinate export exceeds working policy before allocation");
 return size_t(bytes);
}
}
