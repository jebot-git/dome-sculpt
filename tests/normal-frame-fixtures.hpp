// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/material-bindings.hpp"
#include "dome/scene.hpp"
namespace normal_frame_fixture {
inline dome::Scene cube() {
  using namespace dome;
  Mesh mesh;mesh.positions={{-.2f,-.2f,-.2f},{.2f,-.2f,-.2f},{.2f,.2f,-.2f},{-.2f,.2f,-.2f},{-.2f,-.2f,.2f},{.2f,-.2f,.2f},{.2f,.2f,.2f},{-.2f,.2f,.2f}};
  for(auto p:mesh.positions)mesh.normals.push_back(normalized(p));
  mesh.indices={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};unwrap(mesh,128);
  auto paint=std::make_shared<PaintedMesh>(mesh,128);auto origin=std::make_shared<SourceUvOrigin>();origin->paint=paint->snapshot();
  auto frame=std::make_shared<NormalFrames>();frame->origin_vertex_count=mesh.positions.size();frame->origin_vertices=mesh.indices;
  for(auto v:mesh.indices){frame->normals.push_back(mesh.normals[v]);frame->tangents.push_back({1,0,0,1});}
  auto registry=std::make_shared<NormalFrameSet>();registry->pbr=frame;
  auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->uv_origin=origin;binding->normal_frames=registry;binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;
  Scene scene;scene.active().painted=paint;scene.active().mesh_active=true;scene.active().material_bindings=binding;return scene;
}
}
