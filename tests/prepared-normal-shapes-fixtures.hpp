// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake-authoring.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/shapekey-authoring.hpp"
namespace dome::prepared_shape_fixture {
inline Scene model(bool n_only=false){
 Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{0,0},{1,0},{1,1},{0,1}};m.indices={0,1,2,0,2,3};
 Scene s;auto &o=s.active();o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(m,64);DeformationState d(m);d.morphs=MorphSet(m);auto key=m.positions;key[0].z=.04f;d.morphs->set_target(m,"Original key",key);d.skeleton=Skeleton{{{"root",-1,{},1},{"bend",0,{},1}}};d.skin=SkinWeights{};d.skin->vertices={{{0,.7f},{1,.3f}},{{0,.2f},{1,.8f}},{{0,.4f},{1,.6f}},{{0,.9f},{1,.1f}}};s.set_deformation(o.id,d);
 if(n_only){
  auto f=std::make_shared<NormalFrames>();f->origin_vertex_count=4;f->origin_vertices=m.indices;f->normals.assign(6,normalized(Vec3{.2f,.1f,1}));f->targets.push_back({1,std::vector<Vec3>(6,Vec3{.05f,0,-.01f}),{}});
  auto set=std::make_shared<NormalFrameSet>();set->pbr=set->mtoon=f;set->targets={{1,0,std::nullopt,{}}};set->current_targets={1};set->next_target=2;
  auto origin=std::make_shared<SourceUvOrigin>();origin->paint=o.painted->snapshot();origin->deformation=o.deformation;
  auto b=std::make_shared<SourceMaterialBindings>();b->has_mtoon=false;b->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;b->normal_frames=set;b->uv_origin=origin;o.material_bindings=b;
 }else{ShapeKeyEdit prior;prior.name="Before normal preparation";author_shapekey(s,o.id,prior);}
 auto id=s.add("Readonly high source",Volume{});auto &high=s.object(id);high.visible=false;high.locked=true;high.mesh_active=true;auto h=m;for(auto &p:h.positions){p.x*=2;p.y*=2;p.z=.15f;}h.normals.assign(4,normalized(Vec3{.1f,.15f,1}));high.painted=std::make_shared<PaintedMesh>(h,64);s.select(1);
 ObjectBakeSettings settings;settings.prepare_normals=true;settings.front_mm=500;settings.back_mm=50;settings.ao_samples=1;settings.padding=2;bake_object(s,id,settings);
 auto paint=s.active().painted->snapshot();paint->add_layer("Hidden authored layer");paint->layer_properties("Hidden authored layer",false,.35f);paint->add_layer("Protection");Brush brush;brush.from=brush.to={};brush.normal={0,0,1};brush.radius=.1f;PaintSettings p;p.channel=PaintChannel::Mask;p.mask=.25f;p.front_only=false;paint->begin_stroke();paint->stamp(brush,p);paint->end_stroke();s.active().painted=paint;
 auto live=*s.active().deformation;live.skin->vertices[0]={{0,.4f},{1,.6f}};live.pose={{},{}};live.pose[0].scale={1.2f,.8f,.7f};live.pose[1].scale={.8f,1.1f,1.3f};live.morph_weights.assign(live.morphs->targets.size(),0);live.morph_weights[0]=.35f;s.set_deformation(1,live);s.validate();return s;
}
}
