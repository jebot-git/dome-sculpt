// SPDX-License-Identifier: GPL-3.0-or-later
#include "cut-color-fixture.hpp"
#include "dome/document.hpp"
#include <iostream>
#include <sys/stat.h>
using namespace dome;
namespace dome { struct DocumentNativeTestAccess {
 static std::condition_variable *files(Document &d){return &d.file_changed_;}
}; }
namespace {
std::condition_variable *shutdown_condition=nullptr;
thread_local bool destroying=false;
std::mutex shutdown_mutex;std::condition_variable shutdown_changed;bool shutdown_entered=false;

size_t checks=0;
void check(bool yes,const char *why){if(!yes)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
struct Gate {
  std::mutex mutex;std::condition_variable changed;bool armed=false;size_t entered=0,released=0;
  void arm(){std::lock_guard lock(mutex);armed=true;entered=released=0;}
  void stop(){std::unique_lock lock(mutex);if(!armed)return;auto ticket=++entered;changed.notify_all();changed.wait(lock,[&]{return !armed||released>=ticket;});}
  void wait(size_t ticket=1){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(10),[&]{return entered>=ticket;}))throw std::runtime_error("Save barrier not entered");}
  void advance(){std::lock_guard lock(mutex);++released;changed.notify_all();}
  void release(){std::lock_guard lock(mutex);armed=false;changed.notify_all();}
} gate;
struct Release{~Release(){gate.release();}};
struct Controller{
 Document &d;SpatialFrame frame;
 Controller(Document &doc):d(doc){frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};frame.hands[1].tracked.valid=true;send();}
 void send(float x=0,float y=0,bool press=false){++frame.sequence;frame.time+=.02;auto &h=frame.hands[1];h.x=x;h.y=y;h.joystick=press;d.submit(frame);if(!d.wait_input())throw std::runtime_error("Controller input stalled");}
 void open(){send(0,0,true);send();}
 void sector(int index){float angle=index*6.283185307f/12;send(std::cos(angle),std::sin(angle));send(std::cos(angle),std::sin(angle),true);send();}
};
void ready(Document &d){check(d.wait_cut()&&d.wait_mesh()&&d.frame().state->error.empty()&&d.frame().state->cut&&d.frame().state->cut->preview,"Native current-color source/preview prepares on real workers");}
void outputs(Document &d,const Scene &source){
 auto scene=d.copy_scene();check(scene.objects().size()==3&&scene.active().polygon&&scene.active().polygon->colors,"Native accepted cut creates current-field shell and independent cap");
 auto shell=scene.active().id;auto cap=scene.objects().back().id;
 check(scene.active().polygon->colors->source_owner()==source.active().polygon->colors->source_owner()&&scene.active().polygon->origin->shape_source==source.active().shape_source,"Native worker retains exact shared field basis and complete original shape capsule");
 check(d.frame().state->selected_objects==std::vector<uint64_t>{shell}&&d.object_editable(shell)&&!d.object_editable(cap),"Accepted shell is selected/editable while the unselected cap remains a read-only ghost");
 std::array<uint64_t,1> choose{cap};d.select_objects(choose);
 check(d.object_editable(cap)&&!d.object_editable(shell),"Explicit selection transfers edit authority to the new cap without changing stored visibility");
 choose[0]=shell;d.select_objects(choose);
}
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){struct stat status{};if(!fstat(fd,&status)&&S_ISREG(status.st_mode))gate.stop();return __real_fsync(fd);}
extern "C" void __real__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(std::condition_variable *,std::unique_lock<std::mutex> &);
extern "C" void __wrap__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(std::condition_variable *condition,std::unique_lock<std::mutex> &lock) {
 if(destroying&&condition==shutdown_condition){std::lock_guard ready(shutdown_mutex);shutdown_entered=true;shutdown_changed.notify_all();}
 __real__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(condition,lock);
}
int main(int argc,char **argv)try{
 std::filesystem::path directory=argc>1?argv[1]:std::filesystem::temp_directory_path()/"dome-colored-cut-doc";std::filesystem::create_directories(directory);
 for(bool affine:{false,true})for(bool pose:{false,true}){
  auto original=cut_color_fixture::scene(true,affine);if(pose)cut_color_fixture::pose(original);auto baseline=original.encode();
  Document d;d.replace(original.snapshot());d.configure_selection(true);std::array<uint64_t,1> choose{original.active().id};d.select_objects(choose);d.configure_selection(false);
  check(d.wait_mesh()&&d.frame().state->can_cut,"Real native readiness enables the supported current-field plane cut");
  d.begin_cut();ready(d);auto immutable=d.frame().surface;auto settings=d.frame().state->cut->settings;settings.offset_mm=31;d.edit_cut(settings);ready(d);
  check(d.copy_scene().encode()==baseline&&!immutable->source_colors.empty(),"Immutable clipping previews retain RGBA and never change source authority");
  d.end_cut(false);check(d.copy_scene().encode()==baseline&&!d.frame().state->can_undo&&!d.frame().surface->clipped_object,"Cancel restores exact source and leaves no history");
  d.begin_cut();ready(d);settings=d.frame().state->cut->settings;settings.offset_mm=31;d.edit_cut(settings);ready(d);d.end_cut(true);
  auto file=directory/("accepted-"+std::to_string(affine)+"-"+std::to_string(pose)+".dsculpt");auto saving=d.save_async(file);saving.get();
  check(d.wait_asset()&&d.wait_mesh()&&!d.frame().state->cut&&d.frame().state->error.empty(),"Immediate ordered Save drains accepted colored-cut work");outputs(d,original);
  auto accepted=d.copy_scene().encode();check(Scene::load(file).encode()==accepted,"Disk restart equals the actual accepted scene rather than its pre-Apply snapshot");
  check(d.undo()&&d.copy_scene().encode()==baseline&&!d.frame().state->can_undo,"Exactly one global Undo restores all original field, pose, paint, placement and visibility");
  check(d.redo()&&d.copy_scene().encode()==accepted,"Redo restores exact accepted color/cap/source authority");
  d.restore_polygon_source();check(d.wait_asset()&&d.copy_scene().active().painted->encode()==original.active().painted->encode(),"Actual Restore source command recovers retained editable paint and original UVs");
 }
 // A real preceding fsync holds accepted work; Cancel/retry exercise its own
 // asset lease instead of relying on a slow machine or arbitrary delay.
 for(bool controller:{false,true}){
  auto original=cut_color_fixture::scene(true,true);auto encoded=original.encode();Document d;Release release;d.replace(original.snapshot());
  auto previous=directory/(controller?"previous-controller.dsculpt":"previous-native.dsculpt");gate.arm();auto saving=d.save_async(previous);gate.wait();
  d.begin_cut();ready(d);auto preview=d.frame().state->cut->preview;d.end_cut(true);
  check(d.frame().state->asset_busy&&d.frame().state->cut->committing&&d.asset_status().cancellable,"Accepted colored cut owns its guarded cancellable worker lease behind an earlier Save");
  if(controller){Controller hand(d);hand.open();check(d.frame().state->menu_page==17&&d.frame().state->menu_enabled[11],"Controller opens real page17 with accepted-work Cancel enabled");hand.sector(11);}else d.end_cut(false);
  check(d.asset_status().cancelling,"Native/controller Cancel targets the accepted cut lease");gate.release();saving.get();check(d.wait_asset(),"Cancellation drains the accepted worker");
  check(d.copy_scene().encode()==encoded&&d.frame().state->cut&&d.frame().state->cut->preview==preview&&!d.frame().state->cut->committing,"Cancellation retains the exact editable draft, original RGBA and source graph");
  d.end_cut(true);d.finish_spatial();auto file=directory/(controller?"retry-controller.dsculpt":"retry-native.dsculpt");d.save_async(file).get();
  check(d.wait_asset()&&!d.frame().state->cut&&Scene::load(file).encode()==d.copy_scene().encode()&&d.undo()&&d.copy_scene().encode()==encoded,"Retry, spatial shutdown and ordered Save commit one exact source-retaining transaction");
 }
 {
  auto original=cut_color_fixture::scene();auto encoded=original.encode();Document d;d.replace(original);Controller hand(d);hand.open();hand.sector(8);hand.sector(9);hand.sector(4);hand.sector(0);ready(d);
  hand.open();hand.sector(7);d.finish_spatial();check(d.wait_asset()&&!d.frame().state->cut&&d.copy_scene().objects().size()==3&&d.undo()&&d.copy_scene().encode()==encoded,"Joystick-only page17 Apply survives shutdown and preserves exact one-Undo original");
 }
 {
  auto original=cut_color_fixture::scene();auto encoded=original.encode();Document d;d.replace(original);d.begin_cut();ready(d);auto settings=d.frame().state->cut->settings;settings.offset_mm=-1000;d.edit_cut(settings);ready(d);d.end_cut(true);
  check(!d.frame().state->cut&&d.copy_scene().encode()==encoded&&!d.frame().state->can_undo,"Exact no-op colored cut closes without output IDs or history");
  d.begin_cut();d.replace(original);check(d.wait_cut()&&!d.frame().state->cut&&d.copy_scene().encode()==encoded,"A newer document generation rejects stale source/preview publication");
 }
 // Save owns the accepted cut endpoint even when a later Undo happens before
 // its disk fsync completes. Both regular-file barriers are observed, not timed.
 {
  auto original=cut_color_fixture::scene(true);auto encoded=original.encode();Document d;Release release;d.replace(original);
  gate.arm();auto earlier=d.save_async(directory/"endpoint-before.dsculpt");gate.wait();d.begin_cut();ready(d);d.end_cut(true);auto path=directory/"endpoint-after.dsculpt";auto saving=d.save_async(path);
  gate.advance();earlier.get();gate.wait(2);auto accepted=d.copy_scene().encode();check(d.undo()&&d.copy_scene().encode()==encoded,"Undo after accepted publication restores original while its captured Save is writing");
  gate.release();saving.get();check(Scene::load(path).encode()==accepted&&d.copy_scene().encode()==encoded&&d.frame().state->dirty,"Pending Save writes its exact accepted cut endpoint without marking a later Undo clean");
 }
 for(bool replace:{false,true}){
  auto original=cut_color_fixture::scene(true);auto encoded=original.encode();Document d;Release release;d.replace(original);auto path=directory/(replace?"stale-cut-save.dsculpt":"cancelled-cut-save.dsculpt");original.save(path);
  gate.arm();auto earlier=d.save_async(directory/"failure-before.dsculpt");gate.wait();d.begin_cut();ready(d);d.end_cut(true);auto saving=d.save_async(path);
  if(replace)d.replace(original);else d.end_cut(false);
  gate.release();earlier.get();bool failed=false;try{saving.get();}catch(const std::exception&){failed=true;}
  check(failed&&Scene::load(path).encode()==encoded&&d.copy_scene().encode()==encoded,"Cancelled or newer-generation cut fails dependent Save without writing any stale scene or changing original authority");
  if(d.frame().state->cut)d.end_cut(false);
 }
 {
  auto original=cut_color_fixture::scene(true);std::future<void> saving;
  auto d=std::make_unique<Document>();Release release;d->replace(original);gate.arm();auto earlier=d->save_async(directory/"teardown-before.dsculpt");gate.wait();d->begin_cut();ready(*d);d->end_cut(true);auto path=directory/"teardown-accepted.dsculpt";saving=d->save_async(path);
  shutdown_condition=DocumentNativeTestAccess::files(*d);shutdown_entered=false;
  std::thread teardown([&]{destroying=true;d.reset();destroying=false;});
  bool entered=false;{std::unique_lock lock(shutdown_mutex);entered=shutdown_changed.wait_for(lock,std::chrono::seconds(10),[]{return shutdown_entered;});}
  gate.release();teardown.join();earlier.get();saving.get();check(entered&&Scene::load(path).objects().size()==3,"Destruction observes the actual pending-cut predicate wait and drains accepted cut plus ordered Save before stopping workers");
 }
 std::cout<<checks<<" native/controller colored cut checks\n";return 0;
}catch(const std::exception &e){gate.release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
