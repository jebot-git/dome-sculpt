// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-paint-sampling.hpp"
#include "canonical-paint-prospect.hpp"
#include "polygon-conversion.hpp"
#include "source-paint-pairing.hpp"
#include <iostream>
#include <numeric>
#include <cstdlib>
namespace {thread_local int64_t deny=-1;thread_local bool observing=false,denied=false;thread_local size_t calls=0;void allocation(){if(observing)++calls;if(deny==0){denied=true;throw std::bad_alloc();}if(deny>0)--deny;}}
void *operator new(size_t n){allocation();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void *operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;
namespace {
size_t checks=0;void check(bool yes,const char *why){++checks;if(!yes)throw std::runtime_error(why);}
template<class F>bool refuses(F f,std::string_view part={}){try{f();return false;}catch(const std::exception&e){return std::string_view(e.what()).find(part)!=std::string_view::npos;}}
size_t entries(const nlohmann::json &j){if(j.is_null())return 0;size_t n=j.at("pose").size()+j.at("morph_weights").size();for(const auto &p:j.at("pose"))if(p.contains("full_local_matrix"))n+=16;if(!j.at("skin").is_null())for(const auto &v:j.at("skin").at("vertices"))n+=v.size();if(!j.at("morphs").is_null())for(const auto &t:j.at("morphs").at("targets"))n+=t.at("delta").size();return n;}
size_t encoded_entries(const nlohmann::json &j){size_t n=0;if(j.contains("source_materials"))n+=SourceMaterialArchive::deformation_entries(j.at("source_materials"));for(const auto &o:j.at("objects"))n+=entries(o.at("deformation"));if(j.contains("polygon_sources"))for(const auto &o:j.at("polygon_sources").at("origins"))n+=entries(o.at("deformation"));if(j.contains("baked_normal_fields"))for(const auto &o:j.at("baked_normal_fields").at("origins"))n+=entries(o.at("deformation"));return n;}
std::pair<Mesh,std::vector<uint32_t>> target(const Mesh &source,bool split){Mesh out;std::vector<uint32_t> map;if(split){for(auto v:source.indices){map.push_back(v);out.positions.push_back(source.positions[v]);out.normals.push_back(source.normals[v]);out.uv.push_back(source.uv[v]);out.indices.push_back(uint32_t(out.indices.size()));}}else{out=source;map.resize(source.positions.size());std::iota(map.begin(),map.end(),0);}for(auto &uv:out.uv){const auto v=uv;uv={.5f-.7f*(v[1]-.5f),.5f+.7f*(v[0]-.5f)};}return {std::move(out),std::move(map)};}
bool exact_layers(const PaintedMesh&a,const PaintedMesh&b){if(a.resolution()!=b.resolution()||a.layers().size()!=b.layers().size()||a.active_layer()!=b.active_layer())return false;for(size_t i=0;i<a.layers().size();++i){const auto &x=a.layers()[i],&y=b.layers()[i];if(x.id!=y.id||x.name!=y.name||x.visible!=y.visible||x.opacity!=y.opacity||x.tiles!=y.tiles||x.pbr_tiles!=y.pbr_tiles||x.emission_tiles!=y.emission_tiles)return false;}return true;}
void boundaries(){
 Scene plain;auto &o=plain.active();Mesh mesh;mesh.positions.resize(31250);mesh.normals.assign(31250,{0,0,1});mesh.uv.assign(31250,{0,0});mesh.positions[1]={1,0,0};mesh.positions[2]={0,1,0};mesh.uv[1]={1,0};mesh.uv[2]={0,1};mesh.indices={0,1,2};
 o.painted=std::make_shared<PaintedMesh>(mesh,64);o.mesh_active=true;
 auto model=std::make_shared<DeformationState>(mesh);model->morphs=MorphSet(mesh);for(size_t i=0;i<128;++i){MorphTarget t;t.name="quota-"+std::to_string(i);t.delta.resize(mesh.positions.size());model->morphs->targets.push_back(std::move(t));}model->validate(mesh);o.deformation=model;
 check(model->resource_entries()==4'000'000,"Genuine 128-target model has exact four-million count");
 const auto at=detail::preflight_canonical_paint_models(plain,o.id,4'000'000);
 check(at.deformation_entries==8'000'000&&at.deformation_slots==2,"Exact-cap current and new before slots retain both models");
 check(refuses([&]{detail::preflight_canonical_paint_models(plain,o.id,4'000'001);},"before allocation"),"Cap plus one must refuse before copying future model arrays");
 // The two source fields share ONE actual pooled origin; equal model pointers
 // in different inline current/origin slots still count separately.
 auto captured=capture_polygon_origin(plain,o.id);plain.active().shape_source=captured;plain.active().rig_source=captured;
 check(refuses([&]{detail::preflight_canonical_paint_models(plain,o.id,1);},"before allocation"),"Shared shape/rig root is pooled once; prospective current still charges its real extra entry");
 const auto shared=detail::preflight_canonical_paint_models(plain,o.id,0);
 check(shared.deformation_entries==8'000'000&&shared.deformation_slots==3,"Replacement excludes old current slot while actual shared source root is charged once");
 // No Scene-wide COW copy or JSON encoding is required to predict rejection.
 std::cout<<"BOUNDARY entries="<<at.deformation_entries<<" shared="<<shared.deformation_entries<<std::endl;
}
}
int main(int argc,char **argv)try{
 if(argc<2||argc>3)return 2;const auto source=Scene::load(argv[1]);const auto original=source.encode();const auto &o=source.active();
 const auto usage=detail::preflight_canonical_paint_models(source,o.id,o.deformation?o.deformation->resource_entries():0);
 const auto expected=encoded_entries(original)+(o.deformation?o.deformation->resource_entries():0);
 std::cout<<"ARCHIVE_COUNT planned="<<usage.deformation_entries<<" expected="<<expected<<" slots="<<usage.deformation_slots<<std::endl;
 check(usage.deformation_entries==expected,"Before-capture planner equals real owning serialized inline model slots");
 for(bool split:{false,true}){auto [mesh,map]=target(o.painted->mesh(),split);auto result=detail::CanonicalPaintSampling::create(source,o.id,mesh,map);
  check(result.owner->valid_owner(result.owner)&&result.owner->matches_current(*result.paint),"Only genuine immutable factory owner authenticates current writable attachment");
  auto foreign=std::shared_ptr<const detail::CanonicalPaintSampling>(result.owner.get(),[](auto*){});check(!result.owner->valid_owner(foreign),"Same-address foreign control block cannot borrow factory certificate");
  const auto &before=*result.owner->before();check(before.paint.get()!=o.painted.get()&&exact_layers(*before.paint,*o.painted)&&before.paint->baked()==o.painted->baked(),"Captured Paint metadata is isolated while actual F32/RGBA/PBR/baked planes stay exact");
  check(before.material_bindings==o.material_bindings&&before.deformation==o.deformation&&before.rig_source==o.rig_source&&before.shape_source==o.shape_source&&before.bake_recipe==o.bake_recipe&&before.polygon==o.polygon&&before.source_object==o.id,"Complete normal/model/first-rig/first-shape/recipe/polygon/source roots remain actual original authorities");
  check(exact_layers(*result.paint,*o.painted)&&result.paint->baked()==o.painted->baked()&&result.owner->normal_uv()->origin()==o.material_bindings->canonical_attributes,"Independent paint and baked normal planes never rerasterize during genuine owning transition");
  if(o.deformation){check(bool(result.deformation)==true,"Current model remap remains present");check(result.deformation->encode(mesh).at("pose")==o.deformation->encode(o.painted->mesh()).at("pose")&&result.deformation->morph_weights==o.deformation->morph_weights,"Current exact pose and morph mixture stay unchanged");if(o.deformation->morphs)for(size_t t=0;t<o.deformation->morphs->targets.size();++t)for(size_t v=0;v<map.size();++v){const auto a=result.deformation->morphs->targets[t].delta[v],b=o.deformation->morphs->targets[t].delta[map[v]];check(a.x==b.x&&a.y==b.y&&a.z==b.z,"Every genuine morph delta broadcasts by exact vertex map");}if(o.deformation->skin)for(size_t v=0;v<map.size();++v)check(result.deformation->skin->vertices[v]==o.deformation->skin->vertices[map[v]],"Actual per-vertex skin/locks retain exact source correspondence");}
  auto independent=detail::CanonicalPaintSampling::create(source,o.id,mesh,map);check(!result.owner->matches_current(*independent.paint),"Equal-valued foreign geometry/domain owner cannot borrow current attachment");
  result.paint->layer_properties(result.paint->layers().back().name,false,.1f);check(exact_layers(*before.paint,*o.painted)&&!exact_layers(*result.paint,*before.paint),"Real current layer edit cannot mutate immutable full before Paint");result.owner->validate_current(*result.paint);
  auto unbound=std::make_shared<SourceMaterialBindings>(*o.material_bindings);unbound->canonical_uv=result.owner->normal_uv();check(refuses([&]{detail::source_paint_pairing(result.paint.get(),unbound.get(),nullptr,true);},"own current-field material authority"),"Standalone owner does not numerically or implicitly grant Scene37 publication");
  PaintSamplingLimits measured;size_t polls=0;measured.cancelled=[&]{++polls;return false;};detail::CanonicalPaintSampling::create(source,o.id,mesh,map,measured);for(size_t p=0;p<polls;++p){size_t at=0;auto cancel=measured;cancel.cancelled=[&]{return at++>=p;};check(refuses([&]{detail::CanonicalPaintSampling::create(source,o.id,mesh,map,cancel);},"cancel"),"Every measured owning factory cancellation refuses unchanged source");}std::cout<<"CANCEL split="<<split<<" polls="<<polls<<std::endl;
  if(argc==3){calls=0;observing=true;detail::CanonicalPaintSampling::create(source,o.id,mesh,map);observing=false;const auto n=calls;for(size_t p=0;p<n;++p){denied=false;deny=int64_t(p);bool failed=false;try{detail::CanonicalPaintSampling::create(source,o.id,mesh,map);}catch(...){failed=true;}deny=-1;check(denied&&failed,"Every measured persistent allocation prefix aborts owning transition");check(source.encode()==original,"All factory failure prefixes retain exact native source graph and float bytes");}detail::CanonicalPaintSampling::create(source,o.id,mesh,map);std::cout<<"OOM split="<<split<<" prefixes="<<n<<std::endl;}
 }
 check(source.encode()==original,"Owning detached factories never change current Scene/source/history");if(argc==3)boundaries();std::cout<<"PASS "<<checks<<" canonical paint full-before owner checks\n";return 0;
}catch(const std::exception&e){deny=-1;observing=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
