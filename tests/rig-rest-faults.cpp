// SPDX-License-Identifier: GPL-3.0-or-later
#define main rig_rest_pose_fixture_main
#include "rig-rest-pose.cpp"
#undef main
#include "dome/document.hpp"
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
namespace {
Scene rest_fixture(){Scene scene;scene.active().painted=std::make_shared<PaintedMesh>(mesh(),64);scene.active().mesh_active=true;DeformationState d(mesh());d.skeleton=rig();d.skin=SkinWeights{{{{0,1}},{{1,1}},{{1,.5f},{2,.5f}}}};for(const auto &b:d.skeleton->bones)d.pose.push_back(b.rest);d.pose[1].rotation=arcball({0,1,0},normalized({.2f,1,.1f}));scene.set_deformation(1,d);return scene;}
}
int main()try{
 test_thread=std::this_thread::get_id();const auto original=rest_fixture();const auto before=original.encode();RigRestJointEdit edit;edit.joint=1;edit.parent=2;auto head=original.active().deformation->skeleton->world()[1];edit.world_position=Vec3{head[12]+.03f,head[13],head[14]};
 auto warm=original;author_rig_rest(warm,1,{&edit,1});auto measured=original;allocation_count=0;fail_at=SIZE_MAX;measure=true;author_rig_rest(measured,1,{&edit,1});measure=false;const auto sites=allocation_count.load();check(sites>20&&sites<20000,"Actual scene rest/bind/source capture allocation phases are bounded and measured");
 for(size_t site=0;site<sites;++site){Scene scene=original;const auto paint=scene.active().painted;const auto deformation=scene.active().deformation;allocation_count=0;fail_at=site;std::exception_ptr failed;measure=true;try{author_rig_rest(scene,1,{&edit,1});}catch(...){failed=std::current_exception();}measure=false;denial=false;if(!failed)throw std::runtime_error("Rest allocation did not fail at measured site "+std::to_string(site));try{std::rethrow_exception(failed);}catch(const std::bad_alloc&){}if(scene.encode()!=before||scene.active().painted!=paint||scene.active().deformation!=deformation||scene.active().rig_source)throw std::runtime_error("Rest failure changed source endpoint");}
 check(true,"Every persistent scene-operation allocation failure preserves exact source and permits retry");
 size_t callbacks=0;auto complete=original;author_rig_rest(complete,1,{&edit,1},{},[&]{++callbacks;return false;});for(size_t at=1;at<=callbacks;++at){auto scene=original;size_t call=0;bool failed=false;try{author_rig_rest(scene,1,{&edit,1},{},[&]{return ++call>=at;});}catch(const std::exception&){failed=true;}if(!failed||scene.encode()!=before)throw std::runtime_error("Cancellation changed rest source");}check(true,"Every measured rest/source-capture cancellation boundary is atomic");
 auto state=complete.encode();allocation_count=0;fail_at=SIZE_MAX;measure=true;auto encoded=complete.encode();measure=false;const auto encodes=allocation_count.load();
 for(size_t site=0;site<encodes;++site){allocation_count=0;fail_at=site;std::exception_ptr failed;measure=true;try{auto output=complete.encode();}catch(...){failed=std::current_exception();}measure=false;denial=false;if(!failed)throw std::runtime_error("Rig encode allocation did not fail");try{std::rethrow_exception(failed);}catch(const std::bad_alloc&){}if(complete.encode()!=state)throw std::runtime_error("Rig encode denial changed exact source");}
 check(true,"Complete Scene25 encode/source DAG cleanup survives every persistent allocation boundary");
 allocation_count=0;fail_at=SIZE_MAX;measure=true;auto decoded=Scene::decode(state);measure=false;const auto decodes=allocation_count.load();for(size_t site=0;site<decodes;++site){allocation_count=0;fail_at=site;std::exception_ptr failed;measure=true;try{auto output=Scene::decode(state);}catch(...){failed=std::current_exception();}measure=false;denial=false;if(!failed)throw std::runtime_error("Rig decode allocation did not fail");try{std::rethrow_exception(failed);}catch(const std::bad_alloc&){}if(complete.encode()!=state)throw std::runtime_error("Rig decode denial changed retained source");}
 check(true,"Complete Scene25 decode/source DAG cleanup survives every persistent allocation boundary");
 {
  Document doc;doc.replace(original);check(doc.wait_mesh(),"Actual rest worker cancellation fixture drains its immutable preview");doc.begin_rig_fit();auto view=*doc.frame().state->rig_fit;view.role=1;view.bone_heads[1].x+=.03f;doc.configure_rig_fit(view);
  {WorkerGate gate(sizeof(Bone)*view.rest_source->skeleton->bones.size());doc.accept_rig_fit();gate.wait();check(doc.asset_status().cancellable,"Actually executing rest/bind worker exposes shared Cancel work");doc.cancel_background_work();gate.release();}
  check(doc.wait_asset()&&doc.copy_scene().encode()==before&&!doc.frame().state->can_undo&&doc.frame().state->error.empty(),"Actual worker Cancel retains complete rig/source endpoint and global history");doc.begin_rig_fit();view=*doc.frame().state->rig_fit;view.role=1;view.bone_heads[1].x+=.03f;doc.configure_rig_fit(view);doc.accept_rig_fit();check(doc.wait_asset()&&doc.copy_scene().active().rig_source&&doc.frame().state->error.empty(),"Same ordered worker retries actual rest fitting after cancellation");
 }
 auto retry=original;check(author_rig_rest(retry,1,{&edit,1})&&retry.encode()==state&&Scene::decode(state).encode()==state,"Successful retry after exhaustive persistent denial preserves exact endpoint/restart");std::cout<<sites<<" rest-operation, "<<encodes<<" encode, "<<decodes<<" decode allocation boundaries; "<<callbacks<<" cancellation boundaries passed\n";return 0;
}catch(const std::exception&e){measure=false;denial=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
