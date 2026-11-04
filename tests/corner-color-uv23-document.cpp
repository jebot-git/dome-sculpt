// SPDX-License-Identifier: GPL-3.0-or-later
#include "bake-fixture-identity.hpp"
#include "current-color-target-fixture.hpp"
#include "dome/color-preparation.hpp"
#include "dome/document.hpp"
#include "dome/source-uv-bake.hpp"
#include <fstream>
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
struct ExactUvFailure:std::runtime_error{ExactUvFailure():std::runtime_error("Exact corner-colour UV accepted endpoint failure") {}};
struct Gate {
 std::mutex mutex;std::condition_variable changed;bool armed=false,entered=false,released=false,fail=false;
 void arm(bool failure=false){std::lock_guard lock(mutex);armed=true;entered=false;released=false;fail=failure;}
 bool pause(){std::unique_lock lock(mutex);if(!armed)return false;armed=false;entered=true;changed.notify_all();changed.wait(lock,[&]{return released;});return fail;}
 void wait(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(30),[&]{return entered;}))throw std::runtime_error("UV file worker did not reach actual transfer gate");}
 void release(){std::lock_guard lock(mutex);released=true;changed.notify_all();}
} gate,save_gate;
struct Release {~Release(){gate.release();save_gate.release();}};
void ready(Document &doc){if(!doc.wait_asset(std::chrono::seconds(30))||!doc.wait_mesh(std::chrono::seconds(30))||!doc.frame().state->error.empty())throw std::runtime_error("UV endpoint: "+doc.frame().state->error);}
void accept(Document &doc,bool fail=false){doc.begin_uv();check(doc.wait_uv(),"Actual UV draft initializes before accepted endpoint capture");const auto &mesh=doc.frame().state->uv->mesh;size_t face=0;while(face<mesh->indices.size()/3&&mesh->normals[mesh->indices[face*3]].z<.9f)++face;
 std::array<float,2> cursor{};for(size_t k=0;k<3;++k){auto q=mesh->uv[mesh->indices[face*3+k]];for(size_t d=0;d<2;++d)cursor[d]+=q[d]/3;}doc.edit_uv(UvAction::Pick,cursor[0],cursor[1]);if(!doc.wait_uv())throw std::runtime_error("Current UV Pick timeout");doc.edit_uv(UvAction::Move,1.f/64,0);check(doc.wait_uv()&&doc.frame().state->uv->valid,"Actual source-aware UV draft validates");gate.arm(fail);doc.end_uv(true);gate.wait();check(doc.frame().state->asset_busy&&doc.frame().state->can_save_after_asset,"Accepted UV publishes the shared exact endpoint Save capability");}
