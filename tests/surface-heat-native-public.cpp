// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-surface-heat.hpp"
#include "dome/skin-authoring.hpp"
#include "dome/document.hpp"
#include "dome/rig-rest.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <numbers>
using namespace dome;using namespace dome::detail;
namespace {
size_t checks=0;void check(bool v,const char*s){if(!v)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<std::endl;}
bool files_equal(const std::filesystem::path&a,const std::filesystem::path&b){std::ifstream x(a,std::ios::binary),y(b,std::ios::binary);if(!x||!y)return false;std::array<char,65536>p{},q{};do{x.read(p.data(),p.size());y.read(q.data(),q.size());if(x.gcount()!=y.gcount()||!std::equal(p.begin(),p.begin()+x.gcount(),q.begin()))return false;}while(x.gcount());return true;}
struct Gate {std::atomic_bool armed{true},entered{false};std::atomic_size_t calls{0};~Gate(){armed=false;}};
void wait_gate(Gate&g){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(120);while(!g.entered&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));check(g.entered&&g.calls==64,"Actual preparation reached deterministic real callback64 gate");}
struct NativeDriver {
 Document&doc;SpatialFrame frame;
 explicit NativeDriver(Document&d):doc(d){frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};frame.hands[1].tracked.valid=true;send();}
 void send(float x=0,float y=0,bool click=false,float force=0){++frame.sequence;frame.time+=.03;auto&h=frame.hands[1];h.x=x;h.y=y;h.joystick=click;h.tracked.trigger=force;doc.submit(frame);if(!doc.wait_input())throw std::runtime_error("Surface heat controller input timeout");}
 void sector(int n){float a=n*std::numbers::pi_v<float>/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
 void open(){send();send(0,0,true);send();sector(8);sector(10);sector(5);sector(10);sector(11);}
 void value(float t){const auto state=doc.frame().state;auto&h=frame.hands[1];h.tracked.aimValid=true;h.tracked.aim.position=ob::transform(state->panel,{((100+420*t)/640.f-.5f)*.6f,(.5f-300.f/640.f)*.6f,.25f});h.tracked.aim.orientation=state->panel.orientation;send(0,0,false,.8f);send();}
 void confirm(){send(0,0,true);send();}
};

}
using NativeCancel=std::function<bool()>;
std::atomic<std::shared_ptr<Gate>> current_gate;
extern "C" bool real_heat(Scene&,std::span<const uint64_t>,const SkinAuthoringOptions&,const NativeCancel&)
 asm("__real__ZN4dome24author_surface_heat_skinERNS_5SceneESt4spanIKmLm18446744073709551615EERKNS_20SkinAuthoringOptionsERKSt8functionIFbvEE");
extern "C" bool wrapped_heat(Scene&,std::span<const uint64_t>,const SkinAuthoringOptions&,const NativeCancel&)
 asm("__wrap__ZN4dome24author_surface_heat_skinERNS_5SceneESt4spanIKmLm18446744073709551615EERKNS_20SkinAuthoringOptionsERKSt8functionIFbvEE");
