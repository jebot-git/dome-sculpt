// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/import-authoring.hpp"
#include "dome/material-bindings.hpp"
#include "dome/humanoid-creator.hpp"
#include "import-authoring-fixtures.hpp"
#include <iostream>
using namespace dome;
namespace {
int checks=0;
void check(bool ok,const char *text){if(!ok)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<'\n';}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);
 for(bool implicit:{false,true})for(bool flat:{false,true}){
  import_fixture::Fixture fixture;auto &primitive=fixture.j["meshes"][0]["primitives"][0];
  while(fixture.bin.size()%4)fixture.bin.push_back(0);
  auto uv=fixture.values({0,0,0,0,0,0,0,0},2,"VEC2");primitive["attributes"]["TEXCOORD_0"]=uv;
  if(implicit)primitive.erase("material");else fixture.j["materials"][0]["pbrMetallicRoughness"].erase("baseColorTexture");
  if(flat){primitive["attributes"].erase("NORMAL");primitive["targets"][0].erase("NORMAL");}
  auto label=std::string(implicit?"implicit-":"authored-")+(flat?"flat":"artist");auto path=directory/(label+".glb");fixture.save(path);
  Scene original;auto before=original.encode();ImportAuthoringOptions choices;choices.paint_resolution=64;
  auto raw=prepare_authoring_import(original,path,choices);
  check(raw.objects.size()==2&&original.encode()==before,"Unprepared degenerate unused UVs are retained as view-only source without changing the caller");
  for(auto id:raw.objects){auto paint=raw.scene.object(id).painted->snapshot();const auto encoded=paint->encode();bool refused=false;try{paint->begin_stroke();}catch(const std::logic_error&){refused=true;}
   check(refused&&!paint->in_stroke()&&paint->encode()==encoded&&!paint->surface_diagnostics().covered_texels&&!paint->surface_diagnostics().unpaintable_uv_faces.empty(),"Raw degenerate coordinates cannot start a writable stroke or fabricate coverage");}
  Document raw_document;raw_document.replace(raw.scene);raw_document.wait_mesh();const auto raw_state=raw_document.copy_scene().encode();
  check(!raw_document.begin_native_stroke()&&raw_document.copy_scene().encode()==raw_state&&raw_document.frame().state->message.find("editable texture atlas")!=std::string::npos,"Actual native degenerate-source stroke explains preparation before changing source or history");
  choices.prepare_texture_atlas=true;auto result=prepare_authoring_import(original,path,choices);
  check(result.objects.size()==2&&original.encode()==before,"Explicit atlas generation admits both untextured source instances without changing caller");
  for(auto id:result.objects){const auto &object=result.scene.object(id);check(object.painted->surface_diagnostics().unpaintable_uv_faces.empty(),"Generated current atlas provides writable coverage for every actual triangle");
   check(*object.appearance->original_bytes==fixture.bytes()&&std::all_of(object.appearance->asset->primitives[0].mesh.uv.begin(),object.appearance->asset->primitives[0].mesh.uv.end(),[](auto q){return q==std::array<float,2>{0,0};}),"Raw original bytes and unused degenerate source coordinates remain exact immutable provenance");
   check(object.material_bindings&&!object.material_bindings->sampling_reparameterized&&object.painted->source_sampling_prepared()&&object.painted->source_maps()->empty()&&object.deformation&&object.deformation->morphs,"Generated UVs coexist with retained frame identity and current editable morphs");
  }
  auto encoded=result.scene.encode();check(Scene::decode(encoded).encode()==encoded,"Exact atlas and original source survive a native restart");
  Document document;document.replace(original);ImportSettings options;options.paint_resolution=64;options.prepare_texture_atlas=true;document.import_asset(path,options);
  check(document.wait_asset()&&document.wait_mesh()&&document.frame().state->error.empty(),"Actual native worker accepts explicit unused-UV atlas preparation");
  auto accepted=document.copy_scene().encode();
  auto current=document.copy_scene();const auto &active=current.active();auto geometry=active.deformation?active.deformation->preview(active.painted->mesh()):active.painted->mesh();
  auto local=geometry.positions[geometry.indices[0]]*.15f+geometry.positions[geometry.indices[1]]*.1f+geometry.positions[geometry.indices[2]]*.75f;
  auto world=current.world_matrix(active.id).point({local.x,local.y,local.z});Vec3 point{float(world[0]),float(world[1]),float(world[2])};auto normal=document.gradient(point);
  PaintSettings ink;ink.color={.3f,.9f,.2f,1};document.paint_settings(ink);NativeStrokeSample stroke;stroke.origin=point+normal;stroke.direction=normal*-1.f;stroke.brush.radius=.05f;
  check(document.begin_native_stroke()&&document.sample_native_stroke(stroke),"Actual native paint worker accepts formerly degenerate untextured import geometry");document.end_native_stroke();
  auto painted_path=directory/(label+"-painted.dsculpt");document.save_async(painted_path).get();
  check(document.wait_native_stroke()&&document.wait_mesh()&&document.frame().state->error.empty(),"Immediately queued Save follows the actual native untextured paint stroke");auto painted=document.copy_scene().encode();
  check(painted!=accepted&&Scene::load(painted_path).encode()==painted&&document.undo()&&document.copy_scene().encode()==accepted,"Real painting changes only current editable data and has its own exact history endpoint");
  auto saved=directory/(label+".dsculpt");document.save_async(saved).get();
  check(Scene::load(saved).encode()==accepted&&document.undo()&&document.copy_scene().encode()==before&&document.redo()&&document.copy_scene().encode()==accepted,"Native ordered Save/reopen and one exact Undo/Redo retain generated and original coordinates");
 }
 {
  import_fixture::Fixture typed;while(typed.bin.size()%4)typed.bin.push_back(0);
  auto uv=typed.values({0,0,0,0,0,0,0,0},2,"VEC2");typed.j["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_0"]=uv;
  auto &material=typed.j["materials"][0];material["pbrMetallicRoughness"].erase("baseColorTexture");
  material["extensions"]["VRMC_materials_mtoon"]={{"specVersion","1.0"},{"shadeMultiplyTexture",{{"index",0},{"texCoord",1}}}};typed.j["extensionsUsed"].push_back("VRMC_materials_mtoon");
  auto path=directory/"typed-unused-pbr.glb";typed.save(path);Scene source;auto before=source.encode();ImportAuthoringOptions choices;choices.paint_resolution=64;choices.prepare_texture_atlas=true;
  auto result=prepare_authoring_import(source,path,choices);
  check(result.objects.size()==2&&source.encode()==before,"Typed MToon-only UV1 import uses the actual source-aware adapter without changing its caller");
  for(auto id:result.objects){const auto &o=result.scene.object(id);check(o.painted->source_sampling_prepared()&&o.painted->source_maps()->empty()&&o.painted->source_sampling_authored_uv1()&&o.material_bindings->has_mtoon&&o.material_bindings->material.textures[1].texcoord==1,"Typed-only coordinates remain genuine authored UV1 with no invented PBR image role");}
  const auto encoded=result.scene.encode();check(Scene::decode(encoded).encode()==encoded,"Typed-only source and writable field survive exact native restart");
 }
 std::cout<<"Unmapped atlas checks "<<checks<<'\n';return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
