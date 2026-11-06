// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-surface-codec.hpp"
#include "dome/scene-file.hpp"
#include "dome/paint-sampling.hpp"
#include "dome/material-bindings.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstdlib>
#include <new>
namespace probe {
thread_local bool denied=false;thread_local size_t calls=0;
struct NoAllocation {NoAllocation(){calls=0;denied=true;}~NoAllocation(){denied=false;}};
void hit(){if(denied){++calls;throw std::bad_alloc();}}
}
void*operator new(size_t n){probe::hit();if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
void*operator new(size_t n,std::align_val_t a){probe::hit();void*p=nullptr;if(!posix_memalign(&p,std::max(sizeof(void*),size_t(a)),n?n:1))return p;throw std::bad_alloc();}void*operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}void operator delete(void*p,size_t,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,size_t,std::align_val_t)noexcept{std::free(p);}
using namespace dome;using namespace dome::detail;
namespace {
size_t checks=0;void check(bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);}
bool refuses(auto f,std::string_view s){try{f();return false;}catch(const std::exception&e){return std::string_view(e.what()).find(s)!=std::string_view::npos;}}
void actual(Scene scene){const auto id=scene.active().id;const auto before=scene.encode();auto owner=PolygonSurfaceOwner::capture(scene,id);PolygonSurfaceStep move;move.edit.operation=PolygonOperation::Move;move.edit.selection.mode=PolygonSelectionMode::Vertex;move.edit.selection.ids={owner->trace().current()->vertices().back().id};move.edit.delta={0,0,.15f};auto staged=owner->advance(move)->stage_paint();
 std::shared_ptr<const PolygonOrigin> recorded;const auto record=encode_polygon_surface_node(staged.owner,[&](auto p){recorded=p;return 0;});check(recorded==staged.owner->before(),"Node writer submits actual full before root to enclosing origin pool");
 size_t entries=0;{probe::NoAllocation guard;entries=preflight_polygon_surface_node(record,1);}check(entries==1&&probe::calls==0,"Complete valid node preflight makes no allocation before any decoder/candidate work");
 // This carrier is a genuine legacy native Scene with a full before-only
 // origin, not a claim that Scene39/current Paint6 is already admitted.
 auto carrier=scene.snapshot();const auto dummy=carrier.add("Actual codec before-root carrier",Volume{});auto polygon=std::make_shared<PolygonAuthority>();polygon->origin=recorded;carrier.object(dummy).polygon=polygon;carrier.select(id);const auto encoded=carrier.encode();auto recovered=Scene::decode(encoded);check(recovered.encode()==encoded,"Actual native codec restores every source/rig/key/F32/before-root record exactly");const auto decoded_origin=recovered.object(dummy).polygon->origin;
 size_t reads=0;auto read=[&](uint32_t n){++reads;if(n)throw std::out_of_range("Actual private origin table has exactly one reference");return decoded_origin;};auto decoded=decode_polygon_surface_node(record,1,read);check(reads==1&&decoded.owner->before()==decoded_origin,"Owning node reader uses the actual separately decoded native before-root control block");const auto domain=decoded.paint->paint_sampling_domain();const auto previous=decoded_origin->paint->paint_sampling_domain();const auto basis=previous?previous->basis():decoded_origin->paint->mesh_owner();check(domain->version()==3&&domain->basis()==basis&&!domain->basis().owner_before(basis)&&!basis.owner_before(domain->basis()),"Declared basis witness restores actual before native/grid alias, never equal-value owner merging");check(decoded.binding->polygon_surface_transfer==decoded.owner&&decoded.owner->matches_current(*decoded.paint),"Actual replay reconstructs current binding/paint attachment from authentic source");


 PolygonSurfaceNodeWriter writer;check(writer.add(staged.owner)==0&&writer.add(staged.owner)==0&&writer.size()==1,"Writer deduplicates only genuine identical owning node control blocks");
 const auto written=writer.encode([&](auto root){check(root==recorded,"Writer resolves actual source roots after collecting the bounded node set");return 0;});check(written.at("nodes").size()==1&&written.at("nodes")[0]==record,"Pool writer preserves the exact genuine node record");
 auto borrowed=std::shared_ptr<const PolygonSurfaceOwner>(staged.owner.get(),[](const auto*){});check(refuses([&]{writer.add(borrowed);},"genuine connected owner"),"Same-address foreign node control block cannot enter writer pool");
 auto pool=nlohmann::json{{"version",1},{"nodes",nlohmann::json::array({record,record})}};
 PolygonSurfaceNodeReader reader(pool,1,read);check(refuses([&]{reader.require_complete();},"Unreferenced"),"Every encoded owning node must be consumed");
 const auto&first=reader.at(0);const auto reserved=reader.reserved_working_bytes();check(reader.at(0).owner==first.owner&&reader.reserved_working_bytes()==reserved,"Completed replay retains actual owner identity without reserving a second raster");
 check(reader.at(1).owner!=first.owner,"Distinct declared nodes are not merged by equal recipe or mesh values");reader.require_complete();
 auto foreign_origin=std::make_shared<const PolygonOrigin>(*decoded_origin);bool foreign=false;
 PolygonSurfaceNodeReader context(pool,1,[&](uint32_t){return foreign?foreign_origin:decoded_origin;});(void)context.at(0);foreign=true;check(refuses([&]{context.at(0);},"exact owning context"),"Completed node cannot borrow equal-valued foreign source control block");foreign=false;(void)context.at(0);
 auto bad_pool=pool;PolygonSurfaceTransferLimits small;small.maximum_entries=1;check(refuses([&]{PolygonSurfaceNodeReader too_many(bad_pool,1,read,small);},"aggregate edit-entry"),"Whole pool edit budget is preflighted before any source resolver");
 PolygonSurfaceNodeReader*active=nullptr;bool cycle=true;PolygonSurfaceNodeReader cyclic(pool,1,[&](uint32_t){if(cycle)(void)active->at(0);return decoded_origin;});active=&cyclic;check(refuses([&]{cyclic.at(0);},"cycle"),"Actual resolver reentry refuses before a completed-node shortcut");cycle=false;check(cyclic.at(0).owner!=nullptr,"Exception clears recursion state and leaves authentic replay retry usable");
 pool["nodes"][0]["recipe"]["operations"][0]["delta"][2]=.16f;check(refuses([&]{reader.at(0);},"exact owning context"),"Legally mutable encoded recipe cannot reuse a completed owning witness");pool["nodes"][0]=record;reader.require_complete();
 const auto world=scene.world_matrix(id);for(size_t t=0;t<decoded.paint->mesh().indices.size()/3;++t){const std::array<double,3>b{.2,.3,.5};check(decoded.owner->paint_coordinates(t,b)==staged.owner->paint_coordinates(t,b)&&decoded.owner->normal_coordinates(t,b)==staged.owner->normal_coordinates(t,b),"Owning replay preserves independent writable/normal source streams");check(length(decoded.owner->pbr_normal(t,b,world)-staged.owner->pbr_normal(t,b,world))<1e-7f,"Native before-root roundtrip preserves actual transformed field sample");}
 auto bad=record;bad["before_origin"]=1;reads=0;check(refuses([&]{decode_polygon_surface_node(bad,1,read);},"reference/scalar range")&&reads==0,"Bad origin references fail before resolver or replay allocation");bad=record;bad["basis"]=previous?"captured-native-grid":"captured-retained-grid";check(refuses([&]{decode_polygon_surface_node(bad,1,read);},"basis witness differs"),"Wrong source-basis relationship cannot be inferred from matching values");bad=record;bad["version"]=2;reads=0;check(refuses([&]{decode_polygon_surface_node(bad,1,read);},"shape/version")&&reads==0,"Unsupported node format refuses before any source decode");bad=record;bad["recipe"]["operations"][0]["world"]={1,2,3};check(refuses([&]{preflight_polygon_surface_node(bad,1);},"world dimensions"),"Malformed world witnesses fail in complete preallocation scan");bad=record;bad["recipe"]["operations"][0]["selection"]["ids"].push_back(9);PolygonSurfaceTransferLimits limits;limits.maximum_entries=1;check(refuses([&]{preflight_polygon_surface_node(bad,1,limits);},"aggregate edit-entry"),"Aggregate edit metadata limit is charged before copies");
 check(scene.encode()==before&&recovered.encode()==encoded,"Every new owning codec/control leaves actual original and decoded native sources exact");
}
}
int main(int argc,char**argv)try{if(argc!=3)throw std::invalid_argument("Expected genuine shear/reflection native fixtures");for(int n=1;n<argc;++n)actual(load_scene_file(argv[n]).scene);std::cout<<"PASS "<<checks<<" private owning polygon node controls; no Scene39/Paint6/publication admission\n";return 0;}catch(const std::exception&e){probe::denied=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
