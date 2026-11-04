// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-color-target-fixture.hpp"
#include "dome/document.hpp"
#include "dome/uv.hpp"
#include "dome/source-uv-bake.hpp"
#include "dome/color-preparation.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <numeric>
namespace allocation {
thread_local bool active=false,persistent=true;thread_local long remaining=-1;thread_local size_t calls=0,hits=0;
void visit(){if(!active)return;++calls;if(remaining==0){++hits;if(!persistent)remaining=-1;throw std::bad_alloc();}if(remaining>0)--remaining;}
void begin(long n,bool deny=true){calls=hits=0;remaining=n;persistent=deny;active=true;}void stop(){active=false;}
}
void *operator new(size_t n){allocation::visit();if(auto p=std::malloc(std::max(n,size_t(1))))return p;throw std::bad_alloc();}void *operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t align){allocation::visit();void*p=nullptr;if(posix_memalign(&p,size_t(align),std::max(n,size_t(1))))throw std::bad_alloc();return p;}void*operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}void operator delete(void*p,size_t,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,size_t,std::align_val_t)noexcept{std::free(p);}

using namespace dome;
namespace {
void require(bool v,const char*s){if(!v)throw std::runtime_error(s);}void ready(Document&d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Caller UV worker: "+d.frame().state->error);}
void draft(Document&d){d.begin_uv();require(d.wait_uv(),"Caller UV init");const auto mesh=d.frame().state->uv->mesh;size_t face=0;while(face<mesh->indices.size()/3&&mesh->normals[mesh->indices[face*3]].z<.9f)++face;std::array<float,2> uv{};for(size_t k=0;k<3;++k){auto q=mesh->uv[mesh->indices[face*3+k]];uv[0]+=q[0]/3;uv[1]+=q[1]/3;}d.edit_uv(UvAction::Pick,uv[0],uv[1]);require(d.wait_uv(),"Caller UV Pick");d.edit_uv(UvAction::Move,1.f/64,0);require(d.wait_uv()&&d.frame().state->uv->valid,"Caller UV actual changed draft");}
}
int main()try{
 auto initial=current_color_target_fixture::fixture(true);Document prepared;prepared.replace(initial);ObjectBakeSettings settings;settings.prepare_colors=true;settings.ao_samples=0;prepared.bake_from_object(initial.objects().back().id,settings);ready(prepared);const auto source=prepared.copy_scene();const auto before=source.encode();
 size_t count=0; nlohmann::json accepted;
 {Document measure;measure.replace(source);ready(measure);draft(measure);allocation::begin(-1);measure.end_uv(true);count=allocation::calls;allocation::stop();ready(measure);accepted=measure.copy_scene().encode();require(accepted.at("version")==30&&measure.copy_scene().active().bake_recipe==source.active().bake_recipe&&source.active().bake_recipe&&measure.copy_scene().active().material_bindings->color_preparation->uv_reprojection,"Actual caller keeps its genuine Bake recipe and accepted UV23 reprojection in Scene30");}
 std::cout<<"MEASURE "<<count<<" actual caller admission allocations"<<std::endl;
 size_t refused=0,optional=0;
 for(size_t boundary=0;boundary<count;++boundary){Document d;d.replace(source);ready(d);draft(d);const auto mesh=d.frame().state->uv->mesh;const auto topology=d.frame().state->uv->topology;const auto draft_undo=d.frame().state->can_undo;
  bool failed=false;allocation::begin(long(boundary));try{d.end_uv(true);}catch(const std::bad_alloc&){failed=true;}catch(...){allocation::stop();throw;}auto hits=allocation::hits;allocation::stop();require(hits>0,"Actual caller allocation boundary was not reached");
  if(failed){++refused;auto state=d.frame().state;require(!state->asset_busy&&!state->can_save_after_asset&&state->can_undo==draft_undo&&state->uv&&state->uv->mesh==mesh&&state->uv->topology==topology&&!state->uv->committing,"Caller admission OOM must retain ready draft/lease without stuck committing or phantom accepted Save");require(d.copy_scene().encode()==before,"Caller OOM changed actual source/paint/history");d.end_uv(true);}else ++optional;
  ready(d);require(d.copy_scene().encode()==accepted,"Caller retry differs from actual accepted colour UV endpoint");require(d.undo()&&d.copy_scene().encode()==before&&!d.frame().state->can_undo&&d.redo()&&d.copy_scene().encode()==accepted,"Caller retry is exactly one Undo/Redo and retains complete input");
  std::cout<<"BOUNDARY "<<boundary<<std::endl;
 }
 require(source.encode()==before,"Caller tests changed immutable source");std::cout<<"PASS "<<count<<" measured caller admission boundaries refusals="<<refused<<" optional="<<optional<<std::endl;return 0;
}catch(const std::exception&e){allocation::stop();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
