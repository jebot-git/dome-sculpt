// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/import-authoring.hpp"
#include "dome/material-bindings.hpp"
#include "dome/humanoid-creator.hpp"
#include "import-authoring-fixtures.hpp"
#include "cgltf.h"
#include "import-controller-driver.hpp"
#include <thread>
#include <iostream>
using namespace dome;
namespace {
int checks=0;void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<std::endl;}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
import_fixture::Fixture source(bool flat=false){auto f=import_fixture::Fixture{};auto &material=f.j["materials"][0];material["pbrMetallicRoughness"]["baseColorTexture"]["texCoord"]=1;material["normalTexture"]={{"index",0},{"texCoord",1},{"scale",.6f}};if(flat){auto &p=f.j["meshes"][0]["primitives"][0];p["attributes"].erase("NORMAL");p["targets"][0].erase("NORMAL");}return f;}
}
int main(int argc,char **argv)try{
 if(argc==3&&std::string_view(argv[1])=="--empty-project"){Scene{}.save(argv[2]);return 0;}
 if(argc==3&&std::string_view(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 if(argc==3&&std::string_view(argv[1])=="--ui-fixture"){source().save(argv[2]);return 0;}
 if(argc!=2)return 2;auto directory=std::filesystem::path(argv[1]);std::filesystem::create_directories(directory);
 Scene original;auto baseline=original.encode();ImportAuthoringOptions options;options.paint_resolution=64;
 for(bool flat:{false,true}){auto fixture=source(flat);auto path=directory/(flat?"flat.glb":"artist.glb");fixture.save(path);
  auto raw=prepare_authoring_import(original,path,options);check(raw.objects.size()==2&&raw.scene.active().painted->source_maps()->uv1&&!raw.scene.active().material_bindings->sampling_reparameterized&&original.encode()==baseline,"Default original-coordinate import retains genuine UV1 for viewing without mutating the caller");
  auto detached=raw.scene.active().painted->snapshot();const auto raw_before=detached->encode();check(refuses([&]{detached->begin_stroke();})&&!detached->in_stroke()&&detached->encode()==raw_before,"Raw UV1 paint admission refuses before tiles/stroke/history change; explicit preparation remains required");
  auto prepared_options=options;prepared_options.prepare_texture_atlas=true;auto prepared=prepare_authoring_import(original,path,prepared_options);
  check(prepared.objects.size()==2&&prepared.scene.groups().size()==4&&original.encode()==baseline,"Explicit editable-atlas import retains instances and hierarchy without changing the caller");
  for(auto id:prepared.objects){const auto &object=prepared.scene.object(id);const auto &binding=*object.material_bindings;
   check(object.painted->source_sampling_prepared()&&object.painted->source_maps()->uv1&&!binding.sampling_reparameterized&&object.painted->source_sampling_original()->encode()==raw.scene.object(id).painted->encode()&&binding.uv_origin->paint->source_maps()->uv1,"Writable atlas and exact original UV1 image domain retain independent actual owners");
   check(*object.appearance->original_bytes==fixture.bytes()&&object.appearance->source_node>=1&&binding.normal_frames->pbr->generated_tangents==flat,"Original raw bytes, node identities and authored/generated original tangent authority survive preparation");
   check(object.painted->resolution()==64&&object.painted->preview(object.material)->overlay!=nullptr&&object.deformation->morphs->targets.size()==1,"Imported UV1 material keeps an independent selected-resolution paint overlay and editable morph state");
  }
  auto encoded=prepared.scene.encode();check(encoded["version"]==28&&Scene::decode(encoded).encode()==encoded,"Native Scene28 restart preserves prepared layers, original coordinates and retained material frames");
  auto saved=directory/(flat?"flat.dsculpt":"artist.dsculpt");prepared.scene.save(saved);check(Scene::load(saved).encode()==encoded,"Actual project Save/reopen retains prepared source-coordinate authority");
  auto bad=fixture;bad.j["meshes"][0]["primitives"][0]["attributes"].erase("TEXCOORD_1");auto missing=directory/(flat?"flat-missing.glb":"artist-missing.glb");bad.save(missing);
  check(refuses([&]{prepare_authoring_import(original,missing,prepared_options);})&&original.encode()==baseline,"Missing selected source coordinates fail explicitly without publishing a partial atlas");
  for(int threshold:{4,20,60,150}){auto cancelled=prepared_options;int calls=0;cancelled.limits.cancelled=[&]{return ++calls>=threshold;};check(refuses([&]{prepare_authoring_import(original,path,cancelled);})&&original.encode()==baseline,"Cancellation during explicit import/preparation leaves the complete source unchanged");}
  Document document;document.replace(original);ImportSettings settings;settings.paint_resolution=64;settings.prepare_texture_atlas=true;document.import_asset(path,settings);check(document.wait_asset()&&document.frame().state->error.empty()&&document.wait_mesh(),"Actual native background Import accepts the explicit texture preparation choice");
  auto accepted=document.copy_scene().encode();auto destination=directory/(flat?"flat-native.dsculpt":"artist-native.dsculpt");document.save_async(destination).get();check(Scene::load(destination).encode()==accepted,"Native ordered Save retains accepted imported source and derived texture coordinates");
  check(document.undo()&&document.copy_scene().encode()==baseline&&document.redo()&&document.copy_scene().encode()==accepted,"Import and atlas preparation form one exact global Undo/Redo transaction");
  auto before_paint=document.copy_scene();const auto &active=before_paint.active();auto visible=active.deformation?active.deformation->preview(active.painted->mesh()):active.painted->mesh();const auto &indices=visible.indices;
  auto local=visible.positions[indices[0]]*.15f+visible.positions[indices[1]]*.1f+visible.positions[indices[2]]*.75f;auto world=before_paint.world_matrix(active.id).point({local.x,local.y,local.z});Vec3 point{float(world[0]),float(world[1]),float(world[2])};auto normal=document.gradient(point);
  check(document.raycast(point+normal,normal*-1.f).has_value(),"Prepared imported UV1 geometry remains pickable in its current morphed world pose");
  PaintSettings ink;ink.color={0,1,0,1};document.paint_settings(ink);NativeStrokeSample sample;sample.origin=point+normal;sample.direction=normal*-1.f;sample.brush.radius=.05f;
  check(document.begin_native_stroke()&&document.sample_native_stroke(sample),"Actual native paint worker accepts the prepared UV1 import");document.end_native_stroke();auto painted_path=directory/(flat?"flat-painted.dsculpt":"artist-painted.dsculpt");document.save_async(painted_path).get();
  check(document.wait_native_stroke()&&document.wait_mesh()&&document.frame().state->error.empty(),"Immediately queued Save follows actual native painting on the imported texture atlas");auto painted=document.copy_scene().encode();
  check(painted!=accepted&&Scene::load(painted_path).encode()==painted&&document.copy_scene().active().material_bindings->uv_origin->paint->source_maps()->uv1,"Painting changes editable atlas data while original UV1 authority and ordered restart remain intact");
  check(document.undo()&&document.copy_scene().encode()==accepted&&document.redo()&&document.copy_scene().encode()==painted,"Painting and import retain separate exact global history endpoints");
  auto exported=directory/(flat?"flat-painted.glb":"artist-painted.glb");document.export_asset(exported,AssetExportFormat::EditableGlb256).get();cgltf_options parse_options{};cgltf_data *parsed=nullptr;
  check(cgltf_parse_file(&parse_options,exported.c_str(),&parsed)==cgltf_result_success,"Independent cgltf parser reads the actual painted UV1-import export worker result");std::unique_ptr<cgltf_data,decltype(&cgltf_free)> owner(parsed,cgltf_free);
  check(cgltf_load_buffers(&parse_options,parsed,exported.c_str())==cgltf_result_success&&cgltf_validate(parsed)==cgltf_result_success,"Actual GLB worker emits valid prepared texture, morph and attribute resources");
  size_t mapped=0;for(size_t material=0;material<parsed->materials_count;++material){const auto &m=parsed->materials[material];if(m.normal_texture.texture){++mapped;check(m.normal_texture.texcoord==0&&m.pbr_metallic_roughness.base_color_texture.texcoord==0,"Explicit finite export samples its declared current atlas instead of silently substituting raw UV1");}}
  check(mapped==2,"Both retained imported instances export their independent editable material bindings");
 }
 {
  auto many=source();for(int n=0;n<7;++n){auto index=many.j["nodes"].size();many.j["nodes"].push_back(many.j["nodes"][1]);many.j["nodes"][0]["children"].push_back(index);}
  auto path=directory/"atlas-over-budget.glb";many.save(path);auto choices=options;choices.prepare_texture_atlas=true;choices.paint_resolution=1024;
  bool bounded=false;try{(void)prepare_authoring_import(original,path,choices);}catch(const std::length_error &e){bounded=std::string_view(e.what()).find("Editable atlas import exceeds Scene paint-map budget")!=std::string_view::npos;}
  check(bounded&&original.encode()==baseline,"Multi-instance atlas preparation rejects its aggregate retained/current pixel budget before heavy work and preserves the caller");
 }
 check(spatial_action(23,9)==SpatialCommand::ImportTextureAtlas&&spatial_parameter(23,6)==VrParameter::ImportUnitScale&&!spatial_action(23,8),"XR editable-atlas choice preserves custom-unit control and standard Back navigation");
 auto controlled=directory/"controller";std::filesystem::create_directories(controlled);source().save(controlled/"artist.glb");Document controller;controller.replace(original);ImportSettings settings;settings.paint_resolution=64;controller.configure_import(settings);controller.open_import_browser(controlled);
 auto wait_listing=[&]{const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(std::chrono::steady_clock::now()<end){auto listing=controller.frame().state->importing;if(listing&&!listing->busy)return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}throw std::runtime_error("Controller import listing did not settle");};wait_listing();ImportDriver hand(controller);hand.open();hand.sector(8);hand.sector(9);hand.sector(10);wait_listing();hand.sector(5);
 check(controller.frame().state->menu_page==23&&!controller.import_settings().prepare_texture_atlas,"Actual joystick navigation reaches import options with original coordinates selected");hand.sector(9);
 check(controller.import_settings().prepare_texture_atlas,"Actual joystick action toggles explicit editable-atlas preparation");hand.sector(8);hand.sector(6);
 check(controller.wait_asset()&&controller.wait_mesh()&&controller.frame().state->error.empty()&&controller.copy_scene().objects().size()==3,"Actual controller Apply imports the UV1 material through the shared background preparation path");
 std::cout<<"Texture atlas import checks "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
