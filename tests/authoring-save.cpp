// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-color-target-fixture.hpp"
#include "dome/document.hpp"
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *why) {
  if(!ok)throw std::runtime_error(why);
  ++checks;std::cout<<"PASS "<<why<<std::endl;
}
struct Gate {
  std::mutex mutex;std::condition_variable changed;
  bool armed=false,entered=false;
  void arm(){std::lock_guard lock(mutex);armed=true;entered=false;}
  void stop(){std::unique_lock lock(mutex);if(!armed)return;entered=true;changed.notify_all();changed.wait(lock,[&]{return !armed;});}
  void wait(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(30),[&]{return entered;}))throw std::runtime_error("Actual worker did not enter its barrier");}
  void release(){std::lock_guard lock(mutex);armed=false;changed.notify_all();}
} authoring,durable;
std::atomic<std::condition_variable*> draining{nullptr};
std::atomic_bool observed_drain{false};
thread_local bool destroying=false;
struct Release {~Release(){authoring.release();durable.release();}};
struct AuthoredFailure:std::runtime_error {AuthoredFailure():std::runtime_error("Exact authored worker failure") {}};
struct AuthoredCancel:std::runtime_error {AuthoredCancel():std::runtime_error("Exact authored worker cancellation") {}};
std::string bytes(const std::filesystem::path &path) {
  std::ifstream stream(path,std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
void finish(std::future<void> &future) {
  check(future.wait_for(std::chrono::seconds(60))==std::future_status::ready,"Accepted dependent Save resolves without UI pumping or self-wait");future.get();
}
Scene initial() {Scene scene;scene.active().name="Original authoritative object";return scene;}
void queue(Document &doc,std::string name,bool fail=false) {
  auto draft=doc.draft_scene();authoring.arm();
  doc.edit_scene("Author shared scene",[name=std::move(name),fail](Scene &scene,const auto &cancelled){
    scene.active().name=name;authoring.stop();
    if(cancelled())throw AuthoredCancel();
    if(fail)throw AuthoredFailure();
  },draft.source);
  authoring.wait();
}
}
namespace dome {struct DocumentTransactionTestAccess {
  static auto file_condition(Document &doc){return &doc.file_changed_;}
};}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){struct stat status{};if(!fstat(fd,&status)&&S_ISREG(status.st_mode))durable.stop();return __real_fsync(fd);}
extern "C" void __real__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(std::condition_variable*,std::unique_lock<std::mutex>&);
extern "C" void __wrap__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(std::condition_variable *condition,std::unique_lock<std::mutex> &lock){
  if(destroying&&condition==draining.load())observed_drain.store(true);
  __real__ZNSt18condition_variable4waitERSt11unique_lockISt5mutexE(condition,lock);
}
extern "C" ObjectBakeReport bake_real(Scene&,uint64_t,ObjectBakeSettings,const std::function<bool()>&,const std::function<void(float)>&) asm("__real__ZN4dome11bake_objectERNS_5SceneEmNS_18ObjectBakeSettingsERKSt8functionIFbvEERKS3_IFvfEE");
extern "C" ObjectBakeReport bake_wrap(Scene&,uint64_t,ObjectBakeSettings,const std::function<bool()>&,const std::function<void(float)>&) asm("__wrap__ZN4dome11bake_objectERNS_5SceneEmNS_18ObjectBakeSettingsERKSt8functionIFbvEERKS3_IFvfEE");
extern "C" ObjectBakeReport bake_wrap(Scene &scene,uint64_t source,ObjectBakeSettings settings,const std::function<bool()> &cancelled,const std::function<void(float)> &progress){
  return bake_real(scene,source,settings,cancelled,[&](float value){if(progress)progress(value);if(value>=.05f)authoring.stop();});
}
int main(int argc,char **argv)try {
  if(argc!=2)throw std::runtime_error("Choose a private fixture directory");
  const std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);
  const auto original=initial();const auto before=original.encode();
  {
    Document doc;Release release;doc.replace(original.snapshot());
    const auto path=directory/"accepted.dsculpt";original.save(path);const auto old=bytes(path);
    queue(doc,"Accepted worker endpoint");auto saving=doc.save_async(path);
    check(saving.wait_for(std::chrono::seconds(0))!=std::future_status::ready&&bytes(path)==old&&doc.copy_scene().encode()==before,
      "Save admitted during actual authoring retains old file and detached authoritative scene");
    authoring.release();finish(saving);auto accepted=doc.copy_scene().encode();
    check(doc.wait_asset()&&Scene::load(path).encode()==accepted&&!doc.frame().state->dirty&&doc.frame().state->project_path==path,
      "Save captures the real committed authoring endpoint and adopts only its durable path");
    check(doc.undo()&&doc.copy_scene().encode()==before&&doc.redo()&&doc.copy_scene().encode()==accepted,
      "Authoring and dependent Save create exactly one complete Undo/Redo endpoint");
  }
  {
    Document doc;Release release;doc.replace(original.snapshot());
    const auto path=directory/"accepted-before-undo.dsculpt";
    queue(doc,"Captured accepted result");auto saving=doc.save_async(path);durable.arm();authoring.release();durable.wait();
    auto accepted=doc.copy_scene().encode();check(doc.undo()&&doc.copy_scene().encode()==before,"Undo is usable while the accepted endpoint is durably writing");
    durable.release();finish(saving);
    check(Scene::load(path).encode()==accepted&&doc.copy_scene().encode()==before&&doc.frame().state->dirty,
      "Later Undo cannot replace the captured result or be marked clean by its earlier Save");
  }
  for(int mode=0;mode<3;++mode){
    Document doc;Release release;doc.replace(original.snapshot());
    auto path=directory/("failed-"+std::to_string(mode)+".dsculpt");original.save(path);auto old=bytes(path);
    queue(doc,"Rejected detached result",mode==0);auto saving=doc.save_async(path);
    auto expected=before;
    if(mode==1)doc.cancel_asset_work();
    if(mode==2){auto replacement=original.snapshot();replacement.active().name="Newer generation";doc.replace(replacement);expected=doc.copy_scene().encode();}
    authoring.release();bool exact=false;
    check(saving.wait_for(std::chrono::seconds(60))==std::future_status::ready,"Failure or cancellation resolves every dependent Save");
    try{saving.get();}catch(const AuthoredFailure&){exact=mode==0;}catch(const AuthoredCancel&){exact=mode!=0;}
    check(exact&&doc.wait_asset()&&bytes(path)==old&&doc.copy_scene().encode()==expected&&!doc.frame().state->can_undo,
      "Original worker exception propagates without stale-file fallback, history or newer-generation corruption");
  }
  {
    Document doc;Release release;doc.replace(original.snapshot());queue(doc,"Bounded ordered result");
    std::vector<std::future<void>> saves;
    for(int i=0;i<8;++i)saves.push_back(doc.save_async(directory/("queue-"+std::to_string(i)+".dsculpt")));
    bool refused=false;try{auto ninth=doc.save_async(directory/"ninth-must-not-exist.dsculpt");}catch(const std::runtime_error&){refused=true;}
    check(refused&&!std::filesystem::exists(directory/"ninth-must-not-exist.dsculpt"),"The ninth dependent Save refuses before queue publication or file mutation");
    authoring.release();for(auto &saving:saves)finish(saving);
    auto accepted=doc.copy_scene().encode();bool exact=true;
    for(int i=0;i<8;++i)exact&=Scene::load(directory/("queue-"+std::to_string(i)+".dsculpt")).encode()==accepted;
    check(exact&&doc.frame().state->project_path==directory/"queue-7.dsculpt","All eight accepted saves retain one exact endpoint and ordered final path");
  }
  {
    Release release;auto doc=std::make_unique<Document>();doc->replace(original.snapshot());
    queue(*doc,"Accepted close endpoint");const auto path=directory/"close.dsculpt";auto saving=doc->save_async(path);
    draining.store(DocumentTransactionTestAccess::file_condition(*doc));observed_drain=false;
    std::thread teardown([document=std::move(doc)]()mutable{destroying=true;document.reset();destroying=false;});
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while(!observed_drain.load()&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    const bool observed=observed_drain.load();authoring.release();teardown.join();draining=nullptr;finish(saving);
    check(observed&&Scene::load(path).active().name=="Accepted close endpoint","Close actually waits on the accepted-operation completion predicate and drains its Save before worker shutdown");
  }
  {
    Document doc;Release release;auto scene=current_color_target_fixture::fixture(true);doc.replace(scene.snapshot());check(doc.wait_mesh(),"Actual current-colour target publishes before Bake");
    const auto source=scene.objects().back().id;const auto unchanged=scene.encode();ObjectBakeSettings settings;settings.prepare_colors=true;settings.ao_samples=0;settings.padding=4;
    authoring.arm();doc.bake_from_object(source,settings);authoring.wait();auto path=directory/"actual-bake.dsculpt";auto saving=doc.save_async(path);
    check(doc.copy_scene().encode()==unchanged&&saving.wait_for(std::chrono::seconds(0))!=std::future_status::ready,"Save queues behind real detached current-field Bake preparation");
    authoring.release();finish(saving);auto accepted=doc.copy_scene();auto reopened=Scene::load(path);
    check(doc.wait_asset()&&accepted.encode()==reopened.encode()&&reopened.encode()["version"]==20&&accepted.active().painted->baked()&&accepted.active().material_bindings->color_preparation,
      "Actual Bake and dependent Save retain the complete Scene20 colour receipt and prior source");
    check(doc.undo()&&doc.copy_scene().encode()==unchanged&&doc.redo()&&doc.copy_scene().encode()==accepted.encode(),"Actual Bake keeps exact one-step global history when its Save was accepted mid-preparation");
  }
  std::cout<<checks<<" accepted-authoring Save checks passed\n";return 0;
}catch(const std::exception &error){authoring.release();durable.release();std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
