// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-authoring.hpp"
#include "dome/document.hpp"
#include "dome/source-maps.hpp"
#include "cgltf.h"
#include "dome/humanoid-creator.hpp"
#include "dome/material-bindings.hpp"
#include "import-authoring-fixtures.hpp"
#include <iostream>
#include <map>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
import_fixture::Fixture source(bool legacy){
 using J=nlohmann::json;
 import_fixture::Fixture f;auto &primitive=f.j["meshes"][0]["primitives"][0];
 primitive["attributes"].erase("TEXCOORD_0");primitive["attributes"].erase("TEXCOORD_1");
 auto &material=f.j["materials"][0];material["pbrMetallicRoughness"].erase("baseColorTexture");
 std::map<std::string,size_t> nodes;J bones=legacy?J::array():J::object();
 for(const auto &role:human_roles())if(role.required){
  auto id=f.j["nodes"].size();nodes.emplace(role.name,id);
  f.j["nodes"].push_back({{"name",role.name},{"translation",{0,.1,0}},{"children",J::array()}});
  if(legacy)bones.push_back({{"bone",role.name},{"node",id},{"useDefaultValues",true}});
  else bones[role.name]={{"node",id}};
 }
 for(const auto &role:human_roles())if(role.required){
  auto parent=role.parent;
  while(!parent.empty()&&!nodes.contains(parent)){
   for(const auto &ancestor:human_roles())if(ancestor.name==parent){parent=ancestor.parent;break;}
  }
  auto id=nodes.at(role.name);auto owner=parent.empty()?0:nodes.at(parent);
  f.j["nodes"][owner]["children"].push_back(id);
 }
 if(legacy){
  f.j["extensions"]["VRM"]={{"specVersion","0.0"},
   {"meta",{{"title","MatCap only"},{"author","Native fixture"},{"licenseName","CC0"}}},
   {"humanoid",{{"humanBones",bones}}},{"firstPerson",J::object()},
   {"blendShapeMaster",{{"blendShapeGroups",J::array()}}},{"secondaryAnimation",J::object()},
   {"materialProperties",{{{"name","MatCap"},{"shader","VRM/MToon"},{"textureProperties",{{"_SphereAdd",0}}}}}}};
  f.j["extensionsUsed"].push_back("VRM");
 }else{
  f.j["extensions"]["VRMC_vrm"]={{"specVersion","1.0"},
   {"meta",{{"name","MatCap only"},{"authors",{"Native fixture"}},{"licenseUrl","https://creativecommons.org/publicdomain/zero/1.0/"}}},
   {"humanoid",{{"humanBones",bones}}}};
  material["extensions"]["VRMC_materials_mtoon"]={{"specVersion","1.0"},{"matcapTexture",{{"index",0},{"texCoord",1}}}};
  f.j["extensionsUsed"].push_back("VRMC_vrm");f.j["extensionsUsed"].push_back("VRMC_materials_mtoon");
  f.j["extensionsRequired"]={"VRMC_vrm","VRMC_materials_mtoon"};
 }
 return f;
}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);
 for(bool legacy:{false,true})for(bool atlas:{false,true}){
  auto fixture=source(legacy);auto path=directory/(legacy?"matcap-v0.vrm":"matcap-v1.vrm");fixture.save(path);
  Scene before;const auto original=before.encode();ImportAuthoringOptions options;options.paint_resolution=64;options.prepare_texture_atlas=atlas;
  auto imported=prepare_authoring_import(before,path,options);
  check(imported.objects.size()==2&&before.encode()==original,"View-dependent MatCap-only avatar imports both instances without inventing a source UV stream");
  for(auto id:imported.objects){const auto &object=imported.scene.object(id);const auto &asset=*object.appearance->asset;
   check(*object.appearance->original_bytes==fixture.bytes()&&asset.primitives[0].mesh.uv.empty()&&asset.primitives[0].uv1.empty(),"Exact immutable original model retains its intentional absence of UV0 and UV1");
   check(object.material_bindings&&object.material_bindings->has_mtoon&&object.material_bindings->roles[size_t(MToonSlot::Matcap)].image,"MatCap image and typed versioned shader survive native import");
   const auto &origin=*object.material_bindings->uv_origin;
   check(origin.material&&origin.roles[size_t(MToonSlot::Matcap)].image==object.material_bindings->roles[size_t(MToonSlot::Matcap)].image,"First UV origin retains the same view-dependent image and material descriptor");
   check(!object.painted->mesh().uv.empty()&&object.painted->surface_diagnostics().unpaintable_uv_faces.empty(),"Generated paint atlas remains writable without relabeling view-space MatCap as mesh-UV sampling");
  }
  if(atlas)for(auto id:imported.objects){const auto &o=imported.scene.object(id);check(!o.material_bindings->sampling_reparameterized&&o.painted->source_sampling_prepared()&&o.painted->valid_source_sampling_attachment()&&o.painted->surface_diagnostics().policy==PaintUvPolicy::Strict&&!o.painted->source_sampling_authored_uv1()&&o.painted->source_maps()->empty(),"Explicit writable atlas retains real MatCap view-space authority without fabricated UV1 or PBR image roles");}
  auto encoded=imported.scene.encode();auto saved=directory/(legacy?"matcap-v0.dsculpt":"matcap-v1.dsculpt");imported.scene.save(saved);
  check(Scene::decode(encoded).encode()==encoded&&Scene::load(saved).encode()==encoded,"Native restart preserves both view-dependent roles and original missing coordinate authority");
  if(atlas){
   Document native;native.replace(before);ImportSettings settings;settings.paint_resolution=64;settings.prepare_texture_atlas=true;native.import_asset(path,settings);const auto native_path=directory/(legacy?"native-matcap-v0.dsculpt":"native-matcap-v1.dsculpt");native.save_async(native_path).get();
   check(native.wait_asset()&&native.wait_mesh()&&native.frame().state->error.empty(),"Actual native MatCap import and immediately ordered Save complete with source-aware atlas authority");const auto accepted=native.copy_scene().encode();check(Scene::load(native_path).encode()==accepted,"Native MatCap worker restart retains both original missing UVs and writable source owners");
   check(native.undo()&&native.copy_scene().encode()==original&&native.redo()&&native.copy_scene().encode()==accepted,"Source-aware MatCap import remains one exact global Undo/Redo endpoint");
   const auto exported=directory/(legacy?"native-matcap-v0.glb":"native-matcap-v1.glb");native.export_asset(exported,AssetExportFormat::Glb).get();cgltf_options parse_options{};cgltf_data *parsed=nullptr;const auto code=cgltf_parse_file(&parse_options,exported.c_str(),&parsed);bool valid=code==cgltf_result_success;if(valid)valid=cgltf_load_buffers(&parse_options,parsed,exported.c_str())==cgltf_result_success&&cgltf_validate(parsed)==cgltf_result_success;cgltf_free(parsed);check(valid&&native.copy_scene().encode()==accepted,"Actual MatCap standard export has independently valid glTF geometry/resources and preserves the complete native source");
  }
  auto sampled=fixture;
  if(legacy)sampled.j["extensions"]["VRM"]["materialProperties"][0]["textureProperties"]["_ShadeTexture"]=0;
  else sampled.j["materials"][0]["extensions"]["VRMC_materials_mtoon"]["shadeMultiplyTexture"]={{"index",0},{"texCoord",1}};
  auto invalid=directory/(legacy?"sampled-v0.vrm":"sampled-v1.vrm");sampled.save(invalid);
  bool refused=false;try{(void)prepare_authoring_import(before,invalid,options);}catch(const std::invalid_argument &e){refused=std::string_view(e.what()).find("typed texture role requires original TEXCOORD_")!=std::string_view::npos;}
  check(refused&&before.encode()==original,"A real mesh-sampled shade role still requires original UVs and leaves the caller exact");
 }
 std::cout<<"MatCap import checks "<<checks<<'\n';return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
