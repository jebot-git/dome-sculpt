// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <future>
#include <iostream>
using namespace dome;
namespace dome {struct DocumentTransactionTestAccess {
 static void delayed_weight(Document &d,std::promise<void>&started,std::shared_future<void> release){
  std::lock_guard lock(d.model_mutex_);auto work=std::make_shared<AssetWork>();work->held_weight=true;
  d.queue_scene_operation_locked("Paint skin weights",[&started,release](Scene&s,const auto&cancelled){started.set_value();if(release.wait_for(std::chrono::seconds(5))!=std::future_status::ready||cancelled())throw std::runtime_error("Held worker cancelled");s.active().material.color[0]=.25f;},{},{},work);
 }
};}
namespace {
int checks=0;void check(bool ok,const char*t){if(!ok)throw std::runtime_error(t);++checks;std::cout<<"PASS "<<t<<'\n';}
Mesh triangle(){Mesh m;m.positions={{-.4f,-.4f,0},{.4f,-.4f,0},{0,.4f,0}};m.normals.assign(3,{0,0,1});m.uv={{0,0},{1,0},{.5f,1}};m.indices={0,1,2};return m;}
Scene planes(bool skin=false){Scene s;auto a=s.active().id;auto b=s.add("Readonly source",Volume{});auto group=skin?s.add_group("Linked rig"):0;
 for(auto id:{a,b}){auto&o=s.object(id);o.painted=std::make_shared<PaintedMesh>(triangle(),64);o.mesh_active=true;o.group=group;if(skin){DeformationState d(o.painted->mesh());Skeleton rig;rig.bones={{"root",-1,{},.1f,true},{"other",0,{{0,.1f,0},{},{1,1,1}},.1f,true}};d.skeleton=rig;SkinWeights weights;weights.vertices.assign(3,{{0,1}});d.skin=weights;s.set_deformation(id,d);}}
 if(!skin)s.object(b).transform.position.x=.7f;s.select(a);return s;
}
struct Controller {
 Document&d;SpatialFrame f;
 explicit Controller(Document&doc):d(doc){f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[0].tracked.valid=f.hands[1].tracked.valid=true;f.hands[0].tracked.grip.position={0,1.4f,-.3f};f.hands[1].tracked.grip.position={2,2,2};send();}
 void send(){++f.sequence;f.time+=.05;d.submit(f);if(!d.wait_input())throw std::runtime_error("Controller input timed out");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
 void grab(){f.hands[0].tracked.squeeze=1;send();check(d.frame().state->object_grabbing,"One physical controller grip establishes actual held ownership");f.hands[0].tracked.grip.position.x+=.2f;send();}
 void point(Vec3 local){auto s=d.frame().state;auto i=std::find_if(s->objects.begin(),s->objects.end(),[&](const auto&o){return o.id==s->held_object;});auto world=ob::compose(s->canvas,i->world_transform);f.hands[1].tracked.grip.position=ob::transform(world,{local.x,local.y,local.z});}
 void stroke(Vec3 local){point(local);f.hands[1].tracked.trigger=1;send();send();f.hands[1].tracked.trigger=0;send();}
 void release(){f.hands[0].tracked.squeeze=0;send();}
 void sector(int i){const auto angle=i*3.14159265358979323846f/6;f.hands[1].x=std::cos(angle);f.hands[1].y=std::sin(angle);send();f.hands[1].joystick=true;send();f.hands[1].joystick=false;f.hands[1].x=f.hands[1].y=0;send();std::cout<<"Controller sector "<<i<<" page "<<d.frame().state->menu_page<<" index "<<d.frame().state->menu_index<<'\n';}
};
}
int main()try{
 static_assert(spatial_max_page==spatial_page_count-1);
 check(!spatial_action(-1,0)&&!spatial_action(spatial_page_count,0)&&!spatial_action(1000,4),"The shared spatial page registry rejects invalid pages without aliasing root actions");
 {
  Document d;d.replace(planes());Controller c(d);c.send();c.f.hands[1].joystick=true;c.send();c.f.hands[1].joystick=false;c.send();c.sector(8);c.sector(9);c.sector(9);
  check(d.frame().state->menu_page==20&&spatial_action(20,0)==SpatialCommand::ObjectGrabMode,"Actual joystick input reaches Holding through Objects and Mesh tools");
  c.sector(0);check(d.frame().state->object_grab_mode&&d.frame().state->menu_page==20,"Joystick enables explicit object mode while retaining its Holding page");
  c.sector(1);c.sector(3);auto s=d.frame().state;check(s->held_object&&s->isolate_held&&s->menu_page==20&&std::count_if(s->objects.begin(),s->objects.end(),[](const auto&o){return o.ghost&&!o.editable;})==1,"Controller Hold and Ghost controls establish the same sole-target authority as the desktop");
  c.sector(2);check(!d.frame().state->held_object&&d.frame().state->menu_page==20,"Controller Release clears isolation without losing the Holding page");
  c.sector(8);check(d.frame().state->menu_page==10,"Holding Back returns to Mesh tools with existing movement actions preserved");
 }
 {
  Document d;d.replace(planes());d.configure_object_grab(true,true);check(d.wait_mesh(),"Paint fixture prepares its immutable picking data");PaintSettings paint;paint.color={.1f,.9f,.2f,1};paint.front_only=false;d.paint_settings(paint);auto a=d.copy_scene().active().id,b=d.copy_scene().objects().back().id;auto before=d.copy_scene();auto original=before.object(a).painted->encode();auto source=before.object(b).painted->encode();Controller c(d);c.grab();c.stroke({0,.2f,0});auto edited=d.copy_scene();
  check(edited.object(a).painted->encode()!=original&&edited.object(b).painted->encode()==source&&edited.object(a).transform.position.x==0&&d.frame().state->object_grabbing,"The other hand paints the physically held mesh while its pose remains a GPU-only preview");
  c.release();check(d.copy_scene().object(a).transform.position.x>.19f&&!d.frame().state->held_object,"Releasing physical hold accepts pose after the completed paint gesture");check(d.undo()&&d.copy_scene().object(a).painted->encode()==edited.object(a).painted->encode(),"Transform undo retains the separately accepted other-hand paint");
 }
 {
  auto s=planes();auto a=s.active().id,b=s.objects().back().id;s.object(a).mesh_active=false;s.object(a).painted.reset();auto volume=std::make_shared<Volume>(.05f);volume->begin_stroke();Brush seed;seed.radius=.25f;volume->stamp(seed);volume->end_stroke();s.object(a).volume=volume;
  Document d;d.replace(s);d.configure(Tool::Carve,.12f);d.configure_object_grab(true,true);d.wait_mesh();auto original=d.copy_scene().object(a).volume->encode(),source=d.copy_scene().object(b).painted->encode();Controller c(d);c.grab();c.stroke({0,0,.22f});auto edited=d.copy_scene();check(edited.object(a).volume->encode()!=original&&edited.object(b).painted->encode()==source&&d.frame().state->object_grabbing,"The other hand sculpts the physically held volume without touching a ghost mesh");c.release();
 }
 {
  Document d;d.replace(planes(true));auto a=d.copy_scene().active().id,b=d.copy_scene().objects().back().id;WeightToolSettings weights;weights.enabled=true;weights.linked=true;weights.mode=WeightBrushMode::Replace;weights.value=1;weights.strength=1;d.weight_settings(weights,1);d.configure_object_grab(true,true);check(d.wait_mesh(),"Weight fixture prepares the actual skin overlay and pick view");auto original=d.copy_scene().object(b).deformation->encode(d.copy_scene().object(b).painted->mesh());Controller c(d);c.grab();c.stroke({0,.35f,0});check(d.wait_asset(),"Other-hand weight acceptance drains its real guarded worker");auto edited=d.copy_scene();bool changed=false;for(const auto&v:edited.object(a).deformation->skin->vertices)for(auto w:v)changed|=w.joint==1&&w.weight>0;
  check(changed&&edited.object(b).deformation->encode(edited.object(b).painted->mesh())==original&&d.frame().state->object_grabbing,"Actual other-hand weights update only the physically held mesh despite stored linked-parts setting");check(d.frame().state->weights.linked,"Isolation does not overwrite the user's ordinary linked weight setting");c.release();check(d.copy_scene().object(a).transform.position.x>.19f&&!d.frame().state->held_object,"Physical grip release preserves accepted skin changes and commits the pose");
 }
 {
  Document d;d.replace(planes());d.configure_object_grab(true,true);d.wait_mesh();Controller c(d);c.grab();auto a=d.copy_scene().active().id;std::promise<void> started,release;auto future=started.get_future();DocumentTransactionTestAccess::delayed_weight(d,started,release.get_future().share());check(future.wait_for(std::chrono::seconds(2))==std::future_status::ready,"Delayed tagged worker reaches the actual accepted-work queue");c.release();check(d.frame().state->object_grabbing&&d.copy_scene().object(a).transform.position.x==0,"Grip release waits for accepted held edits instead of invalidating their authority");release.set_value();check(d.wait_asset(),"Deferred release drains its accepted edit");auto finished=d.copy_scene();check(finished.object(a).material.color[0]==.25f&&finished.object(a).transform.position.x>.19f&&!d.frame().state->held_object,"Accepted held edit and deferred transform release both publish without loss");
 }
 {
  Document d;d.replace(planes());d.configure_object_grab(true,true);d.wait_mesh();Controller c(d);c.grab();auto source=d.copy_scene().encode();c.f.focused=false;c.send();
  check(!d.frame().state->object_grabbing&&!d.frame().state->held_object&&d.copy_scene().encode()==source,"Focus loss cancels an unaccepted physical pose and restores exact source authority");
 }
 {
  Document d;d.replace(planes());d.configure_object_grab(true,true);d.wait_mesh();Controller c(d);c.grab();auto source=d.copy_scene().encode();c.f.hands[1].tracked.squeeze=1;c.send();c.f.hands[1].tracked.grip.position.x+=.5f;c.send();auto state=d.frame().state;auto held=std::find_if(state->objects.begin(),state->objects.end(),[&](const auto&o){return o.id==state->held_object;});
  check(held!=state->objects.end()&&held->world_transform.scale>1.01f&&d.copy_scene().encode()==source,"Two physical grips scale the held mesh through published GPU pose authority without changing source geometry");
  c.f.hands[1].tracked.squeeze=0;c.send();c.release();check(d.copy_scene().active().transform.scale>1.01f&&!d.frame().state->held_object,"Releasing the final grip commits the two-hand object transform once");
 }
 std::cout<<"Physical held edit checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
