// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-preparation.hpp"
namespace dome::normal_preparation_fixture {
Mesh quad(){Mesh m;m.positions={{0,0,0},{1,0,0},{1,1,.08f},{0,1,0}};m.normals.assign(4,{0,0,1});m.uv={{0,0},{1,0},{1,1},{0,1}};m.indices={0,1,2,0,2,3};MorphSet morphs(m);return morphs.evaluate(m,{});}
Scene model(){
  auto mesh=quad();Scene scene;auto &target=scene.active();target.mesh_active=true;target.painted=std::make_shared<PaintedMesh>(mesh,64);
  auto deform=std::make_shared<DeformationState>(mesh);deform->morphs=MorphSet(mesh);auto key=mesh.positions;key[1].z+=.06f;deform->morphs->set_target(mesh,"raised native key",key);
  deform->skeleton=Skeleton{{{"root",-1,{},1},{"bend",0,{},1}}};deform->skin=SkinWeights{};deform->skin->vertices={{{0,.7f},{1,.3f}},{{0,.2f},{1,.8f}},{{0,.4f},{1,.6f}},{{0,.9f},{1,.1f}}};target.deformation=deform;
  auto id=scene.add("Read-only high source",Volume{});auto &source=scene.object(id);source.mesh_active=true;source.visible=false;source.locked=true;
  Mesh high;high.positions={{-.5f,-.5f,.15f},{1.5f,-.5f,.15f},{1.5f,1.5f,.15f},{-.5f,1.5f,.15f}};high.normals={normalized(Vec3{-.1f,.1f,1}),normalized(Vec3{.15f,0,1}),normalized(Vec3{0,.2f,1}),normalized(Vec3{-.1f,-.2f,1})};high.uv=mesh.uv;high.indices=mesh.indices;source.painted=std::make_shared<PaintedMesh>(high,64);scene.select(1);scene.validate();return scene;
}
} // namespace dome::normal_preparation_fixture
