// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/rig-edit.hpp"
#include "dome/creator-archive.hpp"
#include "native-rig-pose-scope.hpp"
#include <atomic>
#include <iostream>
#include <numbers>
#include <thread>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char*label){if(!value)throw std::runtime_error(label);++checks;std::cout<<"PASS "<<label<<std::endl;}
template<class F>bool rejects(F&&f){try{f();return false;}catch(const std::exception&e){std::cout<<"expected refusal: "<<e.what()<<std::endl;return true;}}
struct Controller {
 Document&doc;SpatialFrame f;
 explicit Controller(Document&d):doc(d){f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[1].tracked.valid=true;send();}
 void send(float x=0,float y=0,bool click=false){++f.sequence;f.time+=.05;f.hands[1].x=x;f.hands[1].y=y;f.hands[1].joystick=click;doc.submit(f);if(!doc.wait_input())throw std::runtime_error("Rig controller input timeout");}
 void sector(int n){float a=n*std::numbers::pi_v<float>/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
 void rig(){send();send(0,0,true);send();sector(8);sector(10);sector(5);}
};
struct Gate {std::atomic_bool entered=false,release=false;~Gate(){release=true;}};
void wait_gate(Gate&g){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(120);while(!g.entered&&std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(1));check(g.entered,"Actual ordered pose worker reached detached preparation boundary");}
}
int main(int argc,char**argv)try{
 if(argc!=3)throw std::invalid_argument("complete genuinely bound907 scene, output directory");
 const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);
 setenv("XDG_STATE_HOME",(out/"state").c_str(),1);setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
 auto source=Scene::load(argv[1]);std::vector<uint64_t>ids;
 for(const auto&o:source.objects())if(o.mesh_active&&o.painted&&o.deformation&&o.deformation->skin&&o.appearance)ids.push_back(o.id);
 check(ids.size()==19,"Actual complete nineteen-part source-assisted anatomical field");
 const auto id=ids.front();source.select(id);const auto head=source.object(id).deformation->humanoid->joints.at("head");
 RigEdit edit;edit.joint=JointAdjustment{head,{0,0,30},{}};
 const auto baseline=source.encode();auto expected=source;
 auto unlinked=edit;unlinked.linked_pose=false;
 // Independent scope oracle: explicitly edit each recipient with the existing
 // single-object operation. It does not call the new membership path.
 for(auto object:ids)edit_rig(expected,object,unlinked);
 auto actual=source;edit_rig(actual,id,edit);
 size_t changed=0;for(auto object:ids)changed+=actual.object(object).deformation!=source.object(object).deformation;
 check(changed==19,"Actual linked operation changes all nineteen sibling-group native rigs");
 check(actual.encode()==expected.encode(),"Linked output equals explicit per-object scope oracle for the entire contextual Scene");
 double max_error=0;size_t moved=0,corners=0;
 for(auto object:ids){const auto&a=actual.object(object);const auto&b=source.object(object);const auto&c=expected.object(object);
  check(a.painted==b.painted&&a.volume==b.volume&&a.appearance==b.appearance&&a.material_bindings==b.material_bindings&&a.rig_source==b.rig_source&&a.shape_source==b.shape_source&&a.bake_recipe==b.bake_recipe,"Linked pose preserves exact paint/source/volume/origin/recipe owners");
  auto p=a.deformation->preview(a.painted->mesh());auto q=c.deformation->preview(c.painted->mesh());auto rest=b.deformation->preview(b.painted->mesh());
  for(auto v:p.indices){++corners;max_error=std::max(max_error,double(length(p.positions[v]-q.positions[v])));moved+=length(p.positions[v]-rest.positions[v])>0;}
 }
 check(max_error==0&&moved>0,"All evaluated corners match the independent per-object pose and genuinely move");
 std::cout<<"corners="<<corners<<" moved="<<moved<<" maxerror="<<max_error<<std::endl;
 check(source.encode()==baseline,"Detached linked edit leaves original complete source exact");
 auto refuse=[&](Scene s,const char*label){auto before=s.encode();bool no=rejects([&]{edit_rig(s,id,edit);});check(no&&s.encode()==before,label);};
 {auto s=source;s.object(ids.back()).locked=true;refuse(std::move(s),"Locked sibling outside selected group rejects atomically");}
 {auto s=source;s.object(ids.back()).visible=false;// hidden followers remain eligible through the selected operation
  edit_rig(s,id,edit);check(s.object(ids.back()).deformation->pose.size()>head&&s.object(ids.back()).deformation->skeleton->local_matrices(s.object(ids.back()).deformation->pose)==actual.object(ids.back()).deformation->skeleton->local_matrices(actual.object(ids.back()).deformation->pose),"Hidden authenticated sibling follows the linked pose");}
 {auto s=source;auto d=std::make_shared<DeformationState>(*s.object(ids.back()).deformation);d->skeleton->bones[head].length+=.001f;s.object(ids.back()).deformation=d;refuse(std::move(s),"Different native rest authority refuses rather than silently omitting the sibling");}
 {auto s=source;auto d=std::make_shared<DeformationState>(*s.object(ids.back()).deformation);std::swap(d->humanoid->joints.at("leftEye"),d->humanoid->joints.at("rightEye"));s.object(ids.back()).deformation=d;refuse(std::move(s),"Different authored role identity refuses atomically");}
 {auto s=source;edit_rig(s,ids.back(),unlinked);refuse(s,"Conflicting current local pose refuses atomically");RigEdit reset;reset.reset_pose=true;edit_rig(s,id,reset);check(s.encode()==baseline,"Explicit linked reset resolves divergent pose without changing original sources");}
 {auto s=source;auto a=std::make_shared<CreatorAppearance>(*s.object(ids.back()).appearance);a->source_node=-1;s.object(ids.back()).appearance=a;bool no=rejects([&]{edit_rig(s,id,edit);});bool unchanged=s.object(ids.back()).appearance==a&&a->source_node==-1;s.object(ids.back()).appearance=source.object(ids.back()).appearance;check(no&&unchanged&&s.encode()==baseline,"Absent actual source-node identity refuses before publication without changing any recipient");}
 {auto s=source;s.erase(ids.back());refuse(std::move(s),"Missing local primitive cannot borrow another scope");}
 {auto s=source;s.duplicate(ids.back());refuse(std::move(s),"Ambiguous duplicate primitive refuses linked scope");}
 {auto s=source;size_t calls=0;check(rejects([&]{edit_rig(s,id,edit,[&]{return ++calls==64;});})&&calls==64&&s.encode()==baseline,"Real bounded preparation cancellation64 leaves complete Scene untouched");}
 {auto s=source;edit_rig(s,id,unlinked);size_t n=0;for(auto object:ids)n+=s.object(object).deformation!=source.object(object).deformation;check(n==1,"Explicit unlinked operation keeps every other native rig unchanged");}
 // Two genuine current instances under a later user collection. Exact same
 // source owners, transforms and rig are deliberately insufficient membership.
 {auto s=source;const auto groups=s.groups();std::set<uint64_t>needed;for(auto object:ids)for(const auto&g:s.group_path(s.object(object).group))needed.insert(g.id);
  auto collection=s.add_group("User collection");for(const auto&g:groups)if(!g.parent)s.group(g.id).parent=collection;
  std::map<uint64_t,uint64_t>map;for(const auto&g:groups)if(needed.contains(g.id)){auto next=s.add_group(g.name,g.transform,g.parent?map.at(g.parent):collection,g.humanoid);map[g.id]=next;s.group(next).source_placement=g.source_placement;s.group(next).affine_edit=g.affine_edit;}
  std::vector<uint64_t>foreign;for(auto object:ids){auto copy=s.object(object);auto next=s.add(copy.name,Volume{});copy.id=next;copy.group=map.at(copy.group);s.object(next)=std::move(copy);foreign.push_back(next);}
  auto before=s;auto e=s;for(auto object:ids)edit_rig(e,object,unlinked);edit_rig(s,id,edit);check(s.encode()==e.encode(),"Deepest complete current instance prevents merging identical second-avatar rigs");
  bool exact=true;for(auto object:foreign)exact&=s.object(object).deformation==before.object(object).deformation&&s.object(object).painted==before.object(object).painted;check(exact,"Foreign-instance mutable recipients and immutable rig owners remain exact");
 }
 {
  Document doc;doc.replace(source);check(doc.wait_mesh(std::chrono::seconds(120)),"Fresh current native immutable mesh ready");
  auto before=doc.copy_scene();auto before_value=before.encode();auto accepted=before;edit_rig(accepted,id,edit);
  auto gate=std::make_shared<Gate>();
  auto queue=[&]{doc.edit_scene("Pose complete current native rig",[=](Scene&s,const auto&cancel){gate->entered=true;while(!gate->release){if(cancel&&cancel())throw std::runtime_error("Pose test cancelled");std::this_thread::sleep_for(std::chrono::milliseconds(1));}edit_rig(s,id,edit,cancel);});};
  queue();wait_gate(*gate);auto save=doc.save_async(out/"cancelled.dsculpt");doc.cancel_background_work();gate->release=true;check(rejects([&]{save.get();})&&doc.wait_asset(std::chrono::seconds(120))&&doc.copy_scene().encode()==before_value&&!doc.frame().state->can_undo,"Actual worker Cancel fails dependent Save and preserves Scene/history");
  gate=std::make_shared<Gate>();queue();wait_gate(*gate);auto stale=doc.save_async(out/"stale.dsculpt");doc.replace(before);gate->release=true;check(rejects([&]{stale.get();})&&doc.wait_asset(std::chrono::seconds(120))&&doc.copy_scene().encode()==before_value&&!doc.frame().state->can_undo,"New-generation replacement refuses stale pose and its Save");
  gate=std::make_shared<Gate>();queue();wait_gate(*gate);auto saved=doc.save_async(out/"linked.dsculpt");gate->release=true;saved.get();check(doc.wait_asset(std::chrono::seconds(120))&&doc.frame().state->error.empty()&&doc.copy_scene().encode()==accepted.encode()&&Scene::load(out/"linked.dsculpt").encode()==accepted.encode(),"Real native worker and immediate ordered Save publish complete nineteen-part pose");
  check(doc.undo()&&doc.copy_scene().encode()==before_value&&!doc.frame().state->can_undo&&doc.redo()&&doc.copy_scene().encode()==accepted.encode(),"Whole linked operation has one exact Undo and Redo");
  doc.undo();check(doc.wait_mesh(std::chrono::seconds(120)),"Current mesh fence ready for actual controller pose");Controller controller(doc);controller.rig();check(doc.frame().state->menu_page==11,"Real joystick reaches native rig controls");
  for(size_t n=0;doc.frame().state->rig_joint!=head&&n<256;++n){if(!doc.frame().state->menu)controller.rig();controller.sector(1);std::cout<<"native next-joint="<<doc.frame().state->rig_joint<<" expected="<<head<<std::endl;}
  check(doc.frame().state->rig_joint==head,"Actual next-joint controls select authenticated Head slot");
  if(!doc.frame().state->menu)controller.rig();controller.sector(0);controller.send(1,0);controller.send(1,0);auto controls=doc.frame().state->controls;check(controls.parameter==VrParameter::RigRotation&&controls.value[0]>0,"Actual controller stages bounded local rotation");
  RigEdit native;native.joint=JointAdjustment{head,{controls.value[0],controls.value[1],controls.value[2]}, {}};auto native_expected=before;for(auto object:ids){auto one=native;one.linked_pose=false;edit_rig(native_expected,object,one);}
  controller.send(0,0,true);controller.send();auto f=doc.save_async(out/"controller.dsculpt");f.get();check(doc.wait_asset(std::chrono::seconds(120))&&doc.frame().state->error.empty()&&doc.copy_scene().encode()==native_expected.encode()&&Scene::load(out/"controller.dsculpt").encode()==native_expected.encode(),"Actual controller Confirm and ordered Save affect every authenticated native part");
  check(doc.undo()&&doc.copy_scene().encode()==before_value&&!doc.frame().state->can_undo,"Controller linked pose owns one exact Undo");
 }
 std::cout<<"Native current rig scope checks "<<checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