extern "C" bool wrapped_heat(Scene&s,std::span<const uint64_t>ids,const SkinAuthoringOptions&o,const NativeCancel&cancel){
 auto gate=current_gate.load();return real_heat(s,ids,o,[&]{if(gate&&gate->armed&&++gate->calls==64){gate->entered=true;while(gate->armed&&!(cancel&&cancel()))std::this_thread::sleep_for(std::chrono::milliseconds(1));}return cancel&&cancel();});
}
int main(int argc,char**argv)try{
 if(argc!=4)throw std::invalid_argument("actual unskinned input sealed heat field output");
 const std::filesystem::path out=argv[3];std::filesystem::create_directories(out);
 setenv("XDG_STATE_HOME",(out/"state").c_str(),1);setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
 auto input=Scene::load(argv[1]);auto full=Scene::load(argv[2]);uint64_t reference=0;std::vector<uint64_t>ids;
 for(auto&o:input.objects())if(o.deformation&&o.deformation->skeleton)reference=o.id;
 for(auto&o:input.objects())if(o.mesh_active&&o.painted)ids.push_back(o.id);
 check(reference&&ids.size()==19,"Actual unskinned current reference and explicit full nineteen-target request");
 Document doc;doc.replace(input);check(doc.frame().state->skin_options.mode==SkinAuthoringMode::Automatic,"Actual new Document preserves the existing Automatic0 default");doc.select_objects(ids);check(doc.wait_mesh(std::chrono::seconds(120)),"Actual viewed native source mesh fence ready");
 auto live=doc.frame().surface;check(live&&!live->ranges.empty(),"Real native frame owns source markers before capture");
 const auto before=doc.copy_scene();
 auto expected=before;
 for(auto id:ids){auto&o=expected.object(id);o.deformation=full.object(id).deformation;o.rig_source=before.object(id).rig_source?before.object(id).rig_source:capture_polygon_origin(before,id);}
 const auto file=out/"accepted.dsculpt",prior=out/"prior.dsculpt";doc.save_async(file).get();std::filesystem::copy_file(file,prior,std::filesystem::copy_options::overwrite_existing);
 SkinAuthoringOptions public_options;public_options.mode=SkinAuthoringMode::SurfaceHeat;public_options.reference=reference;
 public_options.limits={4,1e-5f}; // actual active influence/prune controls are honored by SurfaceHeat5
 auto queue=[&](const std::shared_ptr<Gate>&gate){current_gate=gate;doc.bind_skin_selected(public_options,{},ids);};
 {
  auto gate=std::make_shared<Gate>();queue(gate);wait_gate(*gate);auto save=doc.save_async(file);
  check(doc.frame().state->asset_busy&&files_equal(file,prior),"Accepted dependent Save leaves prior file untouched during preparation");
  doc.cancel_background_work();gate->armed=false;bool cancelled=false;try{save.get();}catch(const std::exception&){cancelled=true;}
  check(cancelled&&doc.wait_asset(std::chrono::seconds(120))&&doc.copy_scene().encode()==before.encode()&&!doc.frame().state->can_undo&&files_equal(file,prior),"Actual mid-helper Cancel fails dependent Save and preserves whole Scene/history/prior file");
 }
 {
  auto gate=std::make_shared<Gate>();queue(gate);wait_gate(*gate);auto save=doc.save_async(file);
  doc.replace(before);gate->armed=false;bool stale=false;try{save.get();}catch(const std::exception&){stale=true;}
  check(stale&&doc.wait_asset(std::chrono::seconds(120))&&doc.copy_scene().encode()==before.encode()&&!doc.frame().state->can_undo&&files_equal(file,prior),"Actual new-generation replacement refuses stale work and accepted Save without overwriting its model/history/file");
  doc.select_objects(ids);check(doc.wait_mesh(std::chrono::seconds(120)),"Replacement native mesh fence ready before same-controller retry");
 }
 {
  auto gate=std::make_shared<Gate>();queue(gate);wait_gate(*gate);auto save=doc.save_async(file);gate->armed=false;save.get();
  check(doc.wait_asset(std::chrono::seconds(120))&&doc.frame().state->error.empty(),"Real native worker publishes prepared surface heat subset");
  check(doc.frame().state->skin_options.mode==SkinAuthoringMode::SurfaceHeat&&doc.frame().state->skin_options.limits.max_influences==4&&doc.frame().state->skin_options.limits.prune==1e-5f,"Typed admission retains explicit method/resources and actual active influence/prune controls");
  check(doc.copy_scene().encode()==expected.encode(),"All explicit nineteen targets receive exact sealed heat fields and first complete capsules; readonly group remains exact");
  check(Scene::load(file).encode()==expected.encode(),"Immediate accepted Save reopens exact worker result");
 }
 doc.undo();check(doc.copy_scene().encode()==before.encode()&&!doc.frame().state->can_undo,"Exactly one Undo restores complete pre-bind Scene");
 doc.redo();check(doc.copy_scene().encode()==expected.encode(),"Redo restores complete surface heat result");
 // Independent no-op verification on the actual current recipient values; no
 // Document changed flag or public mode is claimed by this prototype bridge.
 {
  auto same=doc.copy_scene();auto request=capture_native_surface_heat(same,ids,reference);auto prepared=prepare_native_surface_heat(request);
  check(prepared.report().changed==0&&!adopt_native_surface_heat(same,request,std::move(prepared)),"Repeated identical actual field is a true factory no-op");
  check(same.encode()==expected.encode(),"No-op preserves all capsules/paint/source values");
 }
 {
  auto locked=doc.copy_scene();uint64_t target=0;size_t row=0;uint32_t joint=0;
  for(auto id:ids){const auto&rows=locked.object(id).deformation->skin->vertices;for(size_t i=0;i<rows.size();++i)if(rows[i].size()==1&&rows[i][0].weight==1){target=id;row=i;joint=rows[i][0].joint;break;}if(target)break;}
  check(target!=0,"Actual generated singleton row supplies an independent hard-lock constraint");
  auto d=std::make_shared<DeformationState>(*locked.object(target).deformation);uint32_t other=0;while(other==joint||!d->skeleton->bones.at(other).deform)++other;
  d->skin->vertices[row]={{joint,.375f},{other,.625f}};d->validate(locked.object(target).painted->mesh());locked.object(target).deformation=d;
  const auto old=locked;
  NativeSurfaceHeatOptions options;options.locks.assign(d->skeleton->bones.size(),1);
  auto request=capture_native_surface_heat(locked,std::span<const uint64_t>(&target,1),reference,options);auto prepared=prepare_native_surface_heat(request);
  check(prepared.report().changed==0&&!adopt_native_surface_heat(locked,request,std::move(prepared))&&locked.encode()==old.encode(),"All hard locks preserve genuinely edited current rows exactly without inventing a field or Undo change");
  options.locks.assign(d->skeleton->bones.size(),0);options.locks[joint]=1;bool infeasible=false;
  try{auto r=capture_native_surface_heat(locked,std::span<const uint64_t>(&target,1),reference,options);auto p=prepare_native_surface_heat(r);(void)p;}catch(const std::invalid_argument&e){infeasible=std::string(e.what()).find("No unlocked weight")!=std::string::npos;}
  check(infeasible&&locked.encode()==old.encode(),"Incompatible partial lock refuses atomically instead of manufacturing unsupported free mass");
 }
 doc.edit_scene("Restore surface heat originals",[ids](Scene&s,const auto&cancel){for(auto id:ids)restore_rig_source(s,id,false,cancel);});auto restored=out/"restored.dsculpt";doc.save_async(restored).get();
 check(doc.wait_asset(std::chrono::seconds(120))&&doc.wait_mesh(std::chrono::seconds(120)),"Actual worker Restore and cache-backed native remesh complete");
 check(doc.copy_scene().encode()==before.encode()&&Scene::load(restored).encode()==before.encode(),"Full original morph/paint/material/source/placement restoration and native restart exact");
 check(doc.frame().surface&&doc.frame().state->error.empty(),"Restored paint retains a valid preview cache and real native frame");
 doc.undo();check(doc.copy_scene().encode()==expected.encode(),"Restore itself owns one complete Undo");
 doc.undo();check(doc.copy_scene().encode()==before.encode(),"Return from redoable selected bind to exact pre-bind source for active API");
 const std::array active_ids{ids.front()};doc.select_objects(active_ids);current_gate=std::make_shared<Gate>();current_gate.load()->armed=false;doc.bind_skin(public_options);auto active_save=out/"active.dsculpt";doc.save_async(active_save).get();
 auto active_before=before;active_before.select(ids.front());auto active_expected=active_before;active_expected.object(ids.front()).deformation=full.object(ids.front()).deformation;active_expected.object(ids.front()).rig_source=before.object(ids.front()).rig_source?before.object(ids.front()).rig_source:capture_polygon_origin(before,ids.front());
 check(doc.wait_asset(std::chrono::seconds(120))&&doc.copy_scene().encode()==active_expected.encode()&&Scene::load(active_save).encode()==active_expected.encode(),"Actual active API binds only its explicit current target and ordered Save reopens exact");
 doc.undo();check(doc.copy_scene().encode()==active_before.encode()&&!doc.frame().state->can_undo,"Active surface heat API owns exactly one complete Undo");
 check(doc.wait_mesh(std::chrono::seconds(120)),"Actual controller receives the current immutable mesh after active Undo");
 const bool dirty_before=doc.frame().state->dirty;NativeDriver driver(doc);driver.open();
 check(doc.frame().state->menu_page==14,"Real joystick reaches the existing native bind page");
 driver.sector(1);driver.value(0);driver.confirm();
 check(doc.frame().state->skin_options.mode==SkinAuthoringMode::Automatic,"Actual controller explicitly stages original legacy Automatic mode");
 driver.sector(1);driver.value(1);driver.confirm();
 check(doc.frame().state->skin_options.mode==SkinAuthoringMode::SurfaceHeat&&doc.copy_scene().encode()==active_before.encode()&&doc.frame().state->dirty==dirty_before,"Actual surface heat Method is staged without changing Scene/history");
 driver.sector(7);auto controller_save=out/"controller.dsculpt";doc.save_async(controller_save).get();
 check(doc.wait_asset(std::chrono::seconds(120))&&doc.frame().state->error.empty()&&doc.copy_scene().encode()==active_expected.encode()&&Scene::load(controller_save).encode()==active_expected.encode(),"Real joystick Bind executes the surface heat worker and accepted Save exactly");
 doc.undo();check(doc.copy_scene().encode()==active_before.encode()&&!doc.frame().state->can_undo,"Controller surface heat bind owns one complete Undo");
 current_gate={};
 std::cout<<"Public surface heat worker checks "<<checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
