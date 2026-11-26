// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-target-resolution.hpp"
#include "dome/bake-recipe.hpp"
#include <fstream>
#include <type_traits>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
namespace filtered_workflow {
unsigned checks=0;
void require(bool yes,const char *why){
 if(!yes)throw std::runtime_error(why);
 ++checks;std::cout<<"PASS "<<why<<'\n';
 // Opt-in observation only: retain every operation, guard and dataset while
 // locating expensive phases in the full-resolution sanitizer workflow.
 if(std::getenv("DOME_TEST_FILTERED_TIMING")){
  using Clock=std::chrono::steady_clock;
  static const auto start=Clock::now();
  std::cerr<<"FILTERED_TIMING seconds="
           <<std::chrono::duration<double>(Clock::now()-start).count()
           <<" check="<<checks<<" phase="<<why<<'\n';
 }
}
std::vector<uint8_t> bytes(const std::filesystem::path &path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
bool byte_equal_files(const std::filesystem::path &a,const std::filesystem::path &b){
 std::ifstream left(a,std::ios::binary),right(b,std::ios::binary);if(!left||!right)return false;
 std::array<char,65536> x,y;
 for(;;){left.read(x.data(),x.size());right.read(y.data(),y.size());const auto n=left.gcount();
  if(n!=right.gcount()||!std::equal(x.begin(),x.begin()+n,y.begin())||left.bad()||right.bad())return false;
  if(n!=std::streamsize(x.size()))return left.eof()&&right.eof();}
}
void reopen_memory(const char *phase){
 std::ifstream status("/proc/self/status");std::string line;std::cout<<"FRESH_REOPEN_MEMORY phase="<<phase;
 while(std::getline(status,line))if(line.starts_with("VmSize:")||line.starts_with("VmRSS:")||line.starts_with("VmPeak:"))std::cout<<" ["<<line<<']';std::cout<<std::endl;
}
int reopen_file(const std::filesystem::path &input,const std::filesystem::path &output){
 reopen_memory("child-start");{auto reopened=Scene::load(input);reopen_memory("child-loaded");reopened.save(output);reopen_memory("child-saved");}
 require(byte_equal_files(input,output),"Independent fresh-process native reopen and re-save preserve every byte");reopen_memory("child-closed");return 0;
}
void fresh_reopen(const std::filesystem::path &input,const std::filesystem::path &output){
 reopen_memory("parent-after-Document-close");auto exe=std::filesystem::read_symlink("/proc/self/exe").string();std::string mode="--filtered-reopen",from=input.string(),to=output.string();
 std::array<char*,5>args{exe.data(),mode.data(),from.data(),to.data(),nullptr};pid_t child=-1;
 const int error=posix_spawn(&child,exe.c_str(),nullptr,nullptr,args.data(),::environ);
 require(error==0,"Fresh native reopen spawns the exact current executable under inherited AS/resource bounds");int status=0;pid_t waited;
 do{waited=waitpid(child,&status,0);}while(waited<0&&errno==EINTR);
 require(waited==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0,"Owned independent fresh-process native reopen completes successfully");
}
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
  for(size_t role=0;role<2;++role){
   D wanted;
   if(source.material_bindings&&source.material_bindings->normal_preparation)wanted=advanced_fixture::decode(source,0,bary,role);
   else{
    // Independent ordinary plane oracle: its source UV0 and declared data
    // texture sampler are explicit, with no retained/prepared frame authority.
    std::array<double,2> uv{};const auto &raw=source.painted->mesh();for(size_t k=0;k<3;++k)for(size_t axis=0;axis<2;++axis)uv[axis]+=raw.uv[raw.indices[k]][axis]*bary[k];
    const auto maps=source.painted->source_maps();if(maps&&maps->normal.image){const auto &view=maps->normal;require(view.texcoord==0&&view.uv_transform==std::array<float,6>{1,0,0,1,0,0}&&view.wrap_s==33071&&view.wrap_t==33071&&view.mag_filter==9729,"Independent generic oracle authenticates its actual clamp/linear UV0 normal sampler");const auto q=sample(*view.image,uv);wanted=unit(D{(q[0]*2-1)*maps->normal_scale,(q[1]*2-1)*maps->normal_scale,q[2]*2-1});}else wanted={0,0,1};
   }
   const auto actual=direction(target,0,bary,role);double error=0;for(size_t c=0;c<3;++c)error=std::max(error,std::abs(wanted[c]-actual[c]));maximum=std::max(maximum,error);require(error<.035,"Explicit higher-resolution genuine Bake passes unchanged .035 at every original role/probe");}
 }
 std::cout<<"FILTERED_RESOLUTION_DIRECTIONS "<<family.name<<" maximum="<<maximum<<'\n';
}
void positive(const Family &family,uint32_t resolution,const std::filesystem::path &folder,bool recover_after_refusal=false){
 const auto path=folder/(family.name+"-filtered-resolution-"+std::to_string(resolution)+".dsculpt");
 const auto accepted_path=folder/(family.name+"-accepted-memory.dsculpt"),reopened_path=folder/(family.name+"-reopened.dsculpt");
 { // Close the completed live Document and every retained endpoint before a
   // fresh independent reopen; never multiply whole2048 rasters for a test.
 Document doc;doc.replace(family.scene.snapshot());require(doc.wait_mesh(),"Higher-resolution Document starts from the original genuine64 target");
 const auto initial=doc.copy_scene().encode();
 if(recover_after_refusal){
  const auto prior_path=folder/(family.name+"-recover-prior.dsculpt");doc.save_async(prior_path).get();const auto prior=bytes(prior_path);const auto history=doc.frame().state;
  doc.bake_from_object(family.source,settings());auto refused_save=doc.save_async(prior_path);bool failed=false;try{refused_save.get();}catch(const std::exception&){failed=true;}
  require(failed&&doc.wait_asset()&&doc.wait_mesh()&&doc.copy_scene().encode()==initial&&bytes(prior_path)==prior&&doc.frame().state->can_undo==history->can_undo&&doc.frame().state->can_redo==history->can_redo&&doc.frame().state->error.find("larger target paint resolution")!=std::string::npos,"The same actual Document first refuses64 and preserves exact scene, history and prior Save before recovery");
 }
 require(doc.frame().state->can_resize_bake_target,"Existing target resolution editor is enabled for the genuine unpainted typed target");
 const auto state=doc.frame().state;const AssetSource captured{state->generation,state->revision,family.target,state->edit_authority};
 TextureSetEdit edit;edit.action=TextureSetAction::Resolution;edit.set=0;edit.resolution=resolution;doc.edit_texture_set(edit,captured);
 auto save=doc.save_async(folder/(family.name+"-selected-resolution.dsculpt"));save.get();require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty(),"Existing resolution worker and immediately ordered Save complete before Bake");
 const auto resized=doc.copy_scene();require(resized.object(family.target).painted->mesh_owner()==family.scene.object(family.target).painted->mesh_owner()&&resized.object(family.target).material_bindings==family.scene.object(family.target).material_bindings,"Explicit blank-target resize retains exact geometry and immutable material binding owners");require(resized.object(family.target).painted->resolution()==resolution&&resized.object(family.source).painted->resolution()==64,"Only explicit user-selected target map changes resolution; original source remains64");unchanged_source(resized,family.scene,family.target);
 require(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==initial,"One actual resize Undo restores exact original64 target and source");
 require(doc.redo()&&doc.wait_mesh()&&doc.copy_scene().encode()==resized.encode(),"One actual resize Redo restores exact selected higher-resolution endpoint");
 bool stale=false;try{doc.edit_texture_set(edit,captured);}catch(const std::exception&){stale=true;}require(stale&&doc.copy_scene().encode()==resized.encode(),"Stale resolution authority cannot publish over the accepted selected map");
 doc.bake_from_object(family.source,settings());auto baked_save=doc.save_async(path);baked_save.get();require(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty(),"Higher-resolution filtered Bake and ordered native Save succeed");
 const auto accepted=doc.copy_scene();require(accepted.active().bake_recipe&&accepted.active().bake_recipe->settings.normal_filter_quality==1&&accepted.active().bake_recipe->encode()["settings"]["normal_filter_quality"]==1,"Accepted genuine Bake persists the checked quality version");
 accepted.save(accepted_path);require(byte_equal_files(path,accepted_path),"Ordered Save matches every native byte of the actual accepted filtered Scene, images, source and policy");unchanged_source(accepted,family.scene,family.target);probes(family,accepted);
 require(doc.undo()&&doc.wait_mesh()&&doc.copy_scene().encode()==resized.encode()&&doc.redo()&&doc.wait_mesh()&&doc.copy_scene().encode()==accepted.encode(),"Bake remains one exact separate Undo/Redo step after explicit resolution selection");
 }
 fresh_reopen(path,reopened_path);
 require(byte_equal_files(path,reopened_path),"Fresh native reopen and re-save retain every actual filtered Bake/source/image/recipe byte");
 std::cout<<"FILTERED_RESOLUTION_WORKFLOW "<<family.name<<" target_resolution="<<resolution<<" source_resolution=64\n";
}
Family ordinary_plane(bool mapped){
 Scene scene;auto &source=scene.active();Mesh mesh;mesh.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};mesh.normals.assign(4,{0,0,1});mesh.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};mesh.indices={0,1,2,0,2,3};source.mesh_active=true;
 if(mapped){auto maps=std::make_shared<PaintSourceMaps>();auto im=std::make_shared<Image>();im->width=im->height=64;im->rgba.resize(64*64*4);for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x){const auto at=(y*64+x)*4;im->rgba[at]=128;im->rgba[at+1]=std::array<uint8_t,4>{64,112,160,208}[x%4];im->rgba[at+2]=245;im->rgba[at+3]=255;}maps->normal.image=im;maps->normal.wrap_s=maps->normal.wrap_t=33071;maps->normal.min_filter=maps->normal.mag_filter=9729;source.painted=capture_source_sampling(mesh,maps,64);}
 else source.painted=std::make_shared<PaintedMesh>(mesh,64);
 const auto id=source.id;auto low=mesh;if(mapped)for(auto &q:low.uv){const auto u=q[0];q={.2f+.6f*q[1],.8f-.6f*u};}
 const auto target=scene.add("Ordinary source explicit target",Volume{});auto &o=scene.object(target);o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(low,64);o.material_bindings=blank_typed(false);scene.select(target);
 return {std::move(scene),id,target,mapped?"unbound-normalmap":"unbound-plain-mesh"};
}
void resize_limits(){
 auto family=ordinary_plane(true);const auto before=family.scene.encode();const auto paint=family.scene.active().painted;
 TextureSetLimits old;bool work=false;try{detail::resize_unpainted_bake_target(family.scene,family.target,2048,old);}catch(const std::length_error&e){work=std::string(e.what()).find("work budget")!=std::string::npos;}
 require(work&&family.scene.encode()==before&&family.scene.active().painted==paint,"Original64M resize-work budget genuinely refuses2048 atomically at unchanged accounting");
 auto bytes=detail::bake_target_resolution_limits();bytes.maximum_bytes=1;bool bounded=false;try{detail::resize_unpainted_bake_target(family.scene,family.target,2048,bytes);}catch(const std::length_error&){bounded=true;}
 require(bounded&&family.scene.encode()==before&&family.scene.active().painted==paint,"Calibrated2048 resize retains actual byte-budget rejection and exact original authority");
 for(unsigned boundary:{1u,2u,8u,64u}){auto limits=detail::bake_target_resolution_limits();unsigned polls=0;limits.cancelled=[&]{return ++polls>=boundary;};bool refused=false;try{detail::resize_unpainted_bake_target(family.scene,family.target,2048,limits);}catch(const std::runtime_error&){refused=true;}
  require(refused&&polls>=boundary&&family.scene.encode()==before&&family.scene.active().painted==paint,"Every selected early/raster resize cancellation keeps the exact original Scene and paint authority");}
 auto selected=family.scene.snapshot();detail::resize_unpainted_bake_target(selected,family.target,1024);const auto baseline=selected.encode();const auto selected_paint=selected.active().painted;bool insufficient=false;
 try{bake_object(selected,family.source,settings());}catch(const std::invalid_argument&e){insufficient=std::string(e.what()).find("1024 px; choose a larger target paint resolution")!=std::string::npos;}
 require(insufficient&&selected.encode()==baseline&&selected.active().painted==selected_paint,"Actual1024 mapped-source fidelity refusal remains a regression with unchanged normal data and angular limit");
}
void ordinary_sources(const std::filesystem::path &folder,uint32_t resolution){
 static_assert(!std::is_convertible_v<detail::GenericBakeNormalSampler,detail::SourceBakeNormalResolver>);
 auto plain=ordinary_plane(false);require(!plain.scene.object(plain.source).material_bindings&&!detail::source_bake_normals(plain.scene.object(plain.source),{}),"Genuine ordinary source has no original/prepared material-frame resolver");
 auto plane=plain.scene.snapshot();bake_object(plane,plain.source,settings());require(plane.active().bake_recipe->settings.normal_filter_quality==1,"Ordinary mesh to prepared-target Bake records an actually checked filtered policy");probes(plain,plane);unchanged_source(plane,plain.scene,plain.target);
 auto raw=plain.scene.snapshot();auto legacy=settings();legacy.prepare_normals=false;bake_object(raw,plain.source,legacy);require(raw.active().bake_recipe->settings.normal_filter_quality==0&&!raw.active().bake_recipe->encode()["settings"].contains("normal_filter_quality"),"Unprepared raw Bake explicitly retains legacy semantics and never claims filtered preparation");
 auto mapped=ordinary_plane(true);require(!mapped.scene.object(mapped.source).material_bindings&&!detail::source_bake_normals(mapped.scene.object(mapped.source),{}),"Actual ordinary original-normal texture does not borrow paired frame authority");low_refusal(mapped,folder);positive(mapped,resolution,folder,true);
 Scene sculpt;sculpt.active().volume=std::make_shared<Volume>(.03f);sculpt.active().volume->begin_stroke();Brush brush;brush.radius=.4f;brush.blend=0;sculpt.active().volume->stamp(brush);sculpt.active().volume->end_stroke();const auto original=sculpt.active().volume->geometry_owner();
 const auto source=sculpt.active().id;Mesh patch;patch.positions={{-.08f,-.08f,.35f},{.08f,-.08f,.35f},{.08f,.08f,.35f},{-.08f,.08f,.35f}};patch.normals.assign(4,{0,0,1});patch.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};patch.indices={0,1,2,0,2,3};
 const auto target=sculpt.add("Actual sculpt prepared target",Volume{});sculpt.object(target).mesh_active=true;sculpt.object(target).painted=std::make_shared<PaintedMesh>(patch,64);sculpt.select(target);const auto before=sculpt.snapshot();
 require(!sculpt.object(source).mesh_active&&!sculpt.object(source).material_bindings,"Actual sculpt source has neither painting nor material-frame adapter");bake_object(sculpt,source,settings());require(sculpt.active().bake_recipe->settings.normal_filter_quality==1&&sculpt.object(source).volume->geometry_owner()==original,"Actual sculpt-to-prepared-target Bake uses filtered generic normals and preserves exact volume authority");unchanged_source(sculpt,before,target);
 const std::array<float,3>bary{.2f,.3f,.5f};Vec3 position{};for(size_t k=0;k<3;++k)position+=patch.positions[patch.indices[k]]*bary[k];const auto expected=unit(D{position.x,position.y,std::sqrt(.16-position.x*position.x-position.y*position.y)});for(size_t role=0;role<2;++role){const auto actual=direction(sculpt.active(),0,bary,role);double error=0;for(size_t c=0;c<3;++c)error=std::max(error,std::abs(expected[c]-actual[c]));require(error<.035,"Independent spherical SDF direction agrees at the actual generic sculpt projection");}
 sculpt.save(folder/"ordinary-sculpt-filtered.dsculpt");require(Scene::load(folder/"ordinary-sculpt-filtered.dsculpt").encode()==sculpt.encode(),"Actual generic sculpt filtered result and checked policy survive native reopen");
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
int run(const std::filesystem::path &folder,const std::filesystem::path &old,uint32_t resolution,uint32_t ordinary_resolution){
 std::filesystem::create_directories(folder);legacy(old,folder);resize_limits();ordinary_sources(folder,ordinary_resolution);cancellation(resolution);unsigned positives=0,refusals=0;
 for(bool legacy:{false,true})for(bool live:{false,true})for(bool seam:{false,true})for(auto method:{SourceNormalUvMethod::Rasterize,SourceNormalUvMethod::RetainSource}){
  auto family=before(legacy,live,seam,method);if(method==SourceNormalUvMethod::RetainSource){low_refusal(family,folder);++refusals;}positive(family,resolution,folder,method==SourceNormalUvMethod::RetainSource);++positives;
 }
 positive(before(false,true,true,SourceNormalUvMethod::RetainSource,true),resolution,folder);++positives;
 require(positives==17&&refusals==8,"All original64 retained refusals and separately named genuine higher-target-resolution families execute");
 std::cout<<"PASS "<<checks<<" filtered Bake workflow positives="<<positives<<" original64_refusals="<<refusals<<" target_resolution="<<resolution<<" ordinary_target_resolution="<<ordinary_resolution<<"\n";return 0;
}
} // namespace filtered_workflow
