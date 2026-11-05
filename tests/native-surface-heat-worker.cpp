// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-surface-heat.hpp"
#include "dome/document.hpp"
#include "dome/rig-rest.hpp"
#include <fstream>
#include <iostream>
#include <thread>
using namespace dome;using namespace dome::detail;
namespace {
size_t checks=0;void check(bool v,const char*s){if(!v)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<std::endl;}
bool files_equal(const std::filesystem::path&a,const std::filesystem::path&b){std::ifstream x(a,std::ios::binary),y(b,std::ios::binary);if(!x||!y)return false;std::array<char,65536>p{},q{};do{x.read(p.data(),p.size());y.read(q.data(),q.size());if(x.gcount()!=y.gcount()||!std::equal(p.begin(),p.begin()+x.gcount(),q.begin()))return false;}while(x.gcount());return true;}
struct Gate {std::atomic_bool armed{true},entered{false};std::atomic_size_t calls{0};~Gate(){armed=false;}};
void wait_gate(Gate&g){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(120);while(!g.entered&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));check(g.entered&&g.calls==64,"Actual preparation reached deterministic real callback64 gate");}
}
int main(int argc,char**argv)try{
 if(argc!=4)throw std::invalid_argument("genuine-unskinned input actual-owning-heat-field output");
 const std::filesystem::path out=argv[3];std::filesystem::create_directories(out);
 setenv("XDG_STATE_HOME",(out/"state").c_str(),1);setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
 auto input=Scene::load(argv[1]);auto full=Scene::load(argv[2]);uint64_t reference=0;std::vector<uint64_t>ids;
 for(auto&o:input.objects())if(o.deformation&&o.deformation->skeleton)reference=o.id;
 for(auto&o:input.objects())if(o.mesh_active&&o.painted&&o.appearance)ids.push_back(o.id);
 check(reference&&ids.size()==19,"Actual unskinned current reference and explicit full nineteen-target request");
 Document doc;doc.replace(input);doc.select_objects(ids);check(doc.wait_mesh(std::chrono::seconds(120)),"Actual viewed native source mesh fence ready");
 auto live=doc.frame().surface;check(live&&!live->ranges.empty(),"Real native frame owns source markers before capture");
 const auto before=doc.copy_scene();
 auto expected=before;
 for(auto id:ids){auto&o=expected.object(id);o.deformation=full.object(id).deformation;o.rig_source=before.object(id).rig_source?before.object(id).rig_source:capture_polygon_origin(before,id);}
 const auto file=out/"accepted.dsculpt",prior=out/"prior.dsculpt";doc.save_async(file).get();std::filesystem::copy_file(file,prior,std::filesystem::copy_options::overwrite_existing);
 auto queue=[&](const std::shared_ptr<Gate>&gate){doc.edit_scene("Private surface heat current-group bind",[ids,reference,gate](Scene&s,const auto&cancel){auto request=capture_native_surface_heat(s,ids,reference);auto prepared=prepare_native_surface_heat(request,[&]{if(gate->armed&&++gate->calls==64){gate->entered=true;while(gate->armed&&!(cancel&&cancel()))std::this_thread::sleep_for(std::chrono::milliseconds(1));}return cancel&&cancel();});adopt_native_surface_heat(s,request,std::move(prepared));});};
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
  check(doc.wait_mesh(std::chrono::seconds(120)),"Replacement native mesh fence ready before same-controller retry");
 }
 {
  auto gate=std::make_shared<Gate>();queue(gate);wait_gate(*gate);auto save=doc.save_async(file);gate->armed=false;save.get();
  check(doc.wait_asset(std::chrono::seconds(120))&&doc.frame().state->error.empty(),"Real native worker publishes prepared surface heat subset");
  check(doc.copy_scene().encode()==expected.encode(),"All explicit nineteen targets receive exact owning heat fields and first complete capsules; readonly group remains exact");
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
 std::cout<<"Native surface heat worker checks "<<checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
