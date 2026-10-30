// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <semaphore>
#include <thread>
#include <unistd.h>
using namespace dome;
struct Gate {std::binary_semaphore entered{0},release{0};std::atomic_bool released=false;void open(){if(!released.exchange(true))release.release();}};
namespace dome {struct DocumentTransactionTestAccess {
 static std::thread::id input(Document&d){return d.input_thread_.get_id();}static std::thread::id file(Document&d){return d.file_thread_.get_id();}
 static bool pending(Document&d){return d.worker_notices_pending_.load()||d.input_notice_pending_.load();}
 static void waiting_weight(Document&d,std::shared_ptr<Gate> gate){std::lock_guard lock(d.model_mutex_);auto work=std::make_shared<AssetWork>();work->held_weight=true;d.queue_scene_operation_locked("Waiting held work",[gate](Scene&,const auto&cancelled){gate->entered.release();gate->release.acquire();if(cancelled())throw std::runtime_error("Waiting operation cancelled");},{},{},work,std::make_shared<bool>(false));}
};}
namespace {
std::atomic_bool denied=false,arm_sync=false;std::atomic<size_t> observed=0;std::thread::id target;
std::shared_ptr<Gate> sync_gate;
size_t checks=0;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;}
template<class F>void await(F f,const char*m){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!f()){if(std::chrono::steady_clock::now()>end)throw std::runtime_error(m);std::this_thread::sleep_for(std::chrono::milliseconds(2));}}
Mesh quad(){Mesh m;m.positions={{-.3f,-.3f,0},{.3f,-.3f,0},{.3f,.3f,0},{-.3f,.3f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
std::vector<uint8_t> read(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}
void*operator new(size_t n){if(denied.load(std::memory_order_acquire)&&std::this_thread::get_id()==target){++observed;throw std::bad_alloc();}if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){if(std::this_thread::get_id()==target&&arm_sync.exchange(false)){if(sync_gate){sync_gate->entered.release();sync_gate->release.acquire();}const int status=__real_fsync(fd);if(!status){denied.store(true,std::memory_order_release);errno=EIO;return -1;}return status;}return __real_fsync(fd);}
int main(int argc,char**argv)try {
 const std::string mode=argc>1?argv[1]:"file";auto home=std::filesystem::temp_directory_path()/("dome-worker-publication-"+std::to_string(getpid()));std::filesystem::create_directories(home);setenv("HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);
 struct Cleanup{std::filesystem::path p;~Cleanup(){denied=false;std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{home};
 Scene source;source.active().painted=std::make_shared<PaintedMesh>(quad(),64);source.active().mesh_active=true;Document d;d.replace(source);check(d.wait_mesh(),"source surface is prepared");const auto baseline=d.copy_scene().encode();
 if(mode.rfind("input",0)==0) {
   d.configure_object_grab(true,false);SpatialFrame f;f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[0].tracked.valid=true;f.hands[0].tracked.grip.position={0,1.4f,-.3f};
   auto send=[&]{++f.sequence;f.time+=.05;d.submit(f);check(d.wait_input(),"actual input worker completes an admitted frame");};send();f.hands[0].tracked.squeeze=1;send();check(d.frame().state->object_grabbing,"physical controller establishes held gesture");
   auto busy=mode=="input-busy"?std::make_shared<Gate>():nullptr;
   struct ReleaseGate{std::shared_ptr<Gate> gate;~ReleaseGate(){if(gate)gate->open();}}release_gate{busy};
   if(busy){DocumentTransactionTestAccess::waiting_weight(d,busy);busy->entered.acquire();}
   target=DocumentTransactionTestAccess::input(d);denied=true;f.hands[0].tracked.grip.position.x+=.2f;send();await([]{return observed.load()>0;},"input allocation denial was not reached");std::this_thread::sleep_for(std::chrono::milliseconds(60));
   check(d.copy_scene().encode()==baseline,"denied actual controller work preserves source/history");
   auto expected=baseline;
   if(mode=="input-late"){auto next=source.snapshot();next.active().material.color[1]=.123f;d.replace(next);expected=next.encode();}
   denied=false;
   if(mode=="input-late"){await([&]{return !DocumentTransactionTestAccess::pending(d);},"old input notice was not retired");check(d.copy_scene().encode()==expected&&d.frame().state->error.empty(),"old input failure cannot overwrite replacement source/status");std::cout<<checks<<" input generation checks passed\n";return 0;}
   await([&]{return !d.frame().state->error.empty();},"input error did not republish after memory recovery");
   if(busy){check(d.frame().state->asset_busy,"controller recovery does not wait for the unrelated running authoring worker");busy->open();check(d.wait_asset(),"unrelated held work drains after controller recovery");}
   check(!d.frame().state->object_grabbing&&d.copy_scene().encode()==baseline,"failed controller preview is cancelled without committing partial motion");
   d.report_error({});f.hands[0].tracked.squeeze=0;send();f.hands[0].tracked.squeeze=1;send();f.hands[0].tracked.grip.position.x+=.2f;send();
   check(d.frame().state->objects[0].world_transform.position.x>.19f,"actual input worker retries controller movement after memory recovery");d.end_object_grab(false);check(d.copy_scene().encode()==baseline,"controller retry/cancel remains source exact");
 }else if(mode.rfind("export",0)==0) {
   auto path=home/"accepted.glb";d.export_asset(path).get();const auto bytes=read(path);target=DocumentTransactionTestAccess::file(d);
   if(mode=="export-late")sync_gate=std::make_shared<Gate>();
   arm_sync=true;auto future=d.export_asset(path);auto expected=baseline;
   if(sync_gate){sync_gate->entered.acquire();auto next=source.snapshot();next.active().material.color[1]=.123f;d.replace(next);expected=next.encode();sync_gate->open();}
   await([]{return observed.load()>0;},"export worker publication denial was not reached");std::this_thread::sleep_for(std::chrono::milliseconds(60));
   check(future.wait_for(std::chrono::seconds(1))==std::future_status::ready,"failed accepted export completes its promise under actual worker denial");bool caught=false;try{future.get();}catch(const std::bad_alloc&){caught=true;}check(caught,"accepted export future retains original bad_alloc");check(read(path)==bytes&&d.copy_scene().encode()==expected,"precommit failure preserves output and authoritative source");
   denied=false;await([&]{return d.frame().state->pending_exports==0&&(mode=="export-late"?d.frame().state->error.empty():!d.frame().state->error.empty());},"failed export state did not republish after memory recovery");d.export_asset(path).get();check((mode=="export-late"?read(path)!=bytes:read(path)==bytes)&&d.frame().state->pending_exports==0,"ordered export worker retries and drains after memory recovery");
 }else {
   target=DocumentTransactionTestAccess::file(d);std::binary_semaphore entered(0),release(0);auto error=std::make_exception_ptr(std::runtime_error("Original source validation failed before any authoritative publication"));
   d.edit_scene("Source validation failure",[&](Scene &,const auto&){entered.release();release.acquire();std::rethrow_exception(error);});entered.acquire();denied=true;release.release();await([]{return observed.load()>0;},"file error-publication denial was not reached");std::this_thread::sleep_for(std::chrono::milliseconds(60));
   check(d.copy_scene().encode()==baseline&&!d.frame().state->can_undo,"failed source validation under worker denial preserves scene/history");
   if(mode=="file-late"){auto next=source.snapshot();next.active().material.color[1]=.123f;d.replace(next);const auto expected=next.encode();denied=false;await([&]{return !DocumentTransactionTestAccess::pending(d);},"old file notice was not retired");check(d.copy_scene().encode()==expected&&d.frame().state->error.empty(),"old authoring failure cannot overwrite replacement source/status");std::cout<<checks<<" file generation checks passed\n";return 0;}
   denied=false;check(d.wait_asset(std::chrono::seconds(5)),"asset busy clears after publication memory recovers");await([&]{return d.frame().state->error=="Original source validation failed before any authoritative publication";},"original file-worker failure was not republished");
   d.edit_scene("Retry authoring",[](Scene&s,const auto&){s.active().material.color[0]=.2f;});check(d.wait_asset()&&d.frame().state->error.empty(),"generic authoring worker remains usable after persistent denial");check(d.undo()&&d.copy_scene().encode()==baseline,"retried authoring remains one exact Undo");
 }
 std::cout<<checks<<" actual "<<mode<<" worker lifecycle checks; "<<observed<<" denied allocations passed\n";return 0;
}catch(const std::exception&e){denied=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
