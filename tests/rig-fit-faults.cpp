// SPDX-License-Identifier: GPL-3.0-or-later
#define main rig_fit_authoring_fixture_main
#include "rig-fit-authoring.cpp"
#undef main
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <new>
#include <thread>
namespace {
std::atomic_bool denial=false,measure=false,gate_enabled=false,gate_entered=false;
std::atomic_size_t allocation_count=0,fail_at=SIZE_MAX,gate_size=0;
std::thread::id test_thread;
std::mutex gate_mutex;std::condition_variable gate_changed;bool release_gate=false;
void allocation(size_t size){
 if(std::this_thread::get_id()==test_thread&&measure){const auto index=allocation_count.fetch_add(1);if(index>=fail_at){denial=true;throw std::bad_alloc();}}
 if(denial&&std::this_thread::get_id()==test_thread)throw std::bad_alloc();
 if(gate_enabled&&size==gate_size&&std::this_thread::get_id()!=test_thread&&!gate_entered.exchange(true)){std::unique_lock lock(gate_mutex);gate_changed.notify_all();gate_changed.wait(lock,[]{return release_gate;});}
}
struct WorkerGate{
 explicit WorkerGate(size_t bytes){std::lock_guard lock(gate_mutex);release_gate=false;gate_entered=false;gate_size=bytes;gate_enabled=true;}
 void wait(){std::unique_lock lock(gate_mutex);if(!gate_changed.wait_for(lock,std::chrono::seconds(20),[]{return gate_entered.load();}))throw std::runtime_error("Fitting worker did not reach geometry preparation");}
 void release(){std::lock_guard lock(gate_mutex);gate_enabled=false;release_gate=true;gate_changed.notify_all();}
 ~WorkerGate(){release();}
};
}
[[gnu::noinline]] void *operator new(size_t n){allocation(n);if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void*p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void*p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void*p,size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main()try{
 test_thread=std::this_thread::get_id();auto landmarks=marks();auto warm=fixture();author_humanoid_fit(warm,1,landmarks);auto baseline=fixture();const auto before=baseline.encode();allocation_count=0;fail_at=SIZE_MAX;measure=true;author_humanoid_fit(baseline,1,landmarks);measure=false;const auto sites=allocation_count.load();check(sites>0&&sites<10000,"Humanoid fitting has bounded measured allocation sites");
 for(size_t site=0;site<sites;++site){auto s=fixture();auto paint=s.active().painted;auto deform=s.active().deformation;allocation_count=0;fail_at=site;std::exception_ptr failure;measure=true;try{author_humanoid_fit(s,1,landmarks);}catch(...){failure=std::current_exception();}measure=false;denial=false;if(!failure)throw std::runtime_error("Injected fitting allocation did not fail at site "+std::to_string(site)+" of "+std::to_string(sites)+"; observed "+std::to_string(allocation_count.load()));try{std::rethrow_exception(failure);}catch(const std::bad_alloc&){}if(s.encode()!=before||s.active().painted!=paint||s.active().deformation!=deform)throw std::runtime_error("Allocation failure modified original rig source");}
 check(true,"Every measured persistent allocation failure retains exact original Scene and source owners");
 size_t callbacks=0;auto complete=fixture();author_humanoid_fit(complete,1,landmarks,{},[&]{++callbacks;return false;});for(size_t at=1;at<=callbacks;++at){auto s=fixture();size_t count=0;bool cancelled=false;try{author_humanoid_fit(s,1,landmarks,{},[&]{return ++count>=at;});}catch(...){cancelled=true;}if(!cancelled||s.encode()!=before)throw std::runtime_error("Cancellation boundary changed original source");}check(true,"Every observed fitting cancellation boundary is atomic");
 auto original=fixture();Document doc;doc.replace(original);check(doc.wait_mesh(),"Native Cancel fixture drains its original preview");{
  WorkerGate gate(original.active().painted->mesh().positions.size()*sizeof(Vec3));doc.fit_humanoid_rig(landmarks,{},doc.draft_scene().source);gate.wait();check(doc.asset_status().cancellable,"Actually executing rig-fit worker exposes shared Cancel work");doc.cancel_background_work();gate.release();
 }
 check(doc.wait_asset()&&doc.frame().state->error.empty()&&doc.copy_scene().encode()==before&&!doc.frame().state->can_undo,"Actual worker Cancel retains complete unskinned source and history");doc.fit_humanoid_rig(landmarks);check(doc.wait_asset()&&doc.frame().state->error.empty()&&doc.copy_scene().active().deformation->skin,"Same native worker retries successfully after cancellation");
 check(doc.undo()&&doc.copy_scene().encode()==before,"Ordered Save fixture restores its exact original unrigged state");
 const auto save=std::filesystem::temp_directory_path()/("dome-rig-fit-ordered-"+std::to_string(getpid())+".dsculpt");
 {WorkerGate gate(original.active().painted->mesh().positions.size()*sizeof(Vec3));doc.fit_humanoid_rig(landmarks);gate.wait();auto saving=doc.save_async(save);check(saving.wait_for(std::chrono::milliseconds(0))==std::future_status::timeout,"Save accepted behind executing fitting waits for its actual scene endpoint");gate.release();saving.get();}
 check(doc.wait_asset()&&Scene::load(save).encode()==doc.copy_scene().encode()&&doc.copy_scene().active().deformation->skin,"Blocked fitting completion publishes the exact rig endpoint to its already accepted Save");std::filesystem::remove(save);
 std::cout<<sites<<" persistent fitting allocation boundaries and "<<callbacks<<" cancellation boundaries passed\n";return 0;
}catch(const std::exception&e){measure=false;denial=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
