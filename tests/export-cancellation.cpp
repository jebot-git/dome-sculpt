// SPDX-License-Identifier: GPL-3.0-or-later
// Real file-worker cancellation, deterministic barriers at actual fsync calls.
#define main existing_export_regression
#include "document-export.cpp"
#undef main
#include "io.hpp"
#include <sys/stat.h>
namespace dome {
struct DocumentNativeTestAccess {
  static bool stopping(const Document &doc){return doc.stopping_.load();}
};
}
namespace gate {
std::mutex mutex;
std::condition_variable changed;
bool entered=false, released=false;
std::atomic<int> target{0};
void arm(bool directory=false){
  std::lock_guard lock(mutex);entered=released=false;target=directory?2:1;
}
void release(){std::lock_guard lock(mutex);released=true;target=0;changed.notify_all();}
bool wait(){std::unique_lock lock(mutex);return changed.wait_for(lock,std::chrono::seconds(30),[]{return entered;});}
struct Release {~Release(){release();}};
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){
  int result=__real_fsync(fd);if(result)return result;
  struct stat info{};int kind=0;
  if(!fstat(fd,&info))kind=S_ISDIR(info.st_mode)?2:S_ISREG(info.st_mode)?1:0;
  if(kind && gate::target.compare_exchange_strong(kind,0)){
    std::unique_lock lock(gate::mutex);gate::entered=true;gate::changed.notify_all();
    gate::changed.wait(lock,[]{return gate::released;});
  }
  return result;
}
namespace {
void seed(const std::filesystem::path &p){std::ofstream f(p,std::ios::binary);f<<"retained prior output";}
bool no_temporaries(const std::filesystem::path &p){
  for(const auto &entry:std::filesystem::directory_iterator(p))
    if(entry.path().filename().string().find(".tmp-")!=std::string::npos)return false;
  return true;
}
bool cancelled(std::future<void> result){
  if(result.wait_for(std::chrono::seconds(30))!=std::future_status::ready)throw std::runtime_error("Export cancellation timeout");
  try{result.get();}catch(const std::exception &e){return std::string_view(e.what()).find("cancelled")!=std::string_view::npos;}
  return false;
}
void atomic_boundary(const std::filesystem::path &dir){
  auto out=dir/"atomic.bin";seed(out);auto original=bytes(out);
  std::vector<uint8_t> payload(3*1024*1024,0xab);size_t calls=0,written=0;
  AtomicWriteOptions options;
  options.cancelled=[&]{
    if(++calls!=3)return false;
    for(const auto &entry:std::filesystem::directory_iterator(dir))
      if(entry.path().filename().string().starts_with("atomic.bin.tmp-"))written=std::filesystem::file_size(entry.path());
    return true;
  };
  check(rejects([&]{atomic_write(out,payload,options);}) && written==1024*1024,
        "Cancellation interrupts an actual bounded temporary write after its first MiB");
  check(bytes(out)==original && no_temporaries(dir),"Cancelled atomic output preserves prior bytes and removes its partial temporary");
  bool claimed=false;options.cancelled=[&]{return claimed;};options.before_commit=[&]{claimed=true;};
  atomic_write(out,payload,options);
  check(claimed && std::filesystem::file_size(out)==payload.size() && no_temporaries(dir),
        "A claimed replacement finishes without a misleading post-commit cancellation");
}
void queued_cancel(const std::filesystem::path &dir){
  Document doc;gate::Release release;doc.replace(fixture());auto before=doc.copy_scene().encode();
  auto one=dir/"running.glb",two=dir/"queued.glb",project=dir/"ordered.dsculpt";seed(one);seed(two);
  auto original=bytes(one);gate::arm();auto running=doc.export_asset(one);
  check(gate::wait(),"Real export reaches durable temporary output on the ordered file worker");
  auto queued=doc.export_asset(two);auto save=doc.save_async(project);
  check(doc.frame().state->pending_exports==2 && doc.frame().state->can_cancel_asset && !doc.frame().state->asset_busy,
        "Queued and running exports expose cancellation while authoring remains available");
  Controller hand(doc);hand.send(0,0,true);hand.send();hand.sector(8);hand.sector(9);hand.sector(6);
  auto state=doc.frame().state;
  check(state->menu_page==12 && state->menu_enabled[5],"Actual joystick baking page enables Cancel work for snapshot exports");
  hand.sector(5);
  check(!doc.frame().state->can_cancel_asset && doc.frame().state->pending_exports==2,
        "Actual controller Cancel claims both exports without cancelling the following project save");
  gate::release();check(cancelled(std::move(running)) && cancelled(std::move(queued)),"Running and queued completion futures both report explicit cancellation");
  finish(std::move(save));
  check(bytes(one)==original && bytes(two)==original && no_temporaries(dir),"Cancellation preserves both existing outputs and leaves no temporary files");
  check(Scene::load(project).encode()==before && doc.copy_scene().encode()==before && !doc.frame().state->dirty,
        "The accepted following save retains exact scene/history and adopts its project path");
  check(!doc.frame().state->pending_exports && !doc.frame().state->can_cancel_asset && doc.frame().state->error.empty(),
        "Completed cancellation retires all export controls without a stale error");
  finish(doc.export_asset(one));auto parsed=parse(one);
  check(parsed->meshes_count==1 && !doc.frame().state->pending_exports,"A new export succeeds after cancellation and parses independently");
}
void replacement_and_late_cancel(const std::filesystem::path &dir){
  Document doc;gate::Release release;doc.replace(fixture());
  gate::arm();auto accepted=doc.export_asset(dir/"replaced.glb");check(gate::wait(),"Snapshot export reaches its real precommit barrier before replacement");
  auto next=fixture();next.active().material.color={0,1,0,1};doc.replace(std::move(next));gate::release();finish(std::move(accepted));
  auto original=parse(dir/"replaced.glb");auto color=base_pixel(*original);
  check(color[0]>250 && color[1]<3 && doc.copy_scene().active().material.color[1]==1,
        "Document replacement preserves accepted old snapshot output and the current green scene");
  gate::arm();auto old=doc.export_asset(dir/"old-cancelled.glb");
  check(gate::wait(),"Another accepted export reaches the durable barrier before document replacement");
  auto blue=fixture();blue.active().material.color={0,0,1,1};doc.replace(std::move(blue));
  doc.cancel_background_work();gate::release();
  check(cancelled(std::move(old)) && !std::filesystem::exists(dir/"old-cancelled.glb") &&
        !doc.frame().state->pending_exports && doc.frame().state->message=="Export cancelled" &&
        doc.copy_scene().active().material.color[2]==1,
        "Explicit Cancel still reaches an older accepted snapshot and retires status in the new document");
  gate::arm(true);auto committed=doc.export_asset(dir/"committed.glb");check(gate::wait(),"Actual output reaches directory sync after atomic replacement");
  bool disabled=!doc.frame().state->can_cancel_asset;
  // Published state may precede the worker's atomic commit claim. The explicit
  // Cancel still must fail to claim it and publish the accurate disabled state.
  doc.cancel_background_work();disabled=disabled || !doc.frame().state->can_cancel_asset;
  gate::release();finish(std::move(committed));
  check(disabled && parse(dir/"committed.glb")->meshes_count==1 && doc.frame().state->error.empty(),
        "Cancel after replacement cannot turn successful committed output into cancellation");
}
void avatar_cancel(const std::filesystem::path &dir){
  Scene scene;append_humanoid(scene,humanoid_baseline_defaults("vroid-beta-female"),"CC0",{},64);
  Document doc;gate::Release release;doc.replace(std::move(scene));auto before=doc.copy_scene().encode();
  auto out=dir/"cancelled.vrm";seed(out);auto original=bytes(out);
  gate::arm();auto result=doc.export_asset(out,AssetExportFormat::Avatar);check(gate::wait(),"Actual source-preserving VRM export reaches its durable temporary barrier");
  doc.cancel_background_work();gate::release();
  check(cancelled(std::move(result)) && bytes(out)==original && no_temporaries(dir) && doc.copy_scene().encode()==before,
        "Native Cancel stops VRM replacement while preserving original output and complete source archives");
}
void close_drains(const std::filesystem::path &dir){
  auto doc=std::make_unique<Document>();doc->replace(fixture());
  gate::arm();auto one=doc->export_asset(dir/"close-first.glb");
  if(!gate::wait()){gate::release();throw std::runtime_error("Close barrier timeout");}
  auto two=doc->export_asset(dir/"close-second.glb");Document *observed=doc.get();
  std::atomic<bool> done=false;std::thread close([&]{doc.reset();done=true;});
  auto end=std::chrono::steady_clock::now()+std::chrono::seconds(30);
  while(!DocumentNativeTestAccess::stopping(*observed) && std::chrono::steady_clock::now()<end)std::this_thread::yield();
  bool draining=DocumentNativeTestAccess::stopping(*observed)&&!done.load();gate::release();close.join();
  check(draining,"Real destructor requests worker shutdown while accepted output is still paused");
  finish(std::move(one));finish(std::move(two));
  check(done && parse(dir/"close-first.glb")->meshes_count==1 && parse(dir/"close-second.glb")->meshes_count==1,
        "Closing drains both running and queued exports instead of treating shutdown as cancellation");
}
}
int main(int argc,char **argv)try{
  if(argc==3 && std::string_view(argv[1])=="--ui-fixture"){fixture().save(argv[2]);return 0;}
  if(argc==3 && std::string_view(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
  if(argc==3 && std::string_view(argv[1])=="--validate-glb"){return parse(argv[2])->meshes_count==1?0:1;}
  auto dir=std::filesystem::temp_directory_path()/("dome-export-cancel-"+std::to_string(getpid()));std::filesystem::create_directories(dir);
  atomic_boundary(dir);queued_cancel(dir);replacement_and_late_cancel(dir);avatar_cancel(dir);close_drains(dir);
  std::filesystem::remove_all(dir);return 0;
}catch(const std::exception&e){gate::release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
