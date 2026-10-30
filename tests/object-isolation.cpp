// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <future>
using namespace dome;
namespace {
int checks=0;
void check(bool ok,const char*text){if(!ok)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
Mesh triangle(){Mesh m;m.positions={{-.4f,-.4f,0},{.4f,-.4f,0},{0,.4f,0}};m.normals.assign(3,{0,0,1});m.uv={{0,0},{1,0},{.5f,1}};m.indices={0,1,2};return m;}
Scene scene(){Scene s;auto a=s.active().id;s.active().painted=std::make_shared<PaintedMesh>(triangle(),64);s.active().mesh_active=true;auto b=s.add("Other mesh",Volume{});s.object(b).painted=std::make_shared<PaintedMesh>(triangle(),64);s.object(b).mesh_active=true;s.object(b).transform.position.x=.6f;s.select(a);return s;}
const ObjectInfo&info(const EditorState&s,uint64_t id){return *std::find_if(s.objects.begin(),s.objects.end(),[&](const auto&o){return o.id==id;});}
}
int main()try{
 Document d;d.replace(scene());check(d.wait_mesh(),"Initial immutable mesh is ready");auto base=d.copy_scene();auto a=base.active().id,b=base.objects().back().id;auto exact=base.encode();auto mesh=d.frame().surface;
 check(d.object_editable(a)&&d.object_editable(b)&&!d.frame().state->object_grab_mode,"Ordinary editing and workspace-grip mode remain the default");
 d.configure_object_grab(true,true);d.hold_object(a);auto state=d.frame().state;
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
 std::cout<<"Object isolation checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
