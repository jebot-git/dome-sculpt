// SPDX-License-Identifier: GPL-3.0-or-later
#define main native_paint_functional_reference
#include "native-paint-sampling.cpp"
#undef main
#include <cstdlib>
#include <new>
#include <mutex>
#include <condition_variable>
#include <atomic>
namespace allocation_probe {
thread_local bool active=false;thread_local size_t calls=0,limit=SIZE_MAX,largest=0;
void hit(size_t n){if(active){largest=std::max(largest,n);if(calls++>=limit)throw std::bad_alloc();}}
struct Guard{explicit Guard(size_t at=SIZE_MAX){calls=0;limit=at;largest=0;active=true;}~Guard(){active=false;}};
}
void *operator new(size_t n){allocation_probe::hit(n);if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);} void operator delete[](void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);} void operator delete[](void *p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t a){allocation_probe::hit(n);void *p=nullptr;if(posix_memalign(&p,std::max(size_t(a),sizeof(void*)),n?n:1))throw std::bad_alloc();return p;}
void *operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void *p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void *p,std::align_val_t)noexcept{std::free(p);}
void operator delete(void *p,size_t,std::align_val_t)noexcept{std::free(p);}void operator delete[](void *p,size_t,std::align_val_t)noexcept{std::free(p);}
void *operator new(size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void *operator new[](size_t n,const std::nothrow_t&)noexcept{try{return ::operator new[](n);}catch(...){return nullptr;}}
void *operator new(size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new(n,a);}catch(...){return nullptr;}}
void *operator new[](size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new[](n,a);}catch(...){return nullptr;}}
void operator delete(void*p,const std::nothrow_t&)noexcept{std::free(p);}void operator delete[](void*p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete(void*p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}
namespace publication_gate {std::mutex mutex;std::condition_variable cv;bool armed=false,entered=false,released=false;
void release(){std::lock_guard lock(mutex);released=true;armed=false;cv.notify_all();}
struct Release{~Release(){release();}};
}
extern "C" detail::RetainedNativePaint __real__ZN4dome6detail19retain_native_paintERKNS_12SculptObjectENS_4MeshESt4spanIKjLm18446744073709551615EERKNS_19PaintSamplingLimitsE(const SculptObject &,Mesh,std::span<const uint32_t>,const PaintSamplingLimits &);
extern "C" detail::RetainedNativePaint __wrap__ZN4dome6detail19retain_native_paintERKNS_12SculptObjectENS_4MeshESt4spanIKjLm18446744073709551615EERKNS_19PaintSamplingLimitsE(const SculptObject &s,Mesh m,std::span<const uint32_t> v,const PaintSamplingLimits &l){auto result=__real__ZN4dome6detail19retain_native_paintERKNS_12SculptObjectENS_4MeshESt4spanIKjLm18446744073709551615EERKNS_19PaintSamplingLimitsE(s,std::move(m),v,l);std::unique_lock lock(publication_gate::mutex);if(publication_gate::armed){publication_gate::entered=true;publication_gate::cv.notify_all();publication_gate::cv.wait(lock,[]{return publication_gate::released;});}return result;}
namespace {
void allocation_sweep(const char *name,const auto &operation,const auto &unchanged){
 operation();size_t total=0;{allocation_probe::Guard g;operation();total=allocation_probe::calls;}std::cout<<"ALLOCATIONS "<<name<<' '<<total<<std::endl;check(total>0,"Real entry has measured allocation sites");
 for(size_t p=0;p<total;++p){bool refused=false;{allocation_probe::Guard g(p);try{operation();}catch(const std::bad_alloc&){refused=true;}}check(refused,"Every measured persistent allocation prefix actually refuses");unchanged();}
 operation();unchanged();std::cout<<"COMPLETE "<<name<<' '<<total<<std::endl;
}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);auto original=source();const auto before=original.encode();const auto paint_owner=original.active().painted;const auto geometry=paint_owner->mesh_owner();
 Mesh target;std::vector<uint32_t> vertices;const auto &old=paint_owner->mesh();for(auto v:old.indices){vertices.push_back(v);target.positions.push_back(old.positions[v]);target.normals.push_back(old.normals[v]);const auto uv=old.uv[v];target.uv.push_back({.5f-.7f*(uv[1]-.5f),.5f+.7f*(uv[0]-.5f)});target.indices.push_back(uint32_t(target.indices.size()));}
 auto unchanged=[&]{check(original.encode()==before&&original.active().painted==paint_owner&&paint_owner->mesh_owner()==geometry&&!paint_owner->paint_sampling_domain(),"Refusal preserves exact source Scene and strong native owners");};
 auto factory=[&]{auto r=detail::retain_native_paint(original.active(),target,vertices);r.owner->validate_current(*r.paint);};allocation_sweep("native-factory",factory,unchanged);
 auto retained=detail::retain_native_paint(original.active(),target,vertices);auto current=original;current.active().painted=retained.paint;current.active().material_bindings=detail::bind_native_paint(retained.owner,{});const auto encoded=current.encode();auto decode=[&]{auto s=Scene::decode(encoded);s.validate();};allocation_sweep("native-owning-decoder",decode,unchanged);
 for(int kind=0;kind<4;++kind){PaintSamplingLimits limits;if(kind==0)limits.max_vertices=4;if(kind==1)limits.max_work=1;if(kind==2)limits.max_basis_bytes=1;if(kind==3)limits.max_working_bytes=1;bool refused=false;size_t largest=0;{allocation_probe::Guard g;try{detail::retain_native_paint(original.active(),target,vertices,limits);}catch(const std::length_error&){refused=true;}largest=allocation_probe::largest;}check(refused&&largest<4096,"Native geometry/work/basis/working denial precedes bulk allocation");unchanged();}
 size_t polls=0;PaintSamplingLimits limits;limits.cancelled=[&]{++polls;return false;};detail::retain_native_paint(original.active(),target,vertices,limits);for(size_t p=1;p<=polls;++p){size_t seen=0;limits.cancelled=[&]{return ++seen>=p;};bool refused=false;try{detail::retain_native_paint(original.active(),target,vertices,limits);}catch(const std::exception&e){refused=std::string_view(e.what()).find("cancelled")!=std::string_view::npos;}check(refused,"Every actual native factory cancellation poll refuses");unchanged();}factory();std::cout<<"CANCEL_POLLS "<<polls<<std::endl;
 Document doc;doc.replace(original);ready(doc);const auto file=out/"ordered.dsculpt";doc.save_async(file).get();const auto initial=doc.copy_scene().encode();const auto undo=doc.frame().state->can_undo,redo=doc.frame().state->can_redo;
 doc.begin_uv();check(doc.wait_uv(),"Actual cancellation UV draft ready");doc.edit_uv(UvAction::RetainPaint);doc.edit_uv(UvAction::Pick,2.f/3,1.f/3);doc.edit_uv(UvAction::Scale,.7f);check(doc.wait_uv()&&doc.frame().state->uv->valid,"Actual cancellation UV draft valid");
 publication_gate::Release release;{std::lock_guard lock(publication_gate::mutex);publication_gate::armed=true;publication_gate::entered=false;publication_gate::released=false;}doc.end_uv(true);{std::unique_lock lock(publication_gate::mutex);check(publication_gate::cv.wait_for(lock,std::chrono::seconds(30),[]{return publication_gate::entered;}),"Actual native factory finishes before publication gate");}
 auto save=doc.save_async(file);check(save.wait_for(std::chrono::milliseconds(20))!=std::future_status::ready,"Save is ordered behind real computed UV endpoint");doc.cancel_background_work();publication_gate::release();check(save.wait_for(std::chrono::seconds(30))==std::future_status::ready,"Queued Save drains after accepted UV Cancel");std::string error;try{save.get();}catch(const std::exception&e){error=e.what();}check(error.find("cancelled")!=std::string::npos,"Queued Save reports this exact canceled operation");check(doc.wait_asset()&&doc.wait_mesh(),"Canceled native UV and mesh endpoints drain");check(doc.frame().state->error==error,"Native error and dependent Save share the exact cancellation diagnostic");doc.end_uv(false);check(doc.copy_scene().encode()==initial&&Scene::load(file).encode()==initial&&doc.frame().state->can_undo==undo&&doc.frame().state->can_redo==redo,"Late Cancel preserves exact Scene/file/history after real computation");apply(doc);check(doc.copy_scene().encode()!=initial,"Same native UV input retries after late Cancel");
 std::cout<<"PASS "<<checks<<" native32 allocation/quota/cancellation controls\n";return 0;
}catch(const std::exception&e){allocation_probe::active=false;publication_gate::release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
