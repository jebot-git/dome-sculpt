// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-authoring.hpp"
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
 for(bool legacy:{false,true}){
  auto fixture=source(legacy);auto path=directory/(legacy?"matcap-v0.vrm":"matcap-v1.vrm");fixture.save(path);
  Scene before;const auto original=before.encode();ImportAuthoringOptions options;options.paint_resolution=64;
  auto imported=prepare_authoring_import(before,path,options);
  check(imported.objects.size()==2&&before.encode()==original,"View-dependent MatCap-only avatar imports both instances without inventing a source UV stream");
  for(auto id:imported.objects){const auto &object=imported.scene.object(id);const auto &asset=*object.appearance->asset;
   check(*object.appearance->original_bytes==fixture.bytes()&&asset.primitives[0].mesh.uv.empty()&&asset.primitives[0].uv1.empty(),"Exact immutable original model retains its intentional absence of UV0 and UV1");
   check(object.material_bindings&&object.material_bindings->has_mtoon&&object.material_bindings->roles[size_t(MToonSlot::Matcap)].image,"MatCap image and typed versioned shader survive native import");
   const auto &origin=*object.material_bindings->uv_origin;
   check(origin.material&&origin.roles[size_t(MToonSlot::Matcap)].image==object.material_bindings->roles[size_t(MToonSlot::Matcap)].image,"First UV origin retains the same view-dependent image and material descriptor");
   check(!object.painted->mesh().uv.empty()&&object.painted->surface_diagnostics().unpaintable_uv_faces.empty(),"Generated paint atlas remains writable without relabeling view-space MatCap as mesh-UV sampling");
  }
  auto encoded=imported.scene.encode();auto saved=directory/(legacy?"matcap-v0.dsculpt":"matcap-v1.dsculpt");imported.scene.save(saved);
  check(Scene::decode(encoded).encode()==encoded&&Scene::load(saved).encode()==encoded,"Native restart preserves both view-dependent roles and original missing coordinate authority");
  auto sampled=fixture;
  if(legacy)sampled.j["extensions"]["VRM"]["materialProperties"][0]["textureProperties"]["_ShadeTexture"]=0;
  else sampled.j["materials"][0]["extensions"]["VRMC_materials_mtoon"]["shadeMultiplyTexture"]={{"index",0},{"texCoord",1}};
  auto invalid=directory/(legacy?"sampled-v0.vrm":"sampled-v1.vrm");sampled.save(invalid);
  bool refused=false;try{(void)prepare_authoring_import(before,invalid,options);}catch(const std::invalid_argument &e){refused=std::string_view(e.what()).find("typed texture role requires original TEXCOORD_")!=std::string_view::npos;}
  check(refused&&before.encode()==original,"A real mesh-sampled shade role still requires original UVs and leaves the caller exact");
 }
 std::cout<<"MatCap import checks "<<checks<<'\n';return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
