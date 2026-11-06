// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/source-maps.hpp"
#include "dome/source-paint.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/paint-sampling.hpp"
#include "polygon-scene-archive.hpp"
#include "source-placement-archive.hpp"
#include "baked-normal-archive.hpp"
#include "material-archive-version.hpp"
#include <iostream>
#include <cstdlib>
namespace {thread_local int64_t denial=-1;thread_local bool watching=false,denied=false;thread_local size_t calls=0;void allocation(){if(watching)++calls;if(denial==0){denied=true;throw std::bad_alloc();}if(denial>0)--denial;}}
void *operator new(size_t n){allocation();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void *operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;
namespace {
size_t checks=0;void check(bool value,const char *why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>bool refuses(F f,std::string_view message={}){try{f();return false;}catch(const std::exception &e){return std::string_view(e.what()).find(message)!=std::string_view::npos;}}
nlohmann::json paint_value(const std::shared_ptr<const PaintedMesh> &paint){
 if(!paint)return nullptr;PaintImageArchive images;detail::BakedNormalArchive fields(images);NormalSamplingArchive normals;PaintSamplingArchive domains;
 auto payload=paint->encode(&images,&normals,&domains);auto normal=normals.encode(images);
 return {{"paint",std::move(payload)},{"images",images.encode()},{"normals",std::move(normal)},{"domains",domains.encode()}};
}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const auto scene=Scene::load(argv[1]);const auto encoded=scene.encode();
 check(Scene::decode(encoded).encode()==encoded,"Full existing owning Scene preflight/decoder accepts the actual input");
 const auto &j=encoded.at("polygon_sources");check(!j.at("origins").empty(),"Reader fixture owns genuine nonempty shape/rig source history");
 auto images=PaintImageArchive::decode(encoded.at("source_images"));detail::BakedNormalArchive fields(encoded.contains("baked_normal_fields")?&encoded.at("baked_normal_fields"):nullptr,images);
 auto models=CreatorAssetArchive::decode(encoded.at("source_models"),images);detail::PolygonColorArchive colors;
 if(j.contains("color_bases"))colors=detail::PolygonColorArchive::decode(j.at("color_bases"),j.at("color_fields"));
 NormalSamplingArchive normals;if(encoded.contains("normal_sampling"))normals=NormalSamplingArchive::decode(encoded.at("normal_sampling"),images);
 PaintSamplingArchive domains;if(encoded.contains("paint_sampling"))domains=PaintSamplingArchive::decode(encoded.at("paint_sampling"));
 const SourcePaintDecodeContext context{false,detail::physical_material_archive(encoded.at("source_materials"))};
 auto materials=SourceMaterialArchive::decode(encoded.at("source_materials"),images,models,j.contains("color_bases")?&colors:nullptr,encoded.contains("normal_sampling")?&normals:nullptr,encoded.contains("paint_sampling")?&domains:nullptr,context);
 detail::SourcePlacementArchive placements;if(encoded.contains("affine_state"))placements=detail::SourcePlacementArchive::decode(encoded.at("affine_state"),models);
 auto old=detail::PolygonSceneArchive::decode(j,images,models,materials,&placements,j.contains("color_bases")?&colors:nullptr,encoded.contains("normal_sampling")?&normals:nullptr,encoded.contains("paint_sampling")?&domains:nullptr,context);
 size_t prefix=materials.bindings().size();
 auto resolver=[&](uint32_t id){if(id>=prefix)throw std::invalid_argument("Actual material prefix is not complete");return materials.at(id);};
 auto reader=[&](const nlohmann::json &input,std::function<bool()> cancelled={}){return detail::PolygonSceneReader(input,images,models,resolver,&placements,j.contains("color_bases")?&colors:nullptr,encoded.contains("normal_sampling")?&normals:nullptr,encoded.contains("paint_sampling")?&domains:nullptr,context,std::move(cancelled));};
 auto lazy=reader(j);for(size_t i=old.origins().size();i>0;--i){const auto &o=lazy.origin_at(uint32_t(i-1));check(o==lazy.origin_at(uint32_t(i-1)),"Repeated before ID preserves one actual strong origin owner");}
 auto current=lazy.finish();check(current.origins().size()==old.origins().size()&&current.authorities().size()==old.authorities().size(),"Lazy completion retains exact declared pool counts");
 for(size_t i=0;i<old.origins().size();++i){const auto &a=*old.origins()[i];const auto &b=*current.origins()[i];
  check(a.source_object==b.source_object&&a.name==b.name&&a.mesh_active==b.mesh_active&&a.material_bindings==b.material_bindings,"Source IDs and actual immutable material authority do not relabel or clone");
  check(a.volume->encode()==b.volume->encode()&&paint_value(a.paint)==paint_value(b.paint),"Every before volume, Paint float/layer/bake/domain/image value equals existing owning decoder");
  check((a.deformation?a.deformation->encode(a.paint->mesh()):nlohmann::json(nullptr))==(b.deformation?b.deformation->encode(b.paint->mesh()):nlohmann::json(nullptr)),"Exact before skin/morph/pose16 values survive lazy resolution");
  for(size_t k=0;k<old.origins().size();++k){check((a.shape_source==old.origins()[k])==(b.shape_source==current.origins()[k]),"Actual first-shape source aliases retain exact pool identity");check((a.rig_source==old.origins()[k])==(b.rig_source==current.origins()[k]),"Actual first-rig source aliases retain exact pool identity");}
 }
 size_t owned=0;while(owned<j.at("origins").size()&&j.at("origins")[owned].at("material_bindings").is_null())++owned;
 check(owned<j.at("origins").size(),"Actual history includes its own prepared material reference");
 prefix=0;auto retry=reader(j);check(refuses([&]{retry.origin_at(uint32_t(owned));},"prefix"),"Incomplete actual material prefix refuses before-origin construction");prefix=materials.bindings().size();const auto expected=retry.origin_at(uint32_t(owned));check(bool(expected),"Exception retry resolves the genuine same record after prefix completion");
 const auto binding=expected->material_bindings;
 auto foreign=std::shared_ptr<const SourceMaterialBindings>(binding.get(),[](auto*){});
 retry.material_resolver([&](uint32_t id){auto actual=materials.at(id);return actual==binding?foreign:actual;});
 check(refuses([&]{retry.origin_at(uint32_t(owned));},"exact owning binding"),"Completed hit must reject same-address foreign material control block");retry.material_resolver(resolver);retry.finish();
 auto cyclic=j;cyclic["origins"][0]["shape_source"]=0;check(refuses([&]{auto cycle=reader(cyclic);cycle.origin_at(0);},"cycle"),"Actual recursive origin cycle refuses before completion");
 auto deep=j;deep["origins"]=nlohmann::json::array();for(size_t i=0;i<33;++i){auto node=j.at("origins")[0];node["polygon"]=nullptr;node["shape_source"]=i==32?nlohmann::json(nullptr):nlohmann::json(i+1);if(node.contains("rig_source"))node["rig_source"]=nullptr;deep["origins"].push_back(std::move(node));}
 auto revisited=reader(deep);revisited.origin_at(31);check(refuses([&]{revisited.origin_at(0);},"bound"),"A shallow completed shared child cannot hide an over-depth origin path");
 size_t polls=0;{auto measured=reader(j,[&]{++polls;return false;});measured.finish();}
 for(size_t p=0;p<polls;++p){size_t at=0;check(refuses([&]{auto cancelled=reader(j,[&]{return at++>=p;});cancelled.finish();},"cancelled"),"Every measured owning-reader cancellation leaves source unchanged");}
 calls=0;watching=true;{auto measured=reader(j);measured.finish();}watching=false;const auto count=calls;
 for(size_t p=0;p<count;++p){denied=false;denial=int64_t(p);bool failed=false;try{auto fail=reader(j);fail.finish();}catch(...){failed=true;}denial=-1;check(denied&&failed,"Every measured persistent reader allocation prefix unwinds without publishing a graph");}
 {auto again=reader(j);again.finish();}check(scene.encode()==encoded,"All retry/foreign-owner/cancel/OOM reads leave complete original source exact");
 std::cout<<"PASS "<<checks<<" lazy before-origin checks; origins="<<old.origins().size()<<" cancellations="<<polls<<" allocation_prefixes="<<count<<'\n';return 0;
}catch(const std::exception &e){denial=-1;watching=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
