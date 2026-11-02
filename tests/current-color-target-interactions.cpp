// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-color-target-fixture.hpp"
#include "dome/bake-authoring.hpp"
#include "dome/document.hpp"
#include <iostream>
namespace {
using namespace dome;
size_t checks=0;void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
struct Gate {
 std::mutex mutex;std::condition_variable changed;bool armed=false,entered=false;
 std::function<bool()> cancelled;
 void arm(){std::lock_guard lock(mutex);armed=true;entered=false;cancelled={};}
 void pause(float p,const std::function<bool()> &check){if(p<.05f)return;std::unique_lock lock(mutex);if(!armed)return;cancelled=check;entered=true;changed.notify_all();changed.wait(lock,[&]{return !armed;});cancelled={};}
 bool stopping(){std::lock_guard lock(mutex);return cancelled&&cancelled();}
 void wait(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(30),[&]{return entered;}))throw std::runtime_error("Actual completed corner preparation did not reach worker gate");}
 void release(){std::lock_guard lock(mutex);armed=false;changed.notify_all();}
} gate;
struct Release {~Release(){gate.release();}};
ObjectBakeSettings settings(){ObjectBakeSettings s;s.prepare_colors=true;s.ao_samples=0;s.padding=4;return s;}
}
extern "C" dome::ObjectBakeReport bake_real(dome::Scene&,uint64_t,dome::ObjectBakeSettings,const std::function<bool()>&,const std::function<void(float)>&) asm("__real__ZN4dome11bake_objectERNS_5SceneEmNS_18ObjectBakeSettingsERKSt8functionIFbvEERKS3_IFvfEE");
extern "C" dome::ObjectBakeReport bake_wrap(dome::Scene&,uint64_t,dome::ObjectBakeSettings,const std::function<bool()>&,const std::function<void(float)>&) asm("__wrap__ZN4dome11bake_objectERNS_5SceneEmNS_18ObjectBakeSettingsERKSt8functionIFbvEERKS3_IFvfEE");
extern "C" dome::ObjectBakeReport bake_wrap(dome::Scene &scene,uint64_t source,dome::ObjectBakeSettings settings,const std::function<bool()> &cancelled,const std::function<void(float)> &progress){return bake_real(scene,source,settings,cancelled,[&](float p){if(progress)progress(p);gate.pause(p,cancelled);});}
int main(int argc,char **argv)try{
 const auto original=current_color_target_fixture::fixture(true);const auto target=original.active().id,source=original.objects().back().id;const auto before=original.encode();
 {
  Document d;Release release;d.replace(original.snapshot());check(d.wait_mesh(),"Current target publishes before real preparation cancellation");gate.arm();d.bake_from_object(source,settings());gate.wait();
  check(d.copy_scene().encode()==before&&d.frame().state->asset_busy,"Actual completed target preparation remains detached before native publication");d.cancel_asset_work();gate.release();check(d.wait_asset()&&d.copy_scene().encode()==before&&!d.frame().state->can_undo,"Explicit native Cancel after actual field preparation preserves source/paint/geometry/history exactly");
  d.bake_from_object(source,settings());check(d.wait_asset()&&d.wait_mesh()&&d.frame().state->error.empty(),"Cancelled target preparation retries through the real native worker");
 }
 {
  Document d;Release release;d.replace(original.snapshot());check(d.wait_mesh(),"Current target publishes before newer-generation arbitration");gate.arm();d.bake_from_object(source,settings());gate.wait();auto replacement=original.snapshot();replacement.active().name="Newer authoritative target";d.replace(replacement);
  const auto newer=d.copy_scene().encode();gate.release();check(d.wait_asset()&&d.wait_mesh()&&d.copy_scene().encode()==newer&&!d.frame().state->can_undo,"Actual stale prepared target cannot overwrite a newer native generation");
 }
 {
  Document d;Release release;d.replace(original.snapshot());check(d.wait_mesh(),"Current target publishes before accepted focus-loss arbitration");gate.arm();d.bake_from_object(source,settings());gate.wait();
  SpatialFrame frame;frame.sequence=1;frame.focused=false;d.submit(frame);check(d.wait_input()&&d.frame().state->asset_busy,"Actual focus loss preserves an already accepted target job rather than losing ownership");gate.release();check(d.wait_asset()&&d.wait_mesh()&&d.frame().state->error.empty()&&d.undo()&&d.copy_scene().encode()==before,"Accepted target finishes across focus loss and remains one exact Undo");
 }
 {
  auto d=std::make_unique<Document>();Release release;d->replace(original.snapshot());check(d->wait_mesh(),"Current target publishes before real shutdown cancellation");gate.arm();d->bake_from_object(source,settings());gate.wait();
  std::thread destroying([document=std::move(d)]()mutable{document.reset();});auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);while(!gate.stopping()&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();const bool stopped=gate.stopping();gate.release();destroying.join();check(stopped&&original.encode()==before,"Actual destruction requests cancellation through the real worker predicate and drains without mutating its immutable input");
 }
 {
  Document d;d.replace(original.snapshot());check(d.wait_mesh(),"Current target publishes before held other-hand editing");d.configure_object_grab(true,true);d.hold_object(target);d.bake_from_object(source,settings());check(d.wait_asset()&&d.wait_mesh()&&d.frame().state->error.empty(),"Pinned-held target Bake preserves central edit isolation");
  const auto field=d.copy_scene().active().polygon->colors;const auto recipe=d.copy_scene().active().material_bindings->color_preparation;const auto source_before=current_color_bake_fixture::source_only(d.copy_scene(),source);
  PaintSettings ink;ink.front_only=false;ink.color={.9f,.1f,.25f,1};d.paint_settings(ink);
  SpatialFrame frame;frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};frame.hands[0].tracked.valid=frame.hands[1].tracked.valid=true;frame.hands[0].tracked.grip.position={0,1.4f,-.3f};frame.hands[1].tracked.grip.position={2,2,2};
  auto send=[&]{++frame.sequence;frame.time+=.05;d.submit(frame);if(!d.wait_input())throw std::runtime_error("Held current-target input timed out");};send();frame.hands[0].tracked.squeeze=1;send();frame.hands[0].tracked.grip.position.x+=.1f;send();
  auto state=d.frame().state;auto object=std::find_if(state->objects.begin(),state->objects.end(),[&](const auto &o){return o.id==target;});const auto world=ob::compose(state->canvas,object->world_transform);frame.hands[1].tracked.grip.position=ob::transform(world,{0,0,.2f});auto prior=d.copy_scene().active().painted->encode();frame.hands[1].tracked.trigger=1;send();send();frame.hands[1].tracked.trigger=0;send();
  check(d.frame().state->object_grabbing&&d.copy_scene().active().painted->encode()!=prior&&d.copy_scene().active().polygon->colors==field&&d.copy_scene().active().material_bindings->color_preparation==recipe&&current_color_bake_fixture::source_only(d.copy_scene(),source)==source_before,"Actual other-hand painting changes only the physically held prepared target while preserving original field/recipe and ghost source");
  frame.hands[0].tracked.squeeze=0;send();check(!d.frame().state->object_grabbing&&d.copy_scene().active().transform.position.x>.09f,"Physical release commits held target placement without relabelling REST color authority");
  auto directory=std::filesystem::path(argc>1?argv[1]:"current-target-interactions");std::filesystem::create_directories(directory);auto path=directory/"held.dsculpt";d.save(path);Document reopened;reopened.open(path);check(reopened.wait_asset()&&reopened.wait_mesh()&&reopened.copy_scene().encode()==d.copy_scene().encode(),"Actual held painting and placement restart with complete current/original Scene20 authority");auto glb=directory/"held.glb";reopened.export_asset(glb).get();check(std::filesystem::file_size(glb)>1000,"Actual held prepared target still exports after painting and physical release");
 }
 std::cout<<checks<<" actual current target cancellation/generation/shutdown/held/controller checks\n";return 0;
}catch(const std::exception &e){gate.release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
