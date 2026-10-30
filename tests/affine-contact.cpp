// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
Scene source(bool posed){auto mesh=quad();Scene s;s.active().painted=std::make_shared<PaintedMesh>(mesh,64);s.active().mesh_active=true;auto m=AffineTransform{}.matrix();m[0]=-2;m[2]=.5;m[4]=.3;m[12]=.7;m[13]=-.2;m[14]=.3;s.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(m));
 if(posed){DeformationState d(mesh);d.skeleton=Skeleton{{{"joint",-1,{},1}}};d.skin=SkinWeights{};d.skin->vertices.assign(4,{{0,1}});d.pose.resize(1);d.pose[0].translation={1,0,0};d.pose[0].scale={.5f,1.5f,1.2f};d.morphs=MorphSet(mesh);auto moved=mesh.positions;for(auto&p:moved)p+=Vec3{.2f,.1f,.1f};d.morphs->set_target(mesh,"Authored move",moved);d.morph_weights={1};s.set_deformation(s.active().id,d);}return s;}
void settled(Document&d){check(d.wait_native_stroke()&&d.wait_mesh(),"Ordered contact gesture and immutable preview finish");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
struct Hand{
 Document&d;SpatialFrame f;
 explicit Hand(Document&doc):d(doc){f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[1].tracked.valid=true;send();}
 void point(Vec3 p){auto &h=f.hands[1].tracked;h.grip.position=ob::transform(d.frame().state->canvas,{p.x,p.y,p.z});h.grip.orientation=d.frame().state->canvas.orientation;}
 void send(float pressure=0,bool menu=false){++f.sequence;f.time+=.05;f.hands[1].tracked.trigger=pressure;f.hands[1].joystick=menu;d.submit(f);if(!d.wait_input())throw std::runtime_error("Input worker did not drain contact frame");}
};
}
int main(int argc,char**argv)try{
 const auto home=std::filesystem::temp_directory_path()/("dome-affine-contact-"+std::to_string(getpid()));std::filesystem::create_directories(home);setenv("HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);
 struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{home};
 for(bool posed:{false,true}){
  auto scene=source(posed);Document d;d.replace(scene);settled(d);d.configure(Tool::Clay,.2f);PaintSettings paint;paint.color={1,0,0,1};d.paint_settings(paint);const auto before=d.copy_scene().encode();const auto geometry=d.frame().surface->geometry_identity;
  Hand h(d);const Vec3 center=posed?Vec3{-1.455f,-.05f,.97f}:Vec3{.7f,-.2f,.3f};h.point(center);h.send();h.send(1);
  check(d.frame().state->native_pending,"Actual tracked grip/pressure admits ordered evaluated-world contact painting");
  h.send();auto path=home/(posed?"posed.dsculpt":"static.dsculpt");auto saved=d.save_async(path);saved.get();settled(d);auto result=d.copy_scene();auto current=result.encode();
  check(current!=before&&result.active().painted->pixel(1,32,32).data[9]>200,"Contact paints current placed center on the exact original UV domain");
  auto pixels=result.active().painted;
  check(posed?(pixels->pixel(1,40,32).data[9]>0&&pixels->pixel(1,32,40).data[9]==0):(pixels->pixel(1,32,40).data[9]>0&&pixels->pixel(1,40,32).data[9]==0),"Independent analytic footprint respects combined pose and full affine world metric");
  check(result.active().painted->mesh_owner()==scene.active().painted->mesh_owner()&&result.active().deformation==scene.active().deformation&&result.active().affine_edit==scene.active().affine_edit,"Contact retains immutable REST mesh, pose/skin/morph and source placement owners");
  check(d.frame().surface->geometry_identity==geometry,"Scalar contact paint retains packed geometry identity");
  check(Scene::load(path).encode()==current,"Immediate accepted Save retains exact ordered contact endpoint");
  check(d.undo()&&d.copy_scene().encode()==before&&d.redo()&&d.copy_scene().encode()==current,"Contact gesture is one exact global Undo/Redo");settled(d);
  h.send();h.send(.8f);h.f.focused=false;h.send(.8f);settled(d);check(d.copy_scene().encode()==current,"Focus loss cancels unaccepted contact draft without partial paint");
  h.f.focused=true;h.send();h.send(.8f);h.send(.8f,true);settled(d);check(d.frame().state->menu&&d.frame().state->menu_page==3,"Joystick accepts contact endpoint and opens existing Paint page after commit");
 }
 {
  auto scene=source(true);scene.active().affine_edit.reset();Document d;d.replace(scene);settled(d);d.configure(Tool::Clay,.2f);auto before=d.copy_scene().encode();Hand h(d);h.point({1.1f,.15f,.12f});h.send();h.send(1);h.send();settled(d);auto paint=d.copy_scene().active().painted;
  check(paint->pixel(1,32,32).data[9]>200&&paint->pixel(1,48,32).data[9]>0&&paint->pixel(1,32,48).data[9]==0,"Posed ordinary similarity meshes also use evaluated-world metric rather than synchronous rest paint");
  check(d.undo()&&d.copy_scene().encode()==before,"Ordinary posed contact retains exact original source and Undo");
 }
 {
  auto scene=source(false);Document d;d.replace(scene);settled(d);d.configure(Tool::Clay,.2f);d.configure_object_grab(true,true);settled(d);const auto before=d.copy_scene().encode();const auto world=scene.world_matrix(scene.active().id);
  Hand h(d);h.f.hands[0].tracked.valid=true;h.f.hands[0].tracked.grip.position={0,1.4f,-.3f};h.point({.7f,-.2f,.3f});h.send();h.f.hands[0].tracked.squeeze=1;h.send();
  check(d.frame().state->object_grabbing,"Actual first-hand squeeze holds affine object while second hand is paint tool");
  h.send(1);check(d.frame().state->native_pending,"Contact painting can begin on physically held affine source");
  const auto canvas=d.frame().state->canvas;auto movement=ob::rotate(canvas.orientation,{.2f*canvas.scale,0,0});h.f.hands[0].tracked.grip.position=h.f.hands[0].tracked.grip.position+movement;h.f.hands[1].tracked.grip.position=h.f.hands[1].tracked.grip.position+movement;h.send(1);
  check(d.copy_scene().encode()==before,"Simultaneous physical motion and paint remain detached until ordered release");
  h.f.hands[0].tracked.squeeze=0;h.send(1);auto path=home/"held-endpoint.dsculpt";auto saved=d.save_async(path);saved.get();settled(d);auto result=d.copy_scene();auto expected=world.matrix();expected[12]+=.2;
  for(size_t i=0;i<16;++i)check(std::abs(result.world_matrix(result.active().id).matrix()[i]-expected[i])<2e-6,"Physical held release preserves full affine placement without shear approximation");
  check(!d.frame().state->object_grabbing&&result.active().painted->pixel(1,32,32).data[9]>200,"Released endpoint contains both accepted centered paint and committed physical motion");
  check(Scene::load(path).encode()==result.encode(),"Immediately queued Save retains combined held/paint endpoint");
  check(d.undo()&&d.copy_scene().encode()==before,"Overlapping held movement and contact paint commit as one exact Undo");
 }
 std::cout<<checks<<" actual affine controller contact/pose/metric/ordered-save checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
