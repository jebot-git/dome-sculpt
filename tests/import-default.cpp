// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creator-archive.hpp"
#include "dome/document.hpp"
#include "dome/gltf-export.hpp"
#include "dome/polygon-scene.hpp"
#include "import-skin-fixtures.hpp"
#include "polygon-conversion.hpp"
#include "scene-feature-version.hpp"
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;}
template<class F>bool rejects(F f,std::string_view why={}){try{f();return false;}catch(const std::exception&e){return why.empty()||std::string_view(e.what()).find(why)!=std::string_view::npos;}}
void restart(const Scene&s){auto j=s.encode();check(j.at("version")==17,"Implicit default authority chooses Scene17");auto decoded=Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(j)));check(decoded.encode()==j,"Scene17 default/source/paint/frame/deformation archive restarts exactly");for(int v=2;v<17;++v){auto old=j;old["version"]=v;check(rejects([&]{Scene::decode(old);},"Scene17"),"Every older Scene version rejects meaningful default identity before nested decode");}}
}
int main()try{
 const auto home=std::filesystem::temp_directory_path()/("dome-import-default-"+std::to_string(getpid()));std::filesystem::create_directories(home);setenv("HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);
 struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{home};
 for(int mode=0;mode<3;++mode)for(bool unused:{false,true}){
  auto f=mode==2?import_fixture::skinned(true):import_fixture::Fixture{};f.j["meshes"][0]["primitives"][0].erase("material");
  if(!unused){f.j.erase("materials");f.j.erase("extensionsUsed");}
  if(mode){f.j["nodes"][0]["scale"]={-1.7f,.8f,1.1f};f.j["nodes"][1]["rotation"]={0,0,std::sin(.31f),std::cos(.31f)};}
  auto path=home/("default"+std::to_string(mode)+(unused?"-unused.glb":"-absent.glb"));f.save(path);Scene original;auto before=original.encode();ImportAuthoringOptions options;options.paint_resolution=64;auto result=prepare_authoring_import(original,path,options);
  check(original.encode()==before&&result.objects.size()==2,"Default import remains detached and preserves both original instances");
  for(auto id:result.objects){const auto&o=result.scene.object(id);const auto&a=*o.appearance;
   check(a.material==0&&a.source_material==-1&&a.material_json.empty()&&a.mtoon_json.empty(),"Absent primitive material has witnessed identity0/-1 and no invented authored JSON");
   check(*a.original_bytes==f.bytes()&&a.asset->materials.size()==size_t(unused?2:1),"Original bytes and unused authored material table stay exact");
   check(o.material.color==std::array<float,4>{1,1,1,1}&&o.material.metallic==1&&o.material.roughness==1&&o.material.emissive==std::array<float,3>{}&&o.material.alpha==Material::AlphaMode::Opaque&&!o.material.double_sided&&!o.material.unlit,"Native material uses normative glTF defaults rather than unused authored material zero");
   check(o.material_bindings&&o.material_bindings->uv_origin->appearance==o.appearance&&o.painted&&!o.painted->source_maps(),"Immutable default source frames and plain paint authority remain paired");
  }
  restart(result.scene);const auto id=result.objects.front();const auto &o=result.scene.object(id);
  CreatorAssetArchive assets;PaintImageArchive images;auto record=encode_creator_appearance(*o.appearance,assets,images);auto reloaded=CreatorAssetArchive::decode(assets.encode(),PaintImageArchive::decode(images.encode()));auto appearance=decode_creator_appearance(record,reloaded);
  check(appearance->material==0&&appearance->source_material==-1&&appearance->material_json.empty(),"Standalone source archive restores signed sentinel only with raw material-absence witness");
  for(auto invalid:{nlohmann::json(-2),nlohmann::json(UINT64_MAX),nlohmann::json(0),nlohmann::json(-1.0),nlohmann::json(nullptr)}){auto bad=record;bad["source_material"]=invalid;check(rejects([&]{decode_creator_appearance(bad,reloaded);}),"Forged, unsigned, floating or missing default sentinel refuses");}
  auto forged=*o.appearance;forged.material_json="{}";check(rejects([&]{CreatorAssetArchive a;PaintImageArchive i;encode_creator_appearance(forged,a,i);}),"Implicit source cannot acquire fabricated authored material JSON");
  forged=*o.appearance;auto altered=std::make_shared<ImportedAsset>(*forged.asset);altered->materials[0].color[0]=.1f;forged.asset=altered;check(rejects([&]{CreatorAssetArchive a;PaintImageArchive i;encode_creator_appearance(forged,a,i);},"defaults"),"Forged implicit material factors refuse even with unchanged raw bytes");
  auto retained=result.scene.snapshot();for(auto object:result.objects)retained.object(object).appearance.reset();restart(retained);
  check(detail::minimum_scene_version(o.material_bindings.get())==17,"First UV origin alone propagates default feature minimum");
  auto capsule=capture_polygon_origin(result.scene,id);Scene dormant;dormant.active().shape_source=capsule;restart(dormant);
  auto authority=std::make_shared<PolygonAuthority>();authority->active=false;authority->origin=capsule;dormant.active().shape_source.reset();dormant.active().polygon=authority;restart(dormant);
  auto color=std::make_shared<SourceColorPreparation>();color->original_appearance=o.appearance;auto cb=std::make_shared<SourceMaterialBindings>();cb->has_mtoon=false;cb->color_preparation=color;check(detail::minimum_scene_version(cb.get())==17,"Color before-state propagates default feature minimum independently of current material");
  auto normal=std::make_shared<SourceNormalPreparation>();normal->original_material=o.material_bindings;SourceMaterialBindings nb;nb.normal_preparation=normal;check(detail::minimum_scene_version(&nb)==17,"Normal nested before-binding propagates default feature minimum");
  auto saved=home/("restart"+std::to_string(mode)+(unused?"u":"a")+".dsculpt");result.scene.save(saved);check(Scene::load(saved).encode()==result.scene.encode(),"Scene17 self-contained disk restart preserves raw material absence");
  if(mode==0&&!unused){
   for(auto invalid:{nlohmann::json(-1),nlohmann::json(UINT64_MAX),nlohmann::json(0.0),nlohmann::json("0"),nlohmann::json(nullptr)}){auto bad=record;bad["material"]=invalid;check(rejects([&]{decode_creator_appearance(bad,reloaded);}),"Malformed implicit material slot refuses without numeric coercion");}
   for(const char *table:{"origins","color_preparations","normal_preparations"}){auto old=result.scene.encode();old["version"]=16;for(auto &object:old["objects"])object["appearance"]=nullptr;for(auto &origin:old["source_materials"]["origins"])origin["appearance"]=nullptr;old["source_materials"][table]=nlohmann::json::array({{{"appearance",record}}});check(rejects([&]{Scene::decode(old);},"Scene17"),"Dormant UV/color/normal appearance requires Scene17 before any bulk decode");}
   for(const char *table:{"origins","color_preparations","normal_preparations"}){auto bad=result.scene.encode();bad["source_materials"][table]=nlohmann::json::array();for(int i=0;i<257;++i)bad["source_materials"][table].push_back({{"appearance",nullptr}});check(rejects([&]{Scene::decode(bad);},"appearance table"),"New default-feature preflight bounds material before-state table visits");}
   auto oversized=result.scene.encode();oversized["polygon_sources"]["origins"]=nlohmann::json::array();for(int i=0;i<513;++i)oversized["polygon_sources"]["origins"].push_back({{"appearance",nullptr}});check(rejects([&]{Scene::decode(oversized);},"appearance table"),"New default-feature preflight bounds polygon history table visits");
   auto malformed=f;malformed.j["meshes"][0]["primitives"][0]["material"]=nullptr;auto invalid_path=home/"null-material.glb";malformed.save(invalid_path);check(rejects([&]{prepare_authoring_import(original,invalid_path,options);})&&original.encode()==before,"An explicit null material property cannot impersonate witnessed absence or mutate source");
   auto explicit_default=f;explicit_default.j["materials"]=nlohmann::json::array({nlohmann::json::object()});explicit_default.j["meshes"][0]["primitives"][0]["material"]=0;auto explicit_path=home/"authored-default.glb";explicit_default.save(explicit_path);auto authored=prepare_authoring_import(original,explicit_path,options);const auto &a=*authored.scene.object(authored.objects.front()).appearance;check(a.material==1&&a.source_material==0&&a.material_json=="{}"&&authored.scene.encode()["version"]==9,"An explicitly authored empty material zero keeps its real identity and historical schema");
  }
 }
 std::cout<<checks<<" witnessed default-material import/archive checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
