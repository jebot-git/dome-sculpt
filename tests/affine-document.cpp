// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
#include "dome/material-visibility.hpp"
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
void ready(Document&d){check(d.wait_mesh(std::chrono::seconds(10)),"actual affine Document surface becomes ready");check(d.frame().state->error.empty(),"affine publication reports no failure");}
}
int main(int argc,char **argv)try{
  auto home=std::filesystem::temp_directory_path()/("dome-affine-document-"+std::to_string(getpid()));std::filesystem::create_directories(home);
  setenv("HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);
  struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{home};
  auto mesh=quad();Scene scene;scene.active().painted=std::make_shared<PaintedMesh>(mesh,64);scene.active().mesh_active=true;
  auto matrix=AffineTransform{}.matrix();matrix[0]=-2;matrix[2]=.5;matrix[4]=.3;matrix[12]=.7;matrix[13]=-.2;matrix[14]=.3;
  if(argc==3&&std::string(argv[1])=="--ui-fixture") {
    matrix[12]=matrix[13]=matrix[14]=0;
    scene.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(matrix));
    scene.save(argv[2]);return 0;
  }
  scene.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(matrix));
  Document document;document.replace(scene);ready(document);auto initial=document.copy_scene().encode();
  auto snapshot=document.frame().surface;
  check(snapshot->ranges.size()==1&&snapshot->ranges[0].affine_world.has_value(),"range retains full packed affine frame");
  check(document.frame().state->objects[0].affine_world.has_value()&&!document.frame().state->objects[0].world_transform_valid,"published object never labels an affine frame as Canvas similarity");
  const auto normal=normalized(Vec3{.25f,-.075f,1});
  for(size_t i=0;i<4;++i){const auto p=mesh.positions[i];const Vec3 expected{-2*p.x+.3f*p.y+.7f,p.y-.2f,.5f*p.x+.3f};check(length(snapshot->mesh.positions[i]-expected)<1e-6,"actual worker packs exact affine positions");check(length(snapshot->mesh.normals[i]-normal)<1e-6,"actual worker applies inverse-transpose normals");}
  check(snapshot->mesh.indices==std::vector<uint32_t>({0,2,1,0,3,2}),"reflection front faces are compensated in transient indices only");
  check(document.copy_scene().active().painted->mesh().indices==mesh.indices,"original ordered corner/UV authority is unchanged");
  const Vec3 center{.7f,-.2f,.3f};check(snapshot->picking&&snapshot->picking->raycast(center+normal,normal*-1.f,2).has_value(),"material-aware reflected front face is pickable");
  check(!snapshot->picking->raycast(center+normal*-1.f,normal,2),"reflected single-sided back face remains culled");
  auto hit=document.raycast(center+normal,normal*-1.f);check(hit&&length(*hit-center)<1e-6,"public Document pick uses current world placement");
  check(length(document.gradient(center)-normal)<1e-6,"public gradient selects the placed active surface normal");
  PaintSettings settings;settings.color={1,0,0,1};document.paint_settings(settings);
  NativeStrokeSample sample;sample.origin=center+normal;sample.direction=normal*-1.f;sample.brush.normal=normal;sample.brush.radius=.2f;sample.brush.blend=0;
  check(document.begin_native_stroke()&&document.sample_native_stroke(sample),"actual worker admits affine native paint");document.end_native_stroke();check(document.wait_native_stroke(std::chrono::seconds(10)),"affine native paint drains");ready(document);
  auto painted=document.copy_scene().active().painted;
  check(document.frame().surface->geometry_identity==snapshot->geometry_identity,"painting reuses the exact geometry identity under affine placement");
  check(painted->pixel(1,32,32).data[9]>200,"world-space native paint maps the original UV center");
  check(painted->pixel(1,32,40).data[9]>0&&painted->pixel(1,40,32).data[9]==0,"spherical world brush respects nonuniform/sheared metric, not guessed scalar radius");
  check(document.copy_scene().active().affine_edit==scene.active().affine_edit&&std::equal(painted->mesh().positions.begin(),painted->mesh().positions.end(),mesh.positions.begin(),[](Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;})&&painted->mesh().uv==mesh.uv,"paint preserves immutable placement and original source geometry/UV");
  check(document.undo()&&document.copy_scene().encode()==initial,"affine paint has one exact Undo");ready(document);
  check(document.redo(),"affine painted endpoint can redo");ready(document);
  auto saved=home/"affine.dome";document.save(saved);check(Scene::load(saved).encode()==document.copy_scene().encode(),"actual native save/restart retains placement and painted endpoint");
  auto before_cancel=document.copy_scene().encode();check(document.begin_native_stroke()&&document.sample_native_stroke(sample),"second affine native gesture starts");document.end_native_stroke(false);check(document.wait_native_stroke(std::chrono::seconds(10)),"affine cancellation drains");check(document.copy_scene().encode()==before_cancel,"cancel preserves exact source and previously accepted paint");
  // Independent analytic combined skin/morph case. The rest surface stays near
  // x=.7, whereas the current posed surface is centered at x=-1.455.
  DeformationState deformation(mesh);deformation.skeleton=Skeleton{{{"joint",-1,{},1}}};
  deformation.skin=SkinWeights{};deformation.skin->vertices.assign(4,{{0,1}});
  deformation.pose.resize(1);deformation.pose[0].translation={1,0,0};deformation.pose[0].scale={.5f,1.5f,1.2f};
  deformation.morphs=MorphSet(mesh);auto moved=mesh.positions;for(auto&p:moved)p+=Vec3{.2f,.1f,.1f};deformation.morphs->set_target(mesh,"Authored move",moved);deformation.morph_weights={1};
  scene.set_deformation(scene.active().id,deformation);document.replace(scene);ready(document);
  const auto posed_before=document.copy_scene().encode();const Vec3 posed_center{-1.455f,-.05f,.97f};
  hit=document.raycast(posed_center+normal,normal*-1.f);check(hit&&length(*hit-posed_center)<1e-6,"public pick follows combined current skin/morph and affine placement");
  check(!document.raycast(center+normal,normal*-1.f),"picking does not hit the displaced immutable rest surface");
  sample.origin=posed_center+normal;document.paint_settings(settings);check(document.begin_native_stroke()&&document.sample_native_stroke(sample),"posed affine native paint begins");document.end_native_stroke();check(document.wait_native_stroke(std::chrono::seconds(10)),"posed affine native paint drains");ready(document);
  painted=document.copy_scene().active().painted;
  check(painted->pixel(1,32,32).data[9]>200&&painted->pixel(1,40,32).data[9]>0&&painted->pixel(1,32,40).data[9]==0,"posed metric paints the analytically reversed UV footprint");
  check(document.copy_scene().active().deformation==scene.active().deformation,"native painting retains exact skin/morph authority");
  check(document.undo()&&document.copy_scene().encode()==posed_before,"combined posed affine paint has one exact Undo");
  std::cout<<checks<<" actual affine Document packing/picking/native-paint checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
