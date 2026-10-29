// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <mutex>
#include <unistd.h>
using namespace dome;
namespace {
std::mutex gate_mutex;
std::condition_variable gate_cv;
bool gate_enabled=false, gate_entered=false;
std::thread::id worker;
void arm() { std::lock_guard lock(gate_mutex); gate_enabled=true; gate_entered=false; }
void release() { std::lock_guard lock(gate_mutex); gate_enabled=false; gate_cv.notify_all(); }
void entered() {
  std::unique_lock lock(gate_mutex);
  if (!gate_cv.wait_for(lock,std::chrono::seconds(15),[]{return gate_entered;}))
    throw std::runtime_error("File worker never reached fsync barrier");
}
void check(bool ok,const char *message) {
  if (!ok) throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
bool failed(std::future<void> &f) {
  check(f.wait_for(std::chrono::seconds(30))==std::future_status::ready,"File result resolves without pumping UI events");
  try { f.get();return false; } catch (...) { return true; }
}
void finish(std::future<void> &f) {
  check(f.wait_for(std::chrono::seconds(30))==std::future_status::ready,"Accepted file result completes");f.get();
}
void rename(Document &doc,const char *name) {
  auto s=doc.copy_scene();const auto &o=s.active();
  doc.object_properties(o.id,name,o.visible,o.locked,o.transform,o.material);
}
struct Scope {
  std::filesystem::path path=std::filesystem::temp_directory_path()/("dome-project-files-"+std::to_string(getpid()));
  Scope(){std::filesystem::create_directories(path);}
  ~Scope(){release();std::error_code ec;std::filesystem::remove_all(path,ec);}
};
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  {
    std::unique_lock lock(gate_mutex);
    if(gate_enabled) {
      worker=std::this_thread::get_id();gate_entered=true;gate_cv.notify_all();
      gate_cv.wait(lock,[]{return !gate_enabled;});
    }
  }
  return __real_fsync(fd);
}
int main() try {
  Scope scope;auto original=scope.path/"original.dsculpt", target=scope.path/"saved.dsculpt", newer=scope.path/"newer.dsculpt";
  Scene scene;scene.active().name="Original";scene.save(original);scene.save(target);
  Document doc;
  struct GateCleanup { ~GateCleanup(){release();} } gate_cleanup;
  doc.replace(scene);rename(doc,"First snapshot");
  arm();auto first=doc.save_async(target);entered();
  check(worker!=std::this_thread::get_id() && first.wait_for(std::chrono::seconds(0))!=std::future_status::ready,
        "Native Save returns while durable filesystem work remains blocked on a separate thread");
  check(Scene::load(target).active().name=="Original" && doc.frame().state->dirty && doc.frame().state->project_path.empty(),
        "Pending Save preserves the old file, unsaved marker and project identity");
  rename(doc,"Later edit");auto second=doc.save_async(newer);
  auto cancelled=doc.open_async(original);
  check(doc.frame().state->asset_busy && doc.asset_status().cancellable && !doc.begin_desktop(),
        "Queued Open leases document editing and exposes cancellation while rendering remains available");
  doc.cancel_asset_work();release();finish(first);finish(second);
  check(failed(cancelled) && doc.wait_asset() && !doc.frame().state->asset_busy,
        "Cancelled queued Open resolves its result and releases the document lease");
  check(Scene::load(target).active().name=="First snapshot" && Scene::load(newer).active().name=="Later edit" &&
        doc.copy_scene().active().name=="Later edit" && doc.frame().state->project_path==newer && !doc.frame().state->dirty,
        "Ordered saves retain their captured scenes; cancelling Open preserves the newer saved project");
  auto loaded=doc.open_async(original);finish(loaded);
  check(doc.copy_scene().encode()==scene.encode() && doc.frame().state->project_path==original && !doc.frame().state->dirty && !doc.asset_status().cancellable,
        "Successful background Open replaces scene and history atomically and publishes its path");
  rename(doc,"Unsaved must survive");auto before=doc.copy_scene().encode();auto bad=doc.open_async(scope.path/"missing.dsculpt");
  check(failed(bad) && doc.copy_scene().encode()==before && doc.frame().state->dirty && doc.frame().state->project_path==original && !doc.frame().state->asset_busy,
        "Failed Open retains exact unsaved edits and project identity and releases the lease");
  check(doc.undo() && doc.copy_scene().active().name=="Original" && doc.redo() && doc.copy_scene().encode()==before,
        "Failed Open preserves exact native Undo and Redo history");
  auto save_bad=doc.save_async(scope.path/"missing/output.dsculpt");
  check(failed(save_bad) && doc.copy_scene().encode()==before && doc.frame().state->dirty && doc.frame().state->project_path==original,
        "Failed Save never marks edits saved or adopts an unwritten path");
  arm();auto slow=doc.save_async(target);entered();auto stale=doc.open_async(original);doc.seed();auto seeded=doc.copy_scene().encode();
  release();finish(slow);
  check(failed(stale) && doc.copy_scene().encode()==seeded && doc.frame().state->project_path.empty() && doc.frame().state->error.empty(),
        "New document generation invalidates an old queued Open without changing the replacement or its status");
  int callbacks=0;bool rejected=false;
  try { Scene::load(original,[&]{return ++callbacks==2;}); } catch (...) { rejected=true; }
  check(rejected && callbacks==2,"Project read cancellation is checked before file reads and parsing");
  std::future<void> drained;
  {Document ending;ending.replace(scene);rename(ending,"Shutdown snapshot");drained=ending.save_async(scope.path/"shutdown.dsculpt");}
  finish(drained);
  check(Scene::load(scope.path/"shutdown.dsculpt").active().name=="Shutdown snapshot","Document shutdown drains accepted native Save jobs");
  check(doc.frame().state->error.empty(),"Old-generation file completion does not publish a spurious error");
  return 0;
} catch(const std::exception &e) { release();std::cerr<<e.what()<<'\n';return 1; }
