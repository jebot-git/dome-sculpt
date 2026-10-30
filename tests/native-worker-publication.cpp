// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <semaphore>
#include <thread>
#include <unistd.h>
using namespace dome;
namespace dome {struct DocumentTransactionTestAccess {
 static std::thread::id worker(Document&d){return d.native_thread_.get_id();}
 static bool pending(Document&d){return d.native_notice_pending_.load();}
 static bool no_history(Document&d){std::lock_guard lock(d.model_mutex_);return d.undo_.empty()&&d.redo_.empty();}
};}
namespace {
std::thread::id target;
std::atomic_bool denied=false,block=false;
std::atomic<size_t> observed=0;
std::binary_semaphore entered(0),release(0);
size_t checks=0;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;}
template<class F>void await(F f,const char*m){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(8);while(!f()){if(std::chrono::steady_clock::now()>end)throw std::runtime_error(m);std::this_thread::sleep_for(std::chrono::milliseconds(2));}}
Mesh quad(){Mesh m;m.positions={{-.3f,-.3f,0},{.3f,-.3f,0},{.3f,.3f,0},{-.3f,.3f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
NativeStrokeSample ray(){NativeStrokeSample s;s.origin={0,0,1};s.direction={0,0,-1};s.brush.radius=.1f;return s;}
}
void*operator new(size_t n){if(std::this_thread::get_id()==target){if(block.exchange(false)){entered.release();release.acquire();}if(denied.load()){++observed;throw std::bad_alloc();}}if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main(int argc,char**argv)try{
 const bool replace=argc>1&&std::string(argv[1])=="replace";
 const bool held=argc>1&&std::string(argv[1])=="held";
 const auto home=std::filesystem::temp_directory_path()/("dome-native-oom-"+std::to_string(getpid()));std::filesystem::create_directories(home);setenv("HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);
 struct Cleanup{std::filesystem::path p;~Cleanup(){denied=false;std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{home};
 Scene scene;scene.active().painted=std::make_shared<PaintedMesh>(quad(),64);scene.active().mesh_active=true;
 if(held){auto a=AffineTransform{}.matrix();a[0]=-2;a[4]=.3;scene.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(a));}
 Document d;d.replace(scene);check(d.wait_mesh(),"Source is ready");const auto before=d.copy_scene().encode();target=DocumentTransactionTestAccess::worker(d);
 struct ReleaseGate{bool opened=false;void open(){if(!opened){opened=true;release.release();}}~ReleaseGate(){open();}}gate;
 SpatialFrame frame;
 auto send=[&]{++frame.sequence;frame.time+=.05;d.submit(frame);check(d.wait_input(),"Actual held frame drains");};
 if(held){d.configure_object_grab(true,true);frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};for(auto &h:frame.hands)h.tracked.valid=true;frame.hands[0].tracked.grip.position={0,1.4f,-.3f};send();frame.hands[1].tracked.grip.position=ob::transform(d.frame().state->canvas,{0,0,0});frame.hands[1].tracked.grip.orientation=d.frame().state->canvas.orientation;send();frame.hands[0].tracked.squeeze=1;send();check(d.frame().state->object_grabbing,"Actual physical affine grip starts before worker denial");}
 denied=block=true;
 if(held){frame.hands[1].tracked.trigger=1;send();check(d.frame().state->native_pending,"Actual held contact gesture reaches native worker");}
 else check(d.begin_native_stroke(),"Real native gesture admitted before worker denial");
 entered.acquire();
 if(held){frame.hands[0].tracked.squeeze=0;send();}
 else {check(d.sample_native_stroke(ray()),"Sample queued while target worker is blocked");d.end_native_stroke();}
 auto path=home/"ordered.dsculpt";auto saved=d.save_async(path);
 gate.open();check(d.wait_native_stroke(),"Native worker completes failed endpoint under persistent target-thread denial");
 check(observed>0,"Actual native worker allocation was denied");
 if(held)check(!d.frame().state->object_grabbing||DocumentTransactionTestAccess::pending(d),"Held failure cleanup is published or retained for allocation-safe publication");
 check(saved.wait_for(std::chrono::seconds(8))==std::future_status::ready,"Accepted endpoint Save cannot hang after worker failure");bool caught=false;try{saved.get();}catch(const std::bad_alloc&){caught=true;}check(caught,"Accepted Save retains original native bad_alloc without constructing a new diagnostic exception");
 check(!std::filesystem::exists(path),"Failure does not write earlier endpoint as accepted Save");check(d.copy_scene().encode()==before,"Failure preserves authoritative source exactly");check(DocumentTransactionTestAccess::no_history(d),"Failure preserves authoritative history even while diagnostic publication is denied");
 auto expected=before;
 if(replace){auto next=scene.snapshot();next.active().material.color[0]=.123f;d.replace(next);expected=next.encode();}
 denied=false;
 await([&]{return !DocumentTransactionTestAccess::pending(d);},"Native diagnostic retries after memory recovery");
 if(replace)check(d.copy_scene().encode()==expected&&d.frame().state->error.empty(),"Old native failure cannot overwrite replacement generation/status");
 else {check(!d.frame().state->error.empty()&&!d.frame().state->can_undo&&!d.frame().state->native_pending,"Native error and completed pending/history flags are eventually published");d.report_error({});}
 check(d.wait_mesh()&&d.begin_native_stroke()&&d.sample_native_stroke(ray()),"Native worker accepts retry after recovery");d.end_native_stroke();check(d.wait_native_stroke()&&d.wait_mesh(),"Retried stroke drains");check(d.copy_scene().encode()!=expected,"Retried stroke really paints");check(d.undo()&&d.copy_scene().encode()==expected,"Retry produces one exact Undo");
 std::cout<<checks<<" actual native-thread allocation recovery checks; "<<observed<<" denied allocations\n";return 0;
}catch(const std::exception&e){denied=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
