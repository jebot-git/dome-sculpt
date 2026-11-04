// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-anatomical-weights.hpp"
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
 if(argc!=4)throw std::invalid_argument("input907 full-field907 output");
 const std::filesystem::path out=argv[3];std::filesystem::create_directories(out);
 setenv("XDG_STATE_HOME",(out/"state").c_str(),1);setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
 auto input=Scene::load(argv[1]);auto full=Scene::load(argv[2]);uint64_t reference=0;std::vector<uint64_t>ids;
 for(auto&o:input.objects())if(o.deformation&&o.deformation->skeleton)reference=o.id;
 for(auto&o:input.objects())if(o.id!=reference&&o.mesh_active&&o.painted&&o.appearance&&!o.rig_source&&ids.size()<2)ids.push_back(o.id);
 check(reference&&ids.size()==2,"Actual unskinned current reference and explicit two-target subset");
 Document doc;doc.replace(input);doc.select_objects(ids);check(doc.wait_mesh(std::chrono::seconds(120)),"Actual viewed native source mesh fence ready");
 auto live=doc.frame().surface;check(live&&!live->ranges.empty(),"Real native frame owns source markers before capture");
 const auto before=doc.copy_scene();
 auto expected=before;
 for(auto id:ids){auto&o=expected.object(id);o.deformation=full.object(id).deformation;o.rig_source=capture_polygon_origin(before,id);}
 const auto file=out/"accepted.dsculpt",prior=out/"prior.dsculpt";doc.save_async(file).get();std::filesystem::copy_file(file,prior,std::filesystem::copy_options::overwrite_existing);
 auto queue=[&](const std::shared_ptr<Gate>&gate){doc.edit_scene("Private anatomical current-group bind",[ids,reference,gate](Scene&s,const auto&cancel){auto request=capture_native_anatomical(s,ids,reference);auto prepared=prepare_native_anatomical(request,[&]{if(gate->armed&&++gate->calls==64){gate->entered=true;while(gate->armed&&!(cancel&&cancel()))std::this_thread::sleep_for(std::chrono::milliseconds(1));}return cancel&&cancel();});adopt_native_anatomical(s,request,std::move(prepared));});};
 {
  auto gate=std::make_shared<Gate>();queue(gate);wait_gate(*gate);auto save=doc.save_async(file);
  check(doc.frame().state->asset_busy&&files_equal(file,prior),"Accepted dependent Save leaves prior file untouched during preparation");
  doc.cancel_background_work();gate->armed=false;bool cancelled=false;try{save.get();}catch(const std::exception&){cancelled=true;}
  check(cancelled&&doc.wait_asset(std::chrono::seconds(120))&&doc.copy_scene().encode()==before.encode()&&!doc.frame().state->can_undo&&files_equal(file,prior),"Actual mid-helper Cancel fails dependent Save and preserves whole Scene/history/prior file");
 }
 {
  auto gate=std::make_shared<Gate>();queue(gate);wait_gate(*gate);auto save=doc.save_async(file);gate->armed=false;save.get();
  check(doc.wait_asset(std::chrono::seconds(120))&&doc.frame().state->error.empty(),"Real native worker publishes prepared anatomical subset");
  check(doc.copy_scene().encode()==expected.encode(),"Only explicit two targets receive exact907 fields and first complete capsules; readonly group remains exact");
  check(Scene::load(file).encode()==expected.encode(),"Immediate accepted Save reopens exact worker result");
 }
 doc.undo();check(doc.copy_scene().encode()==before.encode()&&!doc.frame().state->can_undo,"Exactly one Undo restores complete pre-bind Scene");
 doc.redo();check(doc.copy_scene().encode()==expected.encode(),"Redo restores complete anatomical result");
 // Independent no-op verification on the actual current recipient values; no
 // Document changed flag or public mode is claimed by this prototype bridge.
 {
  auto same=doc.copy_scene();auto request=capture_native_anatomical(same,ids,reference);auto prepared=prepare_native_anatomical(request);
  check(prepared.report().changed==0&&!adopt_native_anatomical(same,request,std::move(prepared)),"Repeated identical actual field is a true factory no-op");
  check(same.encode()==expected.encode(),"No-op preserves all capsules/paint/source values");
 }
 doc.edit_scene("Restore anatomical originals",[ids](Scene&s,const auto&cancel){for(auto id:ids)restore_rig_source(s,id,false,cancel);});auto restored=out/"restored.dsculpt";doc.save_async(restored).get();
 check(doc.wait_asset(std::chrono::seconds(120))&&doc.wait_mesh(std::chrono::seconds(120)),"Actual worker Restore and cache-backed native remesh complete");
 check(doc.copy_scene().encode()==before.encode()&&Scene::load(restored).encode()==before.encode(),"Full original morph/paint/material/source/placement restoration and native restart exact");
 check(doc.frame().surface&&doc.frame().state->error.empty(),"Restored paint retains a valid preview cache and real native frame");
 doc.undo();check(doc.copy_scene().encode()==expected.encode(),"Restore itself owns one complete Undo");
 std::cout<<"Native anatomical worker checks "<<checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
