// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/source-paint.hpp"
#include "dome/humanoid-creator.hpp"
#include "normal-frame-runtime.hpp"
#include <algorithm>
namespace dome::detail {
// Genuine first-source preparation shared by native import and the private
// editable-view compiler. The caller owns its detached destination object;
// no original mesh, frame, appearance, deformation or source image is edited.
// Resource policy is supplied before invoking this operation, never inferred
// from a successful result. Runtime admission still requires the owning factory.
inline void prepare_initial_source_atlas(
    SculptObject &object, uint32_t resolution,
    std::shared_ptr<const std::vector<std::array<float,2>>> authored_uv1,
    const SourcePaintOptions &options = {}) {
  auto stop=[&] {
    if(options.cancelled && options.cancelled())
      throw std::runtime_error("Initial source atlas preparation cancelled");
  };
  stop();
  if(!object.painted || !object.appearance || !object.material_bindings)
    throw std::invalid_argument("Initial source atlas requires its complete native source owners");
  const auto &before=object.painted->mesh();
  auto atlas=before;
  unwrap(atlas,resolution,4,options.cancelled);
  if(atlas.indices.size()!=before.indices.size())
    throw std::runtime_error("Editable atlas changed original triangle count");
  std::vector<uint32_t> vertices(atlas.positions.size(),UINT32_MAX);
  for(size_t corner=0;corner<before.indices.size();++corner) {
    if((corner&4095)==0)stop();
    const auto vertex=atlas.indices[corner],source=before.indices[corner];
    auto &mapped=vertices.at(vertex);
    if(mapped!=UINT32_MAX && mapped!=source)
      throw std::runtime_error("Editable atlas merged different original vertices");
    mapped=source;
  }
  if(std::find(vertices.begin(),vertices.end(),UINT32_MAX)!=vertices.end())
    throw std::runtime_error("Editable atlas contains an unproven vertex");
  auto adopted=prepare_source_paint(object.painted,std::move(atlas),vertices,
                                    std::move(authored_uv1),options);
  auto deformation=normal_frame_remap_deformation(object.deformation,before,
      adopted->mesh(),vertices,options.cancelled);
  auto appearance=std::make_shared<CreatorAppearance>(*object.appearance);
  appearance->source_vertices.clear();
  appearance->source_vertices.reserve(vertices.size());
  for(size_t v=0;v<vertices.size();++v) {
    if((v&1023)==0)stop();
    appearance->source_vertices.push_back(object.appearance->source_vertices.at(vertices[v]));
  }
  normal_frame_current_authority(*object.material_bindings,adopted->mesh(),
                                 deformation.get(),options.cancelled);
  if(!adopted->source_sampling_prepared() || !adopted->valid_source_sampling_attachment())
    throw std::invalid_argument("Editable import lost its authenticated original sampling adapter");
  stop();
  object.painted=std::move(adopted);
  object.appearance=std::move(appearance);
  object.deformation=std::move(deformation);
}
}
