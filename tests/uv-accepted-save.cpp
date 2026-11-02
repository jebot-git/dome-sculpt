// SPDX-License-Identifier: GPL-3.0-or-later
#include "prepared-normal-shapes-fixtures.hpp"
#include "dome/document.hpp"
#include "dome/source-uv-bake.hpp"
#include <fstream>
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
struct ExactUvFailure:std::runtime_error{ExactUvFailure():std::runtime_error("Exact UV accepted endpoint failure") {}};
struct Gate {
 std::mutex mutex;std::condition_variable changed;bool armed=false,entered=false,released=false,fail=false;
 void arm(bool failure=false){std::lock_guard lock(mutex);armed=true;entered=false;released=false;fail=failure;}
 bool pause(){std::unique_lock lock(mutex);if(!armed)return false;armed=false;entered=true;changed.notify_all();changed.wait(lock,[&]{return released;});return fail;}
 void wait(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(30),[&]{return entered;}))throw std::runtime_error("UV file worker did not reach actual transfer gate");}
 void release(){std::lock_guard lock(mutex);released=true;changed.notify_all();}
} gate;
struct Release {~Release(){gate.release();}};
void ready(Document &doc){if(!doc.wait_asset(std::chrono::seconds(30))||!doc.wait_mesh(std::chrono::seconds(30))||!doc.frame().state->error.empty())throw std::runtime_error("UV endpoint: "+doc.frame().state->error);}
void accept(Document &doc,bool fail=false){doc.begin_uv();check(doc.wait_uv(),"Actual UV draft initializes before accepted endpoint capture");doc.edit_uv(UvAction::Scale,.7f);check(doc.wait_uv()&&doc.frame().state->uv->valid,"Actual source-aware UV draft validates");gate.arm(fail);doc.end_uv(true);gate.wait();check(doc.frame().state->asset_busy&&doc.frame().state->can_save_after_asset,"Accepted UV publishes the shared exact endpoint Save capability");}
void complete(std::future<void> &future){check(future.wait_for(std::chrono::seconds(60))==std::future_status::ready,"Dependent UV Save resolves on the ordered worker");future.get();}
std::string bytes(const std::filesystem::path &path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
template<class F>bool refused(F operation){try{operation();}catch(const std::exception&){return true;}return false;}
}
extern "C" SourceUvBakeResult __real__ZN4dome14bake_source_uvERKNS_17SourceUvBakeInputERKNS_19SourceUvBakeOptionsE(const SourceUvBakeInput&,const SourceUvBakeOptions&);
extern "C" SourceUvBakeResult __wrap__ZN4dome14bake_source_uvERKNS_17SourceUvBakeInputERKNS_19SourceUvBakeOptionsE(const SourceUvBakeInput &input,const SourceUvBakeOptions &options){const bool failure=gate.pause();auto result=__real__ZN4dome14bake_source_uvERKNS_17SourceUvBakeInputERKNS_19SourceUvBakeOptionsE(input,options);if(failure)throw ExactUvFailure();return result;}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);const auto source=prepared_shape_fixture::model(true);const auto original=source.encode();
 {
  Document doc;Release release;doc.replace(source);ready(doc);accept(doc);auto path=directory/"accepted.dsculpt";auto saved=doc.save_async(path);
  check(saved.wait_for(std::chrono::milliseconds(20))!=std::future_status::ready,"Save remains queued behind the exact paused UV operation");
  check(refused([&]{doc.edit_uv(UvAction::Move,.1f,0);}),"An accepted UV job cannot race a changed draft");gate.release();complete(saved);ready(doc);const auto accepted=doc.copy_scene().encode();
  check(accepted!=original&&Scene::load(path).encode()==accepted,"Queued Save stores the actual accepted UV model rather than the prior scene");
  check(doc.undo()&&doc.copy_scene().encode()==original&&doc.redo()&&doc.copy_scene().encode()==accepted,"Accepted UV plus queued Save retains exactly one Undo/Redo boundary");
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
  auto doc=std::make_unique<Document>();Release release;doc->replace(source);ready(*doc);auto path=directory/"closed.dsculpt";accept(*doc);auto saved=doc->save_async(path);
  auto closed=std::async(std::launch::async,[owned=std::move(doc)]()mutable{owned.reset();});
  check(closed.wait_for(std::chrono::milliseconds(20))!=std::future_status::ready,"Close drains accepted UV work when an ordered Save owns its endpoint");gate.release();complete(saved);
  check(closed.wait_for(std::chrono::seconds(60))==std::future_status::ready,"Accepted UV and queued Save complete before document worker teardown");closed.get();
  check(Scene::load(path).encode()!=original,"Close persisted the accepted source-aware UV result, not the previous model");
 }
 check(source.encode()==original,"Every completion, failure, cancellation and close preserves the immutable original source");std::cout<<"PASS "<<checks<<" accepted UV endpoint/Save checks\n";return 0;
}catch(const std::exception &error){gate.release();std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
