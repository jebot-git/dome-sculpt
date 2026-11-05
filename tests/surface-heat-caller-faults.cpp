// SPDX-License-Identifier: GPL-3.0-or-later
// SurfaceHeat5 actual direct caller admission faults only. The real helper is gated before entry,
// then cancelled. Full real preparation/publication is covered separately.
#include "dome/document.hpp"
#include "dome/skin-authoring.hpp"
#include "native-weight-retention.hpp"
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <set>
#include <thread>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
thread_local bool tracked=false;thread_local size_t calls=0,fail_at=0;
void*allocate(size_t n,size_t align=0){if(tracked){++calls;if(fail_at&&calls>=fail_at)throw std::bad_alloc();}void*p=nullptr;if(align){if(posix_memalign(&p,align,n?n:1))p=nullptr;}else p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
struct Gate{std::atomic_bool entered=false,release=false;};std::atomic<std::shared_ptr<Gate>> gate;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
void entered(const std::shared_ptr<Gate>&g){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(60);while(!g->entered&&std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(1));check(g->entered,"Actual queued helper entry was not reached");}
struct Drain{Document&d;std::shared_ptr<Gate>g;~Drain(){tracked=false;fail_at=0;g->release=true;d.cancel_background_work();}};
}
void*operator new(size_t n){return allocate(n);}void*operator new[](size_t n){return allocate(n);}void*operator new(size_t n,std::align_val_t a){return allocate(n,size_t(a));}void*operator new[](size_t n,std::align_val_t a){return allocate(n,size_t(a));}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}void operator delete(void*p,size_t,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,size_t,std::align_val_t)noexcept{std::free(p);}
using Cancel=std::function<bool()>;
extern "C" bool real_heat(Scene&,std::span<const uint64_t>,const SkinAuthoringOptions&,const Cancel&) asm("__real__ZN4dome24author_surface_heat_skinERNS_5SceneESt4spanIKmLm18446744073709551615EERKNS_20SkinAuthoringOptionsERKSt8functionIFbvEE");
extern "C" bool wrap_heat(Scene&,std::span<const uint64_t>,const SkinAuthoringOptions&,const Cancel&) asm("__wrap__ZN4dome24author_surface_heat_skinERNS_5SceneESt4spanIKmLm18446744073709551615EERKNS_20SkinAuthoringOptionsERKSt8functionIFbvEE");
extern "C" bool wrap_heat(Scene&s,std::span<const uint64_t>ids,const SkinAuthoringOptions&o,const Cancel&cancel){auto g=gate.load();if(g){g->entered=true;while(!g->release&&!(cancel&&cancel()))std::this_thread::sleep_for(std::chrono::milliseconds(1));}return real_heat(s,ids,o,cancel);}
int main(int argc,char**argv)try{
 if(argc!=3)throw std::invalid_argument("Authentic unskinned complete source, output directory");const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);setenv("XDG_STATE_HOME",(out/"state").c_str(),1);setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
 auto source=Scene::load(argv[1]);uint64_t reference=0;std::vector<uint64_t>ids;for(auto&o:source.objects()){if(o.deformation&&o.deformation->skeleton)reference=o.id;if(o.mesh_active&&o.painted&&o.appearance)ids.push_back(o.id);}check(reference&&ids.size()==19,"Authentic full nineteen-output intent required");
 SkinAuthoringOptions options;options.mode=SkinAuthoringMode::SurfaceHeat;options.reference=reference;options.limits={1,.9f};options.locks.assign(source.object(reference).deformation->skeleton->bones.size(),1);
 auto setup=[&](Document&d){d.replace(source);d.select_objects(ids);check(d.wait_mesh(std::chrono::seconds(120)),"Initial immutable source mesh fence required");};
 auto cancel_save=[&](Document&d,const std::shared_ptr<Gate>&g,const nlohmann::json&before,const std::filesystem::path&file){entered(g);auto f=d.save_async(file);d.cancel_background_work();g->release=true;bool refused=false;try{f.get();}catch(const std::exception&){refused=true;}check(refused&&d.wait_asset(std::chrono::seconds(60))&&d.copy_scene().encode()==before&&!d.frame().state->asset_busy&&!d.frame().state->can_save_after_asset&&!d.frame().state->can_undo,"Real accepted Cancel must fail dependent Save and preserve exact Scene/history");check(!std::filesystem::exists(file),"Cancelled accepted Save must not fabricate a file");};
 size_t total=0;
 {Document d;setup(d);auto before=d.copy_scene().encode();auto g=std::make_shared<Gate>();gate=g;Drain drain{d,g};calls=0;tracked=true;d.bind_skin_selected(options,{},ids);tracked=false;total=calls;check(total>16,"Actual allocating direct caller baseline required");auto state=d.frame().state;check(state->asset_busy&&state->skin_options.mode==SkinAuthoringMode::SurfaceHeat&&state->skin_options.limits.max_influences==1&&state->skin_options.limits.prune==.9f&&state->skin_use_locks&&state->weights.locks==options.locks&&state->skin_options.locks.empty(),"Actual typed configuration/locks must publish with accepted work");cancel_save(d,g,before,out/"baseline-cancelled.dsculpt");}
 std::set<size_t>prefixes;for(size_t n=1;n<=total;n*=2)prefixes.insert(n);for(size_t n=0;n<16;++n)prefixes.insert(total-n);
 size_t passed=0;
 for(auto prefix:prefixes){Document d;setup(d);auto before=d.copy_scene();const auto value=before.encode();const auto state=d.frame().state;auto g=std::make_shared<Gate>();gate=g;Drain drain{d,g};bool refused=false;calls=0;fail_at=prefix;tracked=true;
  try{d.bind_skin_selected(options,{},ids);}catch(const std::bad_alloc&){refused=true;}catch(...){tracked=false;throw;}tracked=false;fail_at=0;
  check(refused&&calls>=prefix,"Measured direct caller denial must actually hit");check(d.frame().state==state&&!state->asset_busy&&!state->can_save_after_asset&&!state->can_undo&&!g->entered,"Failed admission must restore exact published controls/locks/history and queue no work");
  auto current=d.copy_scene();check(current.encode()==value,"Failed direct caller must preserve whole contextual Scene");for(auto&o:before.objects()){const auto&a=current.object(o.id);check(bool(a.painted)==bool(o.painted)&&(!a.painted||(a.painted->mesh_owner()==o.painted->mesh_owner()&&detail::NativeWeightPaintAccess::same_current(*a.painted,*o.painted)))&&a.volume->geometry_owner()==o.volume->geometry_owner()&&detail::NativeWeightVolumeAccess::same_current(*a.volume,*o.volume)&&a.appearance==o.appearance&&a.material_bindings==o.material_bindings&&a.deformation==o.deformation&&a.rig_source==o.rig_source&&a.shape_source==o.shape_source&&a.bake_recipe==o.bake_recipe,"Failed admission replaced an authoritative source owner");}
  // Same controller retry uses the real queue and worker entry, then actual
  // Cancel/accepted Save. It does not stand in for a completed solver run.
  d.bind_skin_selected(options,{},ids);cancel_save(d,g,value,out/("retry-cancelled-"+std::to_string(prefix)+".dsculpt"));++passed;std::cout<<"PASS actual direct caller permanent prefix="<<prefix<<" reached="<<calls<<std::endl;
 }
 std::cout<<"PASS sampled actual direct caller prefixes="<<passed<<" measured="<<total<<"; powers-of-two+last16, exact snapshot/owners/controls/queue/history; same-controller retry+Cancel+accepted Save; not exhaustive helper/publication"<<std::endl;return 0;
}catch(const std::exception&e){tracked=false;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
