// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <new>
#include <unistd.h>
using namespace dome;
namespace {std::atomic_bool tracking=false,hit=false;std::atomic<size_t> calls=0,boundary=0;std::thread::id target;std::atomic_bool release_gate=true;}
void *operator new(size_t bytes){if(tracking.load(std::memory_order_acquire)&&std::this_thread::get_id()==target){const auto n=++calls;if(n==1)while(!release_gate.load())std::this_thread::yield();if(boundary&&n>=boundary){hit=true;throw std::bad_alloc();}}if(auto p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
void operator delete(void *p)noexcept{std::free(p);}void operator delete(void *p,size_t)noexcept{std::free(p);}
namespace dome {struct DocumentNativeTestAccess {
 static auto request(const Document &d){return d.projection_request_.load();}
 static auto file_thread(const Document &d){return d.file_thread_.get_id();}
 static auto load(const Document &d){std::lock_guard lock(d.model_mutex_);return d.projection_load_;}
 static auto history(const Document &d){std::lock_guard lock(d.model_mutex_);return std::array<uint64_t,5>{d.content_,d.next_content_,d.saved_content_,d.undo_.size(),d.redo_.size()};}
};}
namespace {
void require(bool b,const char*m){if(!b)throw std::runtime_error(m);}
Scene scene(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};Scene s;s.active().painted=std::make_shared<PaintedMesh>(m,64);s.active().mesh_active=true;return s;}
auto setup(){auto d=std::make_unique<Document>();d->replace(scene());require(d->wait_mesh(),"Initial scene ready");return d;}
void drain(const auto &task){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!task->done.load()&&std::chrono::steady_clock::now()<end)std::this_thread::yield();require(task->done,"Image worker completed under denial");}
}
int main()try{
 const auto root=std::filesystem::temp_directory_path()/("dome-project-allocation-"+std::to_string(getpid()));std::filesystem::create_directories(root);setenv("HOME",root.c_str(),1);setenv("XDG_STATE_HOME",root.c_str(),1);setenv("XDG_CONFIG_HOME",root.c_str(),1);struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{root};
 const auto path=root/"image.png";auto encoded=Image{1,1,{255,0,0,255}}.png();{std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(encoded.data()),encoded.size());}
 auto measured=setup();target=DocumentNativeTestAccess::file_thread(*measured);calls=0;boundary=0;release_gate=false;tracking=true;measured->load_projection_image(path);const auto measured_task=DocumentNativeTestAccess::load(*measured);release_gate=true;drain(measured_task);require(measured->wait_projection_image(),"Measured image load finished");tracking=false;const size_t total=calls.load();require(total>8,"Real image worker allocation coverage measured");measured.reset();
 for(size_t n=1;n<=total;++n){auto d=setup();const auto before=d->copy_scene().encode();const auto history=DocumentNativeTestAccess::history(*d);target=DocumentNativeTestAccess::file_thread(*d);calls=0;boundary=n;hit=false;release_gate=false;tracking=true;d->load_projection_image(path);const auto task=DocumentNativeTestAccess::load(*d);release_gate=true;require(bool(task),"Pending immutable image task captured");drain(task);require(hit,"Every selected image worker allocation boundary was denied persistently");require(d->copy_scene().encode()==before&&DocumentNativeTestAccess::history(*d)==history,"Persistent image denial preserves exact authoritative Scene/history");
  // The file worker has a retained exception and may retry, but must stay alive
  // without allocating in the failure path. The independent model remains usable.
  auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(30);while(std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
  tracking=false;boundary=0;require(d->wait_projection_image(),"Error publication resumes once memory returns");require(d->frame().state->projection&&!d->frame().state->projection->loading&&!d->frame().state->projection->error.empty(),"Image error status clears pending view after memory recovery");
  d->load_projection_image(path);require(d->wait_projection_image()&&d->frame().state->projection->image,"Same native file worker accepts image retry after persistent denial");require(d->copy_scene().encode()==before&&DocumentNativeTestAccess::history(*d)==history,"Successful image retry still changes no Scene/history");
 }

 std::cout<<total<<" actual image-worker persistent allocation boundaries passed\n";
 auto admission=setup();admission->load_projection_image(path);require(admission->wait_projection_image(),"Old immutable image ready for admission proof");target=std::this_thread::get_id();calls=0;boundary=0;tracking=true;admission->load_projection_image(path);tracking=false;const size_t main_total=calls.load();require(admission->wait_projection_image(),"Measured replacement image completes");admission.reset();
 for(size_t n=1;n<=main_total;++n){auto d=setup();d->load_projection_image(path);require(d->wait_projection_image(),"Prior image ready");const auto before=d->copy_scene().encode();const auto history=DocumentNativeTestAccess::history(*d);const auto published=d->frame().state;const auto request=DocumentNativeTestAccess::request(*d);target=std::this_thread::get_id();calls=0;boundary=n;hit=false;tracking=true;bool refused=false;try{d->load_projection_image(path);}catch(const std::bad_alloc&){refused=true;}tracking=false;boundary=0;require(hit&&refused,"Every admission allocation boundary fails explicitly");require(d->frame().state==published&&DocumentNativeTestAccess::request(*d)==request,"Failed admission preserves exact immutable old image/view and cancellation nonce");require(d->copy_scene().encode()==before&&DocumentNativeTestAccess::history(*d)==history,"Failed image admission preserves exact Scene/history");d->load_projection_image(path);require(d->wait_projection_image()&&d->frame().state->projection->image,"Image replacement retries after admission denial");}
 std::cout<<main_total<<" actual main-thread transactional image admission boundaries passed\n";return 0;
}catch(const std::exception&e){release_gate=true;tracking=false;boundary=0;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
