// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <future>
#include <cstdlib>
#include <new>
thread_local int64_t allocation_budget=-1;
void* operator new(std::size_t n){if(allocation_budget==0)throw std::bad_alloc();if(allocation_budget>0)--allocation_budget;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
using namespace dome;
namespace {
int checks=0;
void check(bool ok,const char*text){if(!ok)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
Mesh triangle(){Mesh m;m.positions={{-.4f,-.4f,0},{.4f,-.4f,0},{0,.4f,0}};m.normals.assign(3,{0,0,1});m.uv={{0,0},{1,0},{.5f,1}};m.indices={0,1,2};return m;}
Scene scene(){Scene s;auto a=s.active().id;s.active().painted=std::make_shared<PaintedMesh>(triangle(),64);s.active().mesh_active=true;auto b=s.add("Other mesh",Volume{});s.object(b).painted=std::make_shared<PaintedMesh>(triangle(),64);s.object(b).mesh_active=true;s.object(b).transform.position.x=.6f;s.select(a);return s;}
const ObjectInfo&info(const EditorState&s,uint64_t id){return *std::find_if(s.objects.begin(),s.objects.end(),[&](const auto&o){return o.id==id;});}
}
int main(int argc,char**argv)try{
 if(argc==3&&std::string(argv[1])=="--ui-fixture"){auto s=scene();s.active().material.color={.8f,.1f,.05f,1};s.active().material.unlit=true;s.object(s.objects().back().id).material.color={.05f,.3f,.9f,1};s.object(s.objects().back().id).material.unlit=true;s.save(argv[2]);return 0;}
 if(argc==3&&std::string(argv[1])=="--inspect"){auto s=Scene::load(argv[2]);auto out=nlohmann::json::array();for(const auto&o:s.objects())out.push_back({{"id",o.id},{"x",o.transform.position.x},{"visible",o.visible},{"locked",o.locked},{"color",o.material.color}});std::cout<<out.dump()<<'\n';return 0;}

 Document d;d.replace(scene());check(d.wait_mesh(),"Initial immutable mesh is ready");auto base=d.copy_scene();auto a=base.active().id,b=base.objects().back().id;auto exact=base.encode();auto mesh=d.frame().surface;
 check(!d.frame().surface->picking,"Ordinary scenes do not allocate an unused object-picking BVH");
 check(d.object_editable(a)&&d.object_editable(b)&&!d.frame().state->object_grab_mode,"Ordinary editing and workspace-grip mode remain the default");
 d.configure_object_grab(true,true);check(d.wait_mesh()&&d.pick_object({0,0,2.2f},{0,0,-1})==a,"Explicit object mode prepares material-aware object picking on the worker");mesh=d.frame().surface;d.hold_object(a);auto state=d.frame().state;
 check(d.copy_scene().encode()==exact && d.frame().surface==mesh,"Holding and ghost isolation preserve exact native scene and immutable geometry");
 check(info(*state,a).editable&&!info(*state,a).ghost&&!info(*state,b).editable&&info(*state,b).ghost,"Published authority identifies the sole editable held mesh and read-only ghosts");
 d.select_object(b);check(!d.begin_desktop()&&!d.begin_native_stroke(),"Ghost targets cannot begin synchronous or asynchronous desktop strokes");
 check(rejects([&]{d.prepare_mesh(64);}),"Ghost target rejects geometry preparation");
 auto ghost=d.copy_scene().object(b);auto moved=ghost.transform;moved.position.y=.1f;
 check(rejects([&]{d.object_properties(b,ghost.name,true,false,moved,ghost.material);}),"Ghost transforms cannot bypass central target authority");
 auto source_before=d.copy_scene().encode();d.use_source();check(d.copy_scene().encode()==source_before,"Direct source-mode command cannot mutate a ghost mesh");
 d.object_visibility(b,false);check(!d.copy_scene().object(b).visible&&!info(*d.frame().state,b).ghost,"Individual visibility remains usable independently of isolation");d.object_visibility(b,true);
 d.select_object(a);auto lease=d.draft_scene().source;d.configure_object_grab(true,false);d.configure_object_grab(true,true);
 check(rejects([&]{d.edit_scene("Stale editor",[](Scene&s,const auto&){s.active().name="stale";},lease);}),"Lease rejects an editor from an earlier authority epoch even after toggling isolation back");
 auto protected_scene=d.copy_scene().encode();d.edit_scene("Invalid ghost mutation",[b](Scene&s,const auto&){s.object(b).material.color[0]=.1f;});
 check(d.wait_asset()&&d.copy_scene().encode()==protected_scene&&!d.frame().state->error.empty(),"Accepted asset jobs cannot publish edits to read-only ghost objects");
 d.edit_scene("Read source",[b](Scene&s,const auto&){s.active().material.color[0]=s.object(b).material.color[1];});
 check(d.wait_asset()&&d.frame().state->error.empty()&&d.copy_scene().object(b).material.color==base.object(b).material.color,"A held target may consume immutable ghost source data without editing the source");
 auto before_work=d.copy_scene().encode();std::promise<void> started;auto future=started.get_future();d.edit_scene("Cancelled ownership work",[&started](Scene&s,const auto&cancelled){started.set_value();while(!cancelled())std::this_thread::yield();s.active().name="must not publish";});
 check(future.wait_for(std::chrono::seconds(2))==std::future_status::ready,"Authority cancellation fixture reaches the actual ordered worker");d.release_held_object();check(d.wait_asset()&&d.copy_scene().encode()==before_work,"Releasing ownership cancels stale accepted work without a partial scene publication");
 d.hold_object(a);d.configure_object_grab(true,true);check(d.wait_mesh(),"Held target scene finishes prior material work");auto before_motion=d.copy_scene().encode();auto geometry=d.frame().surface;
 check(d.begin_object_grab(a),"Explicit object mode begins a held transform gesture");auto local=d.copy_scene().object(a).transform;local.position.x=.2f;d.move_held_object(local);
 check(d.copy_scene().encode()==before_motion && d.frame().surface==geometry,"Object motion is an ephemeral GPU transform instead of CPU geometry regeneration");
 check(info(*d.frame().state,a).world_transform.position.x==.2f && d.frame().state->object_grabbing,"Published held preview has exact current world authority");d.end_object_grab(false);
 check(d.copy_scene().encode()==before_motion&&!d.frame().state->held_object,"Cancelled held motion restores the exact source and releases ghost isolation");
 d.begin_object_grab(a);d.move_held_object(local);d.end_object_grab(true);check(d.copy_scene().object(a).transform.position.x==.2f,"Releasing accepted motion commits the authored transform once");
 check(d.undo()&&d.copy_scene().encode()==before_motion&&d.redo()&&d.copy_scene().object(a).transform.position.x==.2f,"Held motion has exact global undo/redo without changing mesh or material data");
 d.hold_object(a);d.replace(scene());check(!d.frame().state->held_object&&d.object_editable(b),"Document replacement clears obsolete held identities and readonly overrides");
 // Controller path: squeeze defaults to workspace navigation, explicit mode moves only the object.
 auto controller=[&](bool object_mode){Document c;c.replace(scene());c.wait_mesh();c.configure_object_grab(object_mode,true);SpatialFrame f;f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[0].tracked.valid=true;f.hands[0].tracked.grip.position={0,1.4f,-.6f};auto send=[&]{++f.sequence;f.time+=.05;c.submit(f);if(!c.wait_input())throw std::runtime_error("Controller timeout");};send();auto original_canvas=c.frame().state->canvas;f.hands[0].tracked.squeeze=1;send();auto native=c.copy_scene().encode();f.hands[0].tracked.grip.position.x+=.2f;send();if(object_mode){check(c.copy_scene().encode()==native&&c.frame().state->held_object==a,"Actual controller object grip previews only the selected mesh");check(c.frame().state->canvas.position.x==original_canvas.position.x,"Object grip preserves the workspace transform");}else check(c.frame().state->canvas.position.x!=original_canvas.position.x&&!c.frame().state->held_object,"Default controller squeeze still moves the workspace");f.hands[0].tracked.squeeze=0;send();if(object_mode)check(c.copy_scene().object(a).transform.position.x>.19f&&!c.frame().state->held_object,"Controller grip release commits one object transform and restores ordinary presentation");};
 controller(false);controller(true);
 auto bake_scene=scene();bake_scene.object(b).transform.position={0,0,.005f};Document baker;baker.replace(bake_scene);baker.configure_object_grab(true,true);baker.hold_object(a);const auto before_source=baker.copy_scene().object(b).painted->encode();ObjectBakeSettings options;options.ao_samples=0;options.padding=0;
 auto stationary=baker.copy_scene().encode();baker.begin_object_grab(a);auto moving=baker.copy_scene().object(a).transform;moving.position.x=.2f;baker.move_held_object(moving);
 check(!baker.frame().state->can_bake&&!baker.frame().state->can_clear_bake&&rejects([&]{baker.bake_from_object(b,options);})&&baker.copy_scene().encode()==stationary,"Live held motion visibly disables baking and rejects world-space work without capturing the old committed pose");
 baker.end_object_grab(false);baker.hold_object(a);
 check(baker.frame().state->can_bake&&std::find(baker.frame().state->bake_sources.begin(),baker.frame().state->bake_sources.end(),b)!=baker.frame().state->bake_sources.end(),"Read-only ghosts remain actual eligible bake sources for the held target");
 baker.bake_from_object(b,options);check(baker.wait_asset()&&baker.frame().state->error.empty()&&baker.copy_scene().object(a).painted->baked()&&baker.copy_scene().object(b).painted->encode()==before_source,"Actual native bake consumes a ghost while preserving its exact source paint and geometry");
 size_t failed=0;
 for(int phase=0;phase<3;++phase)for(int fail=0;fail<10;++fail){Document fault;fault.replace(scene());fault.wait_mesh();fault.configure_object_grab(true,true);fault.hold_object(a);auto old=fault.frame().state;auto original=fault.copy_scene().encode();bool threw=false;allocation_budget=fail;
  try{if(phase==0)fault.configure_object_grab(true,false);else if(phase==1)fault.hold_object(b);else fault.release_held_object();}catch(const std::bad_alloc&){threw=true;}allocation_budget=-1;
  if(threw){++failed;check(fault.frame().state==old&&fault.copy_scene().encode()==original&&fault.object_editable(a)&&!fault.object_editable(b),"Injected transition-publication allocation failure restores exact permission binding, selection and committed scene");}
 }
 check(failed>=6,"Allocation-failure fixture reaches real transition publications rather than only admission rejection");
 std::cout<<"Object isolation checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