void complete(std::future<void> &future){check(future.wait_for(std::chrono::seconds(60))==std::future_status::ready,"Dependent UV Save resolves on the ordered worker");future.get();}
std::string bytes(const std::filesystem::path &path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
template<class F>bool refused(F operation){try{operation();}catch(const std::exception&){return true;}return false;}
}
extern "C" SourceUvBakeResult __real__ZN4dome14bake_source_uvERKNS_17SourceUvBakeInputERKNS_19SourceUvBakeOptionsE(const SourceUvBakeInput&,const SourceUvBakeOptions&);
extern "C" SourceUvBakeResult __wrap__ZN4dome14bake_source_uvERKNS_17SourceUvBakeInputERKNS_19SourceUvBakeOptionsE(const SourceUvBakeInput &input,const SourceUvBakeOptions &options){const bool failure=gate.pause();auto result=__real__ZN4dome14bake_source_uvERKNS_17SourceUvBakeInputERKNS_19SourceUvBakeOptionsE(input,options);if(failure)throw ExactUvFailure();return result;}
extern "C" void __real__ZNK4dome5Scene4saveERKNSt10filesystem7__cxx114pathE(const Scene*,const std::filesystem::path&);
extern "C" void __wrap__ZNK4dome5Scene4saveERKNSt10filesystem7__cxx114pathE(const Scene *scene,const std::filesystem::path &path){save_gate.pause();__real__ZNK4dome5Scene4saveERKNSt10filesystem7__cxx114pathE(scene,path);}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);auto initial=current_color_target_fixture::fixture(true);const auto other=initial.objects().back().id;
 Document baked;baked.replace(initial);ObjectBakeSettings settings;settings.prepare_colors=true;settings.ao_samples=0;baked.bake_from_object(other,settings);ready(baked);
 const auto source=baked.copy_scene();const auto original=source.encode();
 {
  Document doc;Release release;doc.replace(source);ready(doc);accept(doc);auto path=directory/"accepted.dsculpt";auto saved=doc.save_async(path);
  check(saved.wait_for(std::chrono::milliseconds(20))!=std::future_status::ready,"Save remains queued behind the exact paused UV operation");
  check(refused([&]{doc.edit_uv(UvAction::Move,.1f,0);}),"An accepted UV job cannot race a changed draft");gate.release();complete(saved);ready(doc);const auto accepted=doc.copy_scene().encode();
  check(accepted.at("version")==30&&doc.copy_scene().active().bake_recipe==source.active().bake_recipe&&accepted!=original&&Scene::load(path).encode()==accepted,"Queued Save stores the actual accepted UV model rather than the prior scene");
  check(doc.undo()&&doc.copy_scene().encode()==original&&doc.redo()&&doc.copy_scene().encode()==accepted,"Accepted UV plus queued Save retains exactly one Undo/Redo boundary");
 }
 {
  Document doc;Release release;doc.replace(source);ready(doc);accept(doc);auto path=directory/"captured-after-undo.dsculpt";auto saved=doc.save_async(path);
  save_gate.arm();gate.release();save_gate.wait();ready(doc);const auto accepted=doc.copy_scene().encode();
  check(doc.undo()&&doc.copy_scene().encode()==original,"Undo may restore the original model while its previously accepted UV Save owns the exact endpoint");
  check(saved.wait_for(std::chrono::milliseconds(20))!=std::future_status::ready,"The actual file writer is gated after capturing the accepted UV snapshot");
  save_gate.release();complete(saved);
  check(Scene::load(path).encode()==accepted&&doc.copy_scene().encode()==original,"Previously queued Save writes the captured Scene23 result after Undo without adopting the current older scene");
  check(doc.redo()&&doc.copy_scene().encode()==accepted,"Late dependent Save leaves the exact accepted UV redo history intact");
 }
 {
  Document doc;Release release;doc.replace(source);ready(doc);auto path=directory/"cancelled.dsculpt";source.save(path);const auto old=bytes(path);accept(doc);auto saved=doc.save_async(path);doc.cancel_background_work();gate.release();
  check(saved.wait_for(std::chrono::seconds(60))==std::future_status::ready&&refused([&]{saved.get();}),"Cancelled UV cancels its dependent Save without a stale-scene fallback");
  check(bytes(path)==old&&doc.copy_scene().encode()==original,"Cancelled endpoint preserves exact original model and existing output bytes");
 }
 {
  Document doc;Release release;doc.replace(source);ready(doc);auto path=directory/"failed.dsculpt";source.save(path);const auto old=bytes(path);accept(doc,true);auto saved=doc.save_async(path);gate.release();
  check(saved.wait_for(std::chrono::seconds(60))==std::future_status::ready,"Late UV failure releases dependent Save");bool exact=false;try{saved.get();}catch(const ExactUvFailure&){exact=true;}
  check(exact&&bytes(path)==old&&doc.copy_scene().encode()==original,"The shared UV endpoint preserves its original exception type and exact old source/output");
 }
 {
  Document doc;Release release;doc.replace(source);ready(doc);auto path=directory/"replaced.dsculpt";source.save(path);const auto old=bytes(path);accept(doc);auto saved=doc.save_async(path);auto replacement=source;replacement.active().name="New generation";doc.replace(replacement);gate.release();
  check(saved.wait_for(std::chrono::seconds(60))==std::future_status::ready&&refused([&]{saved.get();}),"Document replacement rejects the old accepted UV endpoint Save");
  check(bytes(path)==old&&doc.copy_scene().encode()==replacement.encode(),"Stale UV completion cannot capture the replacement model or overwrite prior output");
 }
 {
  // The actual default64 fixture correctly refuses .7 scaling at its declared
  // field bound. Use an explicitly imported256 atlas for this tighter-chart
  // positive controller workflow; do not relax the accepted quality policy.
  auto larger=current_color_target_fixture::fixture(true,256);Document prepared;prepared.replace(larger);prepared.bake_from_object(larger.objects().back().id,settings);ready(prepared);
  const auto controller_source=prepared.copy_scene();const auto controller_before=controller_source.encode();
  Document controller;controller.replace(controller_source);ready(controller);
  SpatialFrame frame;frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};frame.hands[1].tracked.valid=true;
  auto send=[&](float x=0,float y=0,bool press=false){++frame.sequence;frame.time+=.05;frame.hands[1].x=x;frame.hands[1].y=y;frame.hands[1].joystick=press;controller.submit(frame);if(!controller.wait_input())throw std::runtime_error("Corner UV controller timeout");};
  auto sector=[&](int cell){float a=cell*3.14159265358979323846f/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();};
  send();send(0,0,true);send();sector(8);sector(9);sector(3);
  check(controller.wait_uv()&&controller.frame().state->uv&&controller.frame().state->controls.parameter==VrParameter::UvEdit,"Actual joystick navigation enters the existing UV workspace for the prepared corner target");
  controller.edit_uv(UvAction::Scale,.7f);check(controller.wait_uv()&&controller.frame().state->uv->valid,"Actual prepared-corner UV scale validates for the supported unposed normal basis");
  auto draft=controller.frame().state->uv->mesh->uv;send(-1,0);send();check(controller.wait_uv()&&controller.frame().state->uv->mesh->uv!=draft,"Controller physical axis changes the actual prepared-corner draft");
  send(0,0,true);send();ready(controller);check(!controller.frame().state->uv&&controller.copy_scene().encode().at("version")==30&&controller.copy_scene().active().bake_recipe==source.active().bake_recipe,"Joystick Confirm publishes the actual Scene30 colour UV model and retained bake recipe");
  check(controller.undo()&&controller.copy_scene().encode()==controller_before,"Controller accepted colour UV is one exact global Undo");
 }
 {
  auto doc=std::make_unique<Document>();Release release;doc->replace(source);ready(*doc);auto path=directory/"closed.dsculpt";accept(*doc);auto saved=doc->save_async(path);
  auto closed=std::async(std::launch::async,[owned=std::move(doc)]()mutable{owned.reset();});
  check(closed.wait_for(std::chrono::milliseconds(20))!=std::future_status::ready,"Close drains accepted UV work when an ordered Save owns its endpoint");gate.release();complete(saved);
  check(closed.wait_for(std::chrono::seconds(60))==std::future_status::ready,"Accepted UV and queued Save complete before document worker teardown");closed.get();
  check(Scene::load(path).encode()!=original,"Close persisted the accepted source-aware UV result, not the previous model");
 }
 check(source.encode()==original,"Every completion, failure, cancellation and close preserves the immutable original source");std::cout<<"PASS "<<checks<<" prepared current-corner native UV endpoint/Save checks\n";return 0;
}catch(const std::exception &error){gate.release();save_gate.release();std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
