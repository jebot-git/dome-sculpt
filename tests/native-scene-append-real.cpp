// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/native-scene-append.hpp"
#include "dome/native-authoring.hpp"
#include "dome/import-authoring.hpp"
#include "dome/creation.hpp"
#include "dome/document.hpp"
#include "dome/polygon-scene.hpp"
#include "polygon-conversion.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;void check(bool ok,const char *s){if(!ok)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<std::endl;}
bool refuses(auto f,std::string_view text){try{f();}catch(const std::exception&e){std::cout<<"REFUSAL "<<e.what()<<std::endl;return std::string_view(e.what()).find(text)!=std::string_view::npos;}return false;}
}
int main(int argc,char**argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 for(const char *base:{"vroid-beta-female","vroid-beta-male"}){
  std::cout<<"STAGE "<<base<<std::endl;Scene source;append_humanoid(source,humanoid_baseline_defaults(base),base,{},64);
  const auto at=std::find_if(source.objects().begin(),source.objects().end(),[](const auto&o){return bool(o.appearance);});const auto id=at->id;
  auto &object=source.object(id);auto first=capture_polygon_origin(source,id);object.rig_source=first;object.shape_source=first;
  object.painted=object.painted->snapshot();Brush b;b.from=b.to=object.painted->mesh().positions.front();b.normal={0,0,1};b.radius=5;b.strength=.7f;PaintSettings p;p.channel=PaintChannel::Color;p.color={.9f,.2f,.1f,1};p.front_only=false;object.painted->begin_stroke();object.painted->stamp(b,p);check(object.painted->end_stroke(),"Authentic CC0 base contains real sparse native paint");
  const auto exact=source.encode();NativeAuthoringExport owner;owner.scene=std::make_shared<const Scene>(source.snapshot());owner.allow_portable_fallback=true;const auto file=out/(std::string(base)+".glb");auto report=export_glb_authoring(owner,file);check(report.meshes==19,"Whole package exports all nineteen authentic CC0 parts");
  ImportAuthoringOptions settings;settings.paint_resolution=64;settings.limits.max_file_bytes=256*1024*1024;
  auto result=prepare_authoring_import(Scene{},file,settings);check(result.objects.size()==20&&result.scene.objects().size()==21,"Whole native package appends all real parts and its hidden/empty project data");
  for(size_t k=0;k<source.objects().size();++k){const auto &old=source.objects()[k],&now=result.scene.object(result.objects[k]);check(result.scene.world_matrix(now.id).matrix()==source.world_matrix(old.id).matrix()&&bool(now.deformation)==bool(old.deformation),"Real part placement and rig/morph presence survive native append");if(old.appearance)check(*now.appearance->original_bytes==*old.appearance->original_bytes&&now.painted->mesh().positions.size()==old.painted->mesh().positions.size()&&std::equal(now.painted->mesh().positions.begin(),now.painted->mesh().positions.end(),old.painted->mesh().positions.begin(),[](auto a,auto b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}),"Actual source bytes and every native REST position remain exact");}
  const auto active=result.objects.at(size_t(at-source.objects().begin()));const auto &mapped=result.scene.object(active);check(mapped.rig_source==mapped.shape_source&&mapped.rig_source->paint->encode()==first->paint->encode(),"Real first-rig/first-shape alias and prepaint state survive extraction and append");
  result.scene.save(out/(std::string(base)+".dsculpt"));check(Scene::load(out/(std::string(base)+".dsculpt")).encode()==result.scene.encode()&&source.encode()==exact,"Real appended Scene restarts exactly without mutating original owners");
  if(std::filesystem::file_size(file)>128*1024*1024){auto default_limit=settings;default_limit.limits.max_file_bytes=128*1024*1024;check(refuses([&]{prepare_authoring_import(Scene{},file,default_limit);},"budget"),"Large real package honors programmatic default128MiB until explicitly authorized256MiB");}
  // A real one-part subset owns the complete artist/source/rig dependencies but
  // does not duplicate unrelated live avatar parts in repeated imports.
  owner.scope=NativeAuthoringExport::Scope::ExportedObjects;owner.objects={id};const auto subset=out/(std::string(base)+"-subset.glb");export_glb_authoring(owner,subset);auto one=prepare_authoring_import(Scene{},subset,settings);auto two=prepare_authoring_import(one.scene,subset,settings);check(one.objects.size()==1&&two.objects.size()==1&&two.scene.objects().size()==3&&two.scene.object(two.objects[0]).group!=one.scene.object(one.objects[0]).group,"Repeated authentic subset imports retain independent live object/group rigs");
  const auto saved=out/(std::string(base)+"-twice.dsculpt");two.scene.save(saved);check(Scene::load(saved).encode()==two.scene.encode(),"Both authentic repeated-import instances survive native Save/reopen");
  Document doc;const auto before=doc.copy_scene().encode();doc.import_asset(file,64);auto save=doc.save_async(out/(std::string(base)+"-worker.dsculpt"));check(doc.wait_asset(std::chrono::seconds(180))&&doc.wait_mesh(std::chrono::seconds(180)),"Actual native UI worker reaches real CC0 package endpoint");if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);save.get();const auto accepted=doc.copy_scene().encode();check(Scene::load(out/(std::string(base)+"-worker.dsculpt")).encode()==accepted,"Actual worker explicitly permits256MiB and ordered Save retains its exact native endpoint");check(doc.undo()&&doc.copy_scene().encode()==before&&doc.redo()&&doc.copy_scene().encode()==accepted,"Real CC0 package import is one exact Undo/Redo transaction");
 }
 auto female=Scene::load(out/"vroid-beta-female.dsculpt"),male=Scene::load(out/"vroid-beta-male.dsculpt");const auto female_before=female.encode(),male_before=male.encode();check(refuses([&]{append_native_scene(female,male);},"save byte budget")&&female.encode()==female_before&&male.encode()==male_before,"Actual female+male full projects refuse combined unsaveable state atomically within the unchanged256MiB cap");
 {Document doc;doc.replace(female);check(doc.wait_mesh(std::chrono::seconds(180)),"Existing real destination is ready before overbudget native import");doc.import_asset(out/"vroid-beta-male.glb",64);check(doc.wait_asset(std::chrono::seconds(180))&&doc.copy_scene().encode()==female_before&&doc.frame().state->error.find("save byte budget")!=std::string::npos&&!doc.undo(),"Actual native worker rejects two-full-base save overflow without scene or history publication");}
 std::cout<<"Real native scene append: "<<checks<<" checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
