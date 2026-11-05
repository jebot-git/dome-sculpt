// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/skin-authoring.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace dome;
#include <cstdlib>
#include <new>
namespace {
thread_local bool tracked=false;thread_local size_t calls=0,deny=0;
void*allocation(size_t n,size_t align=0){if(tracked){++calls;if(deny&&calls>=deny)throw std::bad_alloc();}void*p=nullptr;if(align){if(posix_memalign(&p,align,n?n:1))p=nullptr;}else p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
}
void*operator new(size_t n){return allocation(n);}void*operator new[](size_t n){return allocation(n);}void*operator new(size_t n,std::align_val_t a){return allocation(n,size_t(a));}void*operator new[](size_t n,std::align_val_t a){return allocation(n,size_t(a));}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}void operator delete(void*p,size_t,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,size_t,std::align_val_t)noexcept{std::free(p);}
int main(int argc,char**argv)try{
 if(argc!=3)return 2;const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);const auto source=Scene::load(argv[1]);source.validate();const auto before=source.encode();std::vector<uint64_t>targets;uint64_t reference=0;for(const auto&o:source.objects()){if(o.locked&&o.deformation&&o.deformation->skeleton)reference=o.id;else if(o.mesh_active&&o.painted&&!o.locked)targets.push_back(o.id);}check(reference&&targets.size()==2,"Genuine mixed fixed+generic native source required");SkinAuthoringOptions options;options.reference=reference;auto expected=source;calls=0;tracked=true;bool changed=author_surface_heat_skin(expected,targets,options,{});tracked=false;const auto total=calls;check(changed&&total>64&&total<20000,"Real mixed-component allocating factory baseline required");size_t blended=0;for(auto id:targets)for(const auto&row:expected.object(id).deformation->skin->vertices)blended+=row.size()>1;check(blended>0,"Actual mixed fixture must execute blended heat solve");const auto complete=expected.encode();
 for(size_t prefix=1;prefix<=total;++prefix){auto scene=source;calls=0;deny=prefix;bool hit=false;tracked=true;try{(void)author_surface_heat_skin(scene,targets,options,{});}catch(const std::bad_alloc&){hit=true;}catch(...){tracked=false;throw;}tracked=false;deny=0;check(hit&&calls>=prefix&&scene.encode()==before,"Every actual new-path permanent allocation prefix must preserve the complete original scene");for(const auto&o:source.objects()){const auto&n=scene.object(o.id);check(n.deformation==o.deformation&&n.rig_source==o.rig_source&&n.shape_source==o.shape_source&&n.painted->mesh_owner()==o.painted->mesh_owner()&&n.material_bindings==o.material_bindings&&n.appearance==o.appearance,"Actual failed mixed-component factory replaced an authoritative owner");}check(author_surface_heat_skin(scene,targets,options,{})&&scene.encode()==complete,"Same mixed recipient must really retry to the identical mixed field/capsule after recovery");std::cout<<"PASS mixed factory permanent prefix="<<prefix<<" measured="<<total<<std::endl;}
 source.save(out/"before.dsculpt");expected.save(out/"bound.dsculpt");std::ofstream(out/"report.json")<<nlohmann::json{{"all_actual_factory_prefixes",total},{"preparation_replayed",false},{"scene_atomic",true},{"same_recipient_real_retry",true}}.dump(2);return 0;
}catch(const std::exception&e){tracked=false;deny=0;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
