// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-target-resolution.hpp"
#include "dome/bake-recipe.hpp"
#include <fstream>
namespace filtered_workflow {
unsigned checks=0;
void require(bool yes,const char *why){if(!yes)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<'\n';}
std::vector<uint8_t> bytes(const std::filesystem::path &path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
struct Family{Scene scene;uint64_t source,target;std::string name;};
Family before(bool legacy,bool live,bool seam,SourceNormalUvMethod method,bool repeated=false){
 auto scene=advanced_fixture::uv_source(advanced_fixture::prepared(legacy,require),method,seam);
 if(repeated){scene=advanced_fixture::uv_source(scene,method,seam);scene=advanced_fixture::uv_source(scene,method,seam);}
 advanced_fixture::pose(scene,live);const auto source=scene.active().id;
 const auto &object=scene.active();auto mesh=object.deformation?object.deformation->preview(object.painted->mesh()):object.painted->mesh();
 if(repeated)for(size_t v=0;v<mesh.uv.size();++v)mesh.uv[v]={object.painted->mesh().positions[v].x+.5f,object.painted->mesh().positions[v].y+.5f};
 const auto destination=scene.add("Explicit resolution Bake destination",Volume{});auto &target=scene.object(destination);
 target.mesh_active=true;target.painted=std::make_shared<PaintedMesh>(mesh,64);target.material_bindings=blank_typed(legacy);scene.select(destination);
 return {std::move(scene),source,destination,std::string(legacy?"mt0":"mt1")+(live?"-posed":"-rest")+(seam?"-seam":"-shared")+(method==SourceNormalUvMethod::RetainSource?"-retained":"-raster")+(repeated?"-three-edits":"")};
}
ObjectBakeSettings settings(){ObjectBakeSettings o;o.prepare_normals=true;o.normal_error=.03f;o.front_mm=o.back_mm=500;o.ao_samples=0;o.padding=0;return o;}
void unchanged_source(const Scene &after,const Scene &before,uint64_t target){auto restored=after.snapshot();restored.object(target)=before.object(target);require(restored.encode()==before.encode(),"Resolution and Bake preserve exact complete original source/image/recipe scene");}
void legacy(const std::filesystem::path &path,const std::filesystem::path &out){
 const auto raw=bytes(path);const auto original=nlohmann::json::from_cbor(raw);auto scene=Scene::load(path);
 require(scene.encode()==original,"Genuine historical SAN native64 fixture loads and reencodes without rewriting any saved source recipe");
 require(scene.active().bake_recipe&&scene.active().bake_recipe->settings.normal_filter_quality==0,"Missing saved policy retains actual legacy per-texel semantics");
 scene.save(out/"legacy-preserved.dsculpt");require(bytes(out/"legacy-preserved.dsculpt")==raw,"Real legacy native file survives byte-exact save");
 const auto old=scene.active().bake_recipe;const auto old_encoded=old->encode();bake_object(scene,old->source.object,old->settings);
 require(scene.active().bake_recipe->encode()==old_encoded&&scene.active().bake_recipe->settings.normal_filter_quality==0,"Actual recalled legacy64 Bake remains accepted under its persisted old policy");
 auto current=old_encoded;current["settings"]["normal_filter_quality"]=1;
 require(ObjectBakeRecipe::decode(current)->encode()==current,"New filtered quality version persists and independently roundtrips");
 for(auto invalid:{nlohmann::json(0),nlohmann::json(2),nlohmann::json(true),nlohmann::json("1")}){auto wrong=current;wrong["settings"]["normal_filter_quality"]=invalid;bool refused=false;try{ObjectBakeRecipe::decode(wrong);}catch(const std::exception&){refused=true;}require(refused,"Unknown, boolean or mislabeled filtered quality policy refuses");}
 require(ObjectBakeSettings{}.normal_filter_quality==1,"New API requests default to the stronger explicit filtered policy");
}
void low_refusal(const Family &family,const std::filesystem::path &folder){
 const auto encoded=family.scene.encode();auto candidate=family.scene.snapshot();const auto own_paint=candidate.active().painted;
 bool refused=false;std::string reason;try{bake_object(candidate,family.source,settings());}catch(const std::exception&e){refused=true;reason=e.what();}
 require(refused&&reason.find("larger target paint resolution")!=std::string::npos,"Original64 retained target is a real sampled-fidelity refusal with an explicit resolution remedy");
 require(candidate.encode()==encoded&&candidate.active().painted==own_paint,"Native fidelity refusal preserves exact detached Scene and target authority");
 Document doc;doc.replace(family.scene.snapshot());require(doc.wait_mesh(),"Original64 refusal Document reaches its genuine input packet");
 const auto path=folder/(family.name+"-prior.dsculpt");doc.save_async(path).get();const auto prior=bytes(path);const auto state=doc.frame().state;
 doc.bake_from_object(family.source,settings());auto save=doc.save_async(path);bool saved=true;try{save.get();}catch(const std::exception&){saved=false;}
 require(doc.wait_asset()&&doc.wait_mesh()&&!saved&&bytes(path)==prior&&doc.copy_scene().encode()==encoded,"Fidelity worker and immediately ordered Save preserve exact prior bytes and complete original64 scene");
 const auto after=doc.frame().state;require(after->can_undo==state->can_undo&&after->can_redo==state->can_redo&&after->project_path==state->project_path&&after->error.find("larger target paint resolution")!=std::string::npos,"Fidelity refusal preserves native history/path and reports recovery remedy");
 std::cout<<"FILTERED64_REFUSAL "<<family.name<<'\n';
}
void probes(const Family &family,const Scene &scene){
 const auto &target=scene.object(family.target),&source=scene.object(family.source);const auto &mesh=target.painted->mesh();
 const auto a=mesh.positions[mesh.indices[0]],ex=mesh.positions[mesh.indices[1]]-a,ey=mesh.positions[mesh.indices[2]]-a;
 const auto center=a+(ex+ey)/3,n=normalized(cross(ex,ey)),x=normalized(ex),y=normalized(cross(n,x));const float span=std::max({length(ex),length(ey),length(ey-ex)})*.85f;
 double maximum=0;for(const auto xy:std::array<std::array<int,2>,4>{{{48,48},{45,47},{49,45},{50,50}}}){
  const auto position=center+x*((xy[0]+.5f)/96*2-1)*span+y*((xy[1]+.5f)/96*2-1)*span,r=position-a;
  const double d00=dot(ex,ex),d01=dot(ex,ey),d11=dot(ey,ey),den=d00*d11-d01*d01;const float v=float((d11*dot(r,ex)-d01*dot(r,ey))/den),w=float((d00*dot(r,ey)-d01*dot(r,ex))/den);const std::array<float,3>bary{1-v-w,v,w};
  require(*std::min_element(bary.begin(),bary.end())>.05f,"Every unchanged original graphical probe remains strictly interior");
  for(size_t role=0;role<2;++role){const auto wanted=advanced_fixture::decode(source,0,bary,role),actual=direction(target,0,bary,role);double error=0;for(size_t c=0;c<3;++c)error=std::max(error,std::abs(wanted[c]-actual[c]));maximum=std::max(maximum,error);require(error<.035,"Explicit higher-resolution genuine Bake passes unchanged .035 at every original role/probe");}
 }
 std::cout<<"FILTERED_RESOLUTION_DIRECTIONS "<<family.name<<" maximum="<<maximum<<'\n';
}
void positive(const Family &family,uint32_t resolution,const std::filesystem::path &folder){
 Document doc;doc.replace(family.scene.snapshot());require(doc.wait_mesh(),"Higher-resolution Document starts from the original genuine64 target");
 const auto initial=doc.copy_scene().encode();require(doc.frame().state->can_resize_bake_target,"Existing target resolution editor is enabled for the genuine unpainted typed target");
 const auto state=doc.frame().state;const AssetSource captured{state->generation,state->revision,family.target,state->edit_authority};
 TextureSetEdit edit;edit.action=TextureSetAction::Resolution;edit.set=0;edit.resolution=resolution;doc.edit_texture_set(edit,captured);
 auto save=doc.save_async(folder/(family.name+"-selected-resolution.dsculpt"));save.get();require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty(),"Existing resolution worker and immediately ordered Save complete before Bake");
 const auto resized=doc.copy_scene();require(resized.object(family.target).painted->mesh_owner()==family.scene.object(family.target).painted->mesh_owner()&&resized.object(family.target).material_bindings==family.scene.object(family.target).material_bindings,"Explicit blank-target resize retains exact geometry and immutable material binding owners");require(resized.object(family.target).painted->resolution()==resolution&&resized.object(family.source).painted->resolution()==64,"Only explicit user-selected target map changes resolution; original source remains64");unchanged_source(resized,family.scene,family.target);
 require(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==initial,"One actual resize Undo restores exact original64 target and source");
 require(doc.redo()&&doc.wait_mesh()&&doc.copy_scene().encode()==resized.encode(),"One actual resize Redo restores exact selected higher-resolution endpoint");
 bool stale=false;try{doc.edit_texture_set(edit,captured);}catch(const std::exception&){stale=true;}require(stale&&doc.copy_scene().encode()==resized.encode(),"Stale resolution authority cannot publish over the accepted selected map");
 doc.bake_from_object(family.source,settings());const auto path=folder/(family.name+"-filtered-resolution-"+std::to_string(resolution)+".dsculpt");auto baked_save=doc.save_async(path);baked_save.get();require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty(),"Higher-resolution filtered Bake and ordered native Save succeed");
 const auto accepted=doc.copy_scene();require(accepted.active().bake_recipe&&accepted.active().bake_recipe->settings.normal_filter_quality==1&&accepted.active().bake_recipe->encode()["settings"]["normal_filter_quality"]==1,"Accepted genuine Bake persists the checked quality version");
 require(Scene::load(path).encode()==accepted.encode(),"Fresh native reopen retains actual filtered Bake images, selected resolution and quality policy");unchanged_source(accepted,family.scene,family.target);probes(family,accepted);
 require(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==resized.encode()&&doc.redo()&&doc.wait_mesh()&&doc.copy_scene().encode()==accepted.encode(),"Bake remains one exact separate Undo/Redo step after explicit resolution selection");
 std::cout<<"FILTERED_RESOLUTION_WORKFLOW "<<family.name<<" target_resolution="<<resolution<<" source_resolution=64\n";
}
void cancellation(uint32_t resolution){
 auto family=before(false,false,false,SourceNormalUvMethod::Rasterize);
 detail::resize_unpainted_bake_target(family.scene,family.target,resolution);
 for(unsigned boundary:{1u,8u,64u}){
  auto candidate=family.scene.snapshot();const auto encoded=candidate.encode();const auto paint=candidate.active().painted;
  bool after_raster=false,requested=false,caught=false;unsigned polls=0;
  try{bake_object(candidate,family.source,settings(),[&]{if(after_raster&&++polls==boundary){requested=true;return true;}return false;},[&](float progress){if(progress>=.8999f)after_raster=true;});}catch(const std::exception&){caught=true;}
  require(after_raster&&requested&&caught&&candidate.encode()==encoded&&candidate.active().painted==paint,"Cancellation after actual normal rasterization preserves exact selected grid, source and unpublished Bake");
 }
}
int run(const std::filesystem::path &folder,const std::filesystem::path &old,uint32_t resolution){
 std::filesystem::create_directories(folder);legacy(old,folder);cancellation(resolution);unsigned positives=0,refusals=0;
 for(bool legacy:{false,true})for(bool live:{false,true})for(bool seam:{false,true})for(auto method:{SourceNormalUvMethod::Rasterize,SourceNormalUvMethod::RetainSource}){
  auto family=before(legacy,live,seam,method);if(method==SourceNormalUvMethod::RetainSource){low_refusal(family,folder);++refusals;}positive(family,resolution,folder);++positives;
 }
 positive(before(false,true,true,SourceNormalUvMethod::RetainSource,true),resolution,folder);++positives;
 require(positives==17&&refusals==8,"All original64 retained refusals and separately named genuine higher-target-resolution families execute");
 std::cout<<"PASS "<<checks<<" filtered Bake workflow positives="<<positives<<" original64_refusals="<<refusals<<" target_resolution="<<resolution<<"\n";return 0;
}
} // namespace filtered_workflow
