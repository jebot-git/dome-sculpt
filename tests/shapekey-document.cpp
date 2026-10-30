// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_shapekey_kernel_test
#include "shapekey-authoring.cpp"
#undef main
#include "dome/document.hpp"
#include <atomic>
#include <thread>
namespace gate {
std::atomic<bool> armed=false,entered=false,released=false;
void arm(){entered=false;released=false;armed=true;}
void release(){released=true;released.notify_all();}
struct Release{~Release(){release();}};
void wait(){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(20);while(!entered&&std::chrono::steady_clock::now()<end)std::this_thread::yield();if(!entered)throw std::runtime_error("Shapekey worker barrier timeout");}
}
extern "C" ShapeKeyResult __real__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE(Scene&,uint64_t,const ShapeKeyEdit&,const std::function<bool()>&);
extern "C" ShapeKeyResult __wrap__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE(Scene&s,uint64_t id,const ShapeKeyEdit&e,const std::function<bool()>&cancelled){
 if(gate::armed.exchange(false)){gate::entered=true;while(!gate::released)gate::released.wait(false);}
 return __real__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE(s,id,e,cancelled);
}
namespace {
void ready(Document &doc){check(doc.wait_asset()&&doc.wait_mesh(),"Shapekey worker and immutable mesh publication finish");if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);}
struct Hand{
 Document&doc;SpatialFrame f;
 explicit Hand(Document &d):doc(d){f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[1].tracked.valid=true;send();}
 void send(float x=0,float y=0,bool click=false,float pressure=0){++f.sequence;f.time+=.05;f.hands[1].x=x;f.hands[1].y=y;f.hands[1].joystick=click;f.hands[1].tracked.trigger=pressure;doc.submit(f);if(!doc.wait_input())throw std::runtime_error("Shape controller input timeout");}
 void sector(int i){float a=i*3.14159265358979323846f/6;send(std::cos(a),std::sin(a));check(doc.frame().state->menu_index==i,"Observed joystick deflection selects the intended sector before activation");send(std::cos(a),std::sin(a),true);send();}
 void open(){send();send(0,0,true);send();sector(8);sector(9);sector(7);}
 void point(float x,float y){auto s=doc.frame().state;auto &h=f.hands[1].tracked;h.aimValid=true;h.aim.position=ob::transform(s->panel,{(x/640-.5f)*.6f,(.5f-y/640)*.6f,.25f});h.aim.orientation=s->panel.orientation;send(0,0,false,.8f);send();}
};
}
int main(int argc,char**argv)try{
 if(argc!=2)return 2;auto prefix=std::filesystem::path(argv[1]);Document doc;gate::Release release;doc.replace(fixture());ready(doc);auto original=doc.copy_scene().encode();
 check(doc.frame().state->can_author_shapes&&!doc.frame().state->can_select_shape&&!doc.frame().state->can_restore_shapes,"Native shape capabilities distinguish new targets and retained source restoration");
 ShapeKeyEdit edit;edit.name="Smile";doc.author_shape(edit);ready(doc);auto accepted=doc.copy_scene().encode();
 check(doc.frame().state->can_select_shape&&doc.frame().state->can_restore_shapes&&doc.frame().state->morph_target==0&&doc.frame().state->can_undo,"Actual queued authoring publishes one selected target and source-aware controls");
 check(doc.undo()&&doc.copy_scene().encode()==original&&doc.redo()&&doc.copy_scene().encode()==accepted,"One native Undo/Redo preserves the exact complete shapekey endpoint");ready(doc);
 gate::arm();edit.name="Cancelled";doc.author_shape(edit);gate::wait();check(doc.frame().state->can_cancel_asset&&!doc.frame().state->can_author_shapes,"The real file-worker operation exposes cancellation and disables conflicting authoring");doc.cancel_background_work();gate::release();ready(doc);
 check(doc.copy_scene().encode()==accepted&&doc.frame().state->can_author_shapes,"Actual worker cancellation preserves exact shape, source and history for retry");
 Hand hand(doc);hand.open();check(doc.frame().state->menu_page==24&&doc.frame().state->menu_enabled[0]&&doc.frame().state->menu_enabled[3],"Actual joystick Mesh/Shapekeys navigation reaches the shared native target tools");
 hand.sector(0);ready(doc);check(doc.copy_scene().active().deformation->morphs->targets.size()==2&&doc.frame().state->morph_target==1&&doc.frame().state->menu_page==24,"Controller New creates and selects a real target while retaining the tool page");
 hand.sector(2);ready(doc);check(doc.copy_scene().active().deformation->morphs->targets.size()==3&&doc.frame().state->morph_target==2,"Controller Duplicate authors another independent normal/frame target");
 hand.sector(3);ready(doc);check(doc.copy_scene().active().deformation->morphs->targets.size()==4,"Controller Mirror uses the actual bounded topology mirror kernel");
 hand.sector(7);check(doc.frame().state->controls.parameter==VrParameter::ShapeRestore&&doc.frame().state->controls.value[0]==0,"Controller Restore begins with Cancel selected instead of executing source restoration");doc.cancel_spatial_menu();check(doc.copy_scene().active().deformation->morphs->targets.size()==4,"Cancelling source choice retains every accepted target");
 hand.open();hand.sector(7);hand.point(520,300);check(doc.frame().state->controls.value[0]>=.5f,"Real panel ray selects Restore before confirmation");hand.point(470,609);ready(doc);
 check(doc.copy_scene().encode()==original&&doc.undo(),"Actual controller confirmation restores complete original source in one undoable operation");ready(doc);auto authored=doc.copy_scene().encode();
 hand.open();gate::arm();hand.sector(0);gate::wait();doc.cancel_spatial_menu();gate::release();ready(doc);
 check(!doc.frame().state->menu&&doc.copy_scene().active().deformation->morphs->targets.size()==5,"Accepted shape creation cannot reopen a menu closed while its worker was busy");
 check(doc.undo()&&doc.copy_scene().encode()==authored,"One Undo after a closed-menu operation restores exact authored source and keys");ready(doc);
 auto path=prefix;path+=".dsculpt";doc.save(path);Document reopened;reopened.open(path);ready(reopened);check(reopened.copy_scene().encode()==doc.copy_scene().encode(),"New Document reopens all controller-authored keys and their durable original source");
 edit={};edit.action=ShapeKeyAction::Restore;reopened.author_shape(edit);ready(reopened);check(reopened.copy_scene().encode()==original&&reopened.undo(),"Native Restore and Undo recover exact original and authored states after reopen");
 auto protected_scene=doc.copy_scene();auto held=protected_scene.active().id;auto other=protected_scene.duplicate(held);doc.replace(std::move(protected_scene));doc.configure_object_grab(true,true);doc.hold_object(held);doc.select_object(other);auto before=doc.copy_scene().encode();
 check(refuses([&]{doc.author_shape({});})&&doc.copy_scene().encode()==before,"A readonly ghost cannot receive queued shape or source edits");
 std::cout<<"PASS "<<checks<<" native/controller shapekey checks\n";return 0;
}catch(const std::exception&e){gate::release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
