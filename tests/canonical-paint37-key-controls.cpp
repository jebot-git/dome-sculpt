// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/shapekey-authoring.hpp"
#include "canonical-paint-sampling.hpp"
#include "normal-key-derivation.hpp"
#include <iostream>
#include <future>
#include <condition_variable>
#include <mutex>
using namespace dome;
namespace union_key_gate {
struct Gate {std::mutex mutex;std::condition_variable cv;bool released=false;std::promise<void> entered;nlohmann::json computed_morphs;uint64_t computed_object=0;bool changed=false;};
std::mutex mutex;std::shared_ptr<Gate> pending;thread_local unsigned depth=0;
struct Release {std::shared_ptr<Gate> gate;~Release(){release();}void release(){if(gate){std::lock_guard lock(gate->mutex);gate->released=true;gate->cv.notify_all();}}};
}
extern "C" ShapeKeyResult __real__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE(Scene&,uint64_t,const ShapeKeyEdit&,const std::function<bool()>&);
extern "C" ShapeKeyResult __wrap__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE(Scene &scene,uint64_t object,const ShapeKeyEdit &edit,const std::function<bool()> &cancelled){
 struct Depth {Depth(){++union_key_gate::depth;}~Depth(){--union_key_gate::depth;}}depth;
 std::shared_ptr<union_key_gate::Gate> gate;
 if(union_key_gate::depth==1){std::lock_guard lock(union_key_gate::mutex);gate=std::exchange(union_key_gate::pending,{});}
 ShapeKeyResult result;const auto computation_start=std::chrono::steady_clock::now();
 try{
  result=__real__ZN4dome15author_shapekeyERNS_5SceneEmRKNS_12ShapeKeyEditERKSt8functionIFbvEE(scene,object,edit,cancelled);
  if(gate){
   // Observe the real computed target without performing another full Scene
   // serialization on this held worker. Whole Scene/file/history equality is
   // still checked below after cancellation and on every accepted endpoint.
   const auto &target=scene.object(object);
   if(!target.painted||!target.deformation||!target.deformation->morphs)throw std::runtime_error("Computed key has no actual native morph target");
   gate->computed_object=object;gate->computed_morphs=target.deformation->morphs->encode();gate->changed=result.changed;
   std::cout<<"KEY_GATE_FACTORY seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-computation_start).count()<<std::endl;
   gate->entered.set_value();
  }
 }catch(...){if(gate)gate->entered.set_exception(std::current_exception());throw;}
 if(gate){
  // Test boundary AFTER the real detached factory has finished. This proves
  // Cancel before native publication, independently of internal kernel polls.
  std::unique_lock lock(gate->mutex);
  if(!gate->cv.wait_for(lock,std::chrono::seconds(60),[&]{return gate->released;}))throw std::runtime_error("Test key publication gate timeout");
  if(cancelled&&cancelled())throw std::runtime_error("Shapekey publication cancelled after actual detached computation");
 }
 return result;
}
namespace {
size_t checks=0;void check(bool ok,const char*what){++checks;std::cout<<(ok?"PASS ":"FAIL ")<<what<<std::endl;if(!ok)throw std::runtime_error(what);}
bool rejects(auto fn){try{fn();return false;}catch(const std::exception&e){std::cout<<"EXPECTED_KEY_CONTROL "<<e.what()<<std::endl;return true;}}
void ready(Document&d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
void cancel_publication(Document&doc,const ShapeKeyEdit&edit,const std::filesystem::path&file){
 const auto before=doc.copy_scene().encode();doc.save_async(file).get();const auto state=doc.frame().state;auto gate=std::make_shared<union_key_gate::Gate>();union_key_gate::Release release{gate};auto entered=gate->entered.get_future();{std::lock_guard lock(union_key_gate::mutex);union_key_gate::pending=gate;}
 doc.author_shape(edit);check(entered.wait_for(std::chrono::seconds(120))==std::future_status::ready,"Actual post37 key worker reaches real completed detached computation");entered.get();check(gate->changed,"Late Cancel gate observes an actually changed computed current key");auto saved=doc.save_async(file);check(saved.wait_for(std::chrono::milliseconds(20))!=std::future_status::ready,"Immediate native Save stays ordered behind held actual key publication");doc.cancel_background_work();release.release();check(saved.wait_for(std::chrono::seconds(120))==std::future_status::ready,"Native Cancel drains computed key and dependent Save");check(rejects([&]{saved.get();})&&doc.wait_asset()&&doc.wait_mesh()&&doc.copy_scene().encode()==before&&Scene::load(file).encode()==before&&doc.frame().state->can_undo==state->can_undo&&doc.frame().state->can_redo==state->can_redo,"Post-computation Cancel preserves full prior Paint/normal/model/owners/file/UndoRedo");
}
}
int main(int argc,char**argv)try{
 if(argc!=3)return 2;const auto source=Scene::load(argv[1]);const auto exact=source.encode();const auto out=std::filesystem::path(argv[2]);std::filesystem::create_directories(out);const auto binding=source.active().material_bindings;check(binding->canonical_paint_sampling&&binding->key_derivation&&binding->key_derivation->before_material()->canonical_paint_sampling==binding->canonical_paint_sampling,"Controls use genuine post37 key with its independently owned current writable context");
 {auto probe=Scene::decode(exact);const auto b=probe.active().material_bindings;auto role=std::const_pointer_cast<NormalFrames>(b->normal_frames->pbr);const auto old=role->normals.at(0);role->normals[0].x+=.031f;check(rejects([&]{b->validate(false);}),"Shallow binding call still fully replays actual changed target/base N/T at the same mutable decoded address");role->normals[0]=old;b->validate(false);check(probe.encode()==exact,"Fresh call after legal mutable-array tamper restores exact normal/key proof with no persistent validity cache");}
 {auto probe=Scene::decode(exact);auto b=std::const_pointer_cast<SourceMaterialBindings>(probe.active().material_bindings);const auto old=b->key_derivation;b->key_derivation=std::shared_ptr<const detail::SourceKeyDerivation>(old.get(),[](const auto*){});check(rejects([&]{b->validate(false);}),"Same-address foreign key control block cannot borrow canonical writable context");b->key_derivation=old;b->validate(false);check(probe.encode()==exact,"Foreign-control-block rejection leaves its actual owner and complete source recoverable");}
 {auto probe=Scene::decode(exact);auto foreign=Scene::decode(exact);const auto b=probe.active().material_bindings;auto before=std::const_pointer_cast<SourceMaterialBindings>(b->key_derivation->before_material());const auto old=before->canonical_paint_sampling;before->canonical_paint_sampling=foreign.active().material_bindings->canonical_paint_sampling;check(rejects([&]{b->validate();}),"Genuine separately decoded equal-value37 paint owner cannot replace the actual key-before context");before->canonical_paint_sampling=old;b->validate();check(probe.encode()==exact,"Exact source and private context recover after foreign-before-owner refusal");}
 ShapeKeyEdit create;create.name="Current37 late-cancel Create";ShapeKeyEdit replace;replace.action=ShapeKeyAction::Replace;replace.target=source.active().deformation->morphs->targets.size()-1;replace.positions=source.active().painted->mesh().positions;replace.positions[1].z+=.011f;
 for(const auto &edit:{create,replace}){
  auto complete=source.snapshot();size_t polls=0;author_shapekey(complete,source.active().id,edit,[&]{++polls;return false;});check(complete.encode()!=exact&&polls>3,"Actual post37 detached key computes a changed endpoint and measures reachable callback polls");for(size_t at:{size_t(1),polls/2,polls-1}){auto attempt=source.snapshot();size_t count=0;check(rejects([&]{author_shapekey(attempt,source.active().id,edit,[&]{return ++count>=at;});})&&count>=at&&attempt.encode()==exact,"Actual early/middle/late internal current-key Cancel leaves complete unchanged source");}
  Document doc;doc.replace(source);ready(doc);cancel_publication(doc,edit,out/(edit.action==ShapeKeyAction::Create?"create.dsculpt":"replace.dsculpt"));
 }
 check(source.encode()==exact,"Every post37 key context and cancellation control preserves immutable original caller scene");std::cout<<"PASS "<<checks<<" current37 key owner/cancel controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
