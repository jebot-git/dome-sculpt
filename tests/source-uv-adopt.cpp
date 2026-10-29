// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/source-uv-bake.hpp"
#include "dome/scene.hpp"
#include "dome/document.hpp"
#include <algorithm>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
Image image(std::array<uint8_t,4> value) {
  Image output{64,64,std::vector<uint8_t>(64*64*4)};
  for(size_t n=0;n<output.rgba.size();++n)output.rgba[n]=value[n%4];return output;
}
Mesh mesh(){Mesh m;m.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};m.normals.assign(4,{0,0,1});m.indices={0,1,2,0,2,3};m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};return m;}
SourceUvBakeInput input(std::shared_ptr<const PaintedMesh> paint) {
  SourceUvBakeInput i;i.paint=paint;i.target=paint->mesh();
  for(auto &q:i.target.uv)q={1-q[1],q[0]};i.source_vertices={0,1,2,3};return i;
}
nlohmann::json canonical_volumes(nlohmann::json scene) {
  // Native volume maps encode unordered samples; ordering is not source data.
  for(auto &object:scene["objects"]) {
    auto &v=object["volume"];auto &samples=v["samples"];std::sort(samples.begin(),samples.end());
    for(auto &layer:v["sculpt_layers"]){auto &delta=layer["delta"];std::sort(delta.begin(),delta.end());}
  }
  return scene;
}
}
int main()try {
  auto source=std::make_shared<PaintSourceMaps>();source->color.image=std::make_shared<const Image>(image({200,100,50,255}));
  source->emission.image=std::make_shared<const Image>(image({32,64,128,255}));
  auto paint=PaintedMesh::from_maps(mesh(),source,64);paint->add_layer("Removed");paint->delete_layer();
  Brush brush;brush.radius=4;brush.from=brush.to={};brush.normal={0,0,1};PaintSettings settings;settings.front_only=false;
  paint->add_layer("Hidden color");paint->begin_stroke();paint->stamp(brush,settings);paint->end_stroke();paint->layer_properties("Hidden color",false,.3f);
  paint->add_layer("Emission");settings.channel=PaintChannel::Emission;settings.emission={.2f,.8f,.1f};
  paint->begin_stroke();paint->stamp(brush,settings);paint->end_stroke();paint->layer_properties("Emission",true,.5f);
  BakedTextures base;base.maps.color=image({20,40,60,255});base.maps.metal_rough=image({255,128,0,255});
  base.maps.emission=image({16,64,32,255});base.maps.normal=image({128,128,255,255});base.coverage.assign(64*64,128);paint->install_bake(base);
  auto original=paint->encode();SourceUvBakeOptions options;options.resolution=64;
  auto result=bake_source_uv(input(paint),options);auto adopted=adopt_source_uv_bake(result);
  check(adopted.paint->source_maps()->color.image==result.maps->color.image && adopted.paint->baked()==result.baked,
        "Native adoption shares immutable generated source images and the separate baked payload");
  check(adopted.paint->layers().size()==paint->layers().size() && adopted.paint->active_layer()==paint->active_layer() &&
        adopted.paint->encode()["next_id"]==original["next_id"] && adopted.paint->undo_count()==0,
        "Native adoption preserves every layer identity, next ID and active selection with caller-owned history");
  check(!adopted.material_bindings->has_mtoon && adopted.material_bindings->uv_origin->paint!=paint &&
        adopted.material_bindings->uv_origin->paint->encode()==original,
        "Generic PBR adoption retains the complete first immutable original rather than dropping its maps or layers");
  auto held=adopted.paint->snapshot();auto held_bytes=held->encode();
  adopted.paint->add_layer("After UV bake");
  check(adopted.paint->active_layer()==original["next_id"].get<uint64_t>(),
        "The first new layer after adoption uses the preserved monotonic source identity");
  settings.channel=PaintChannel::Color;adopted.paint->begin_stroke();adopted.paint->stamp(brush,settings);adopted.paint->end_stroke();
  check(held->encode()==held_bytes && paint->encode()==original,
        "Later native painting cannot mutate adopted result tiles or the retained first source");
  auto again=input(held);again.current_material=adopted.material_bindings;
  auto second=adopt_source_uv_bake(bake_source_uv(again,options));
  check(second.material_bindings->uv_origin==adopted.material_bindings->uv_origin &&
        second.material_bindings->uv_origin->paint->source_maps()->color.image==source->color.image,
        "Repeated baking propagates the exact first-source owner and original image identities");
  Scene scene;scene.active().painted=second.paint;scene.active().mesh_active=true;scene.active().material_bindings=second.material_bindings;
  scene.set_deformation(scene.active().id,DeformationState(second.paint->mesh()));
  scene.duplicate(scene.active().id);auto encoded=scene.encode();
  check(encoded["version"]==7 && encoded["source_materials"]["bindings"].size()==1 &&
        encoded["source_materials"]["origins"].size()==1 && encoded["source_materials"]["paints"].size()==1,
        "Scene v7 deduplicates shared material holders, original-source records and paint payloads");
  auto decoded=Scene::decode(encoded);
  check(decoded.encode()==encoded && decoded.object(1).material_bindings==decoded.object(2).material_bindings &&
        decoded.object(1).material_bindings->uv_origin->paint->encode()==original,
        "Native restart retains exact derived paint and first-source provenance with shared holder identity");
  auto surfaces=decoded.preview_surfaces();check(surfaces.size()==2 && surfaces[0].material_bindings==decoded.object(1).material_bindings,
        "Immutable preview surfaces carry the same explicit material holder without reparsing raw metadata");
  auto path=std::filesystem::temp_directory_path()/("source-uv-adopt-"+std::to_string(getpid())+".dsculpt");
  decoded.save(path);check(Scene::load(path).encode()==encoded,"Native atomic save and disk reopen retain v7 provenance and editable state");std::filesystem::remove(path);
  {
    Document document;document.replace(decoded);auto before_mode=document.copy_scene().encode();
    document.paint_command(SpatialCommand::EditSource);auto volume_mode=document.copy_scene().encode();
    check(source_is_dormant(document.copy_scene().active()) && volume_mode["version"]==7,
          "Native source-mode command keeps complete immutable material and deformation data dormant");
    check(document.undo() && document.copy_scene().encode()==before_mode,
          "One global Undo restores the exact original mesh, source roles, paint and deformation together");
    check(document.redo() && document.copy_scene().encode()==volume_mode,
          "One global Redo restores the exact dormant source mode without dropping provenance");
  }
  auto retained=decoded.object(1).painted;auto retained_binding=decoded.object(1).material_bindings;auto retained_deformation=decoded.object(1).deformation;
  decoded.object(1).mesh_active=false;
  check(rejects([&]{decoded.require_exportable_sources();}) && rejects([&]{decoded.surfaces();}) && decoded.preview_surfaces().size()==1,
        "Scene export preflight catches a visible dormant source even when an empty volume would omit its surface entirely");
  decoded.object(1).visible=false;
  check(!rejects([&]{decoded.require_exportable_sources();}),
        "Explicitly hidden dormant objects are excluded from export preflight");
  decoded.object(1).visible=true;
  Brush volume_brush;volume_brush.radius=.15f;decoded.object(1).volume->begin_stroke();decoded.object(1).volume->stamp(volume_brush);decoded.object(1).volume->end_stroke();
  decoded.object(1).mesh_active=false;auto dormant=decoded.preview_surfaces();
  auto volume=std::find_if(dormant.begin(),dormant.end(),[](const auto &s){return s.id==1;});
  check(volume!=dormant.end() && volume->source_dormant && !volume->appearance && !volume->material_bindings && !volume->deformation && !volume->textures,
        "Generated volume previews explicitly mark retained source as dormant and never sample unrelated UV or deformation data");
  check(rejects([&]{export_glb(dormant,path);}) && !std::filesystem::exists(path),
        "Source-to-volume export refuses dormant source before publishing a flattened file");
  auto dormant_encoded=decoded.encode();auto dormant_loaded=Scene::decode(dormant_encoded);
  check(canonical_volumes(dormant_loaded.encode())==canonical_volumes(dormant_encoded) && dormant_loaded.object(1).material_bindings->uv_origin->paint->encode()==original,
        "Native save and reopen retain dormant original mesh, layers and material provenance exactly");
  decoded.object(1).mesh_active=true;auto restored=decoded.preview_surfaces();
  auto restored_mesh=std::find_if(restored.begin(),restored.end(),[](const auto &s){return s.id==1;});
  check(restored_mesh!=restored.end() && !restored_mesh->source_dormant && restored_mesh->material_bindings==retained_binding && restored_mesh->deformation==retained_deformation && decoded.object(1).painted==retained,
        "Restoring the original painting mesh reactivates exact retained source identity without reconstruction");
  decoded.object(2).material_bindings.reset();decoded.object(2).deformation.reset();decoded.object(2).mesh_active=false;
  decoded.object(2).volume=std::make_shared<Volume>(decoded.object(1).volume->geometry_snapshot());
  auto generic_dormant=decoded.preview_surfaces();auto generic_volume=std::find_if(generic_dormant.begin(),generic_dormant.end(),[](const auto &s){return s.id==2;});
  check(generic_volume!=generic_dormant.end() && generic_volume->source_dormant && !generic_volume->textures,
        "Generic painted source maps and editable paint also remain explicit dormant export data without a creator or resolved holder");
  auto bad=result;bad.layers[0].name="Wrong metadata";check(rejects([&]{adopt_source_uv_bake(bad);}),"Adoption rejects changed layer metadata");
  bad=result;bad.layers[0].tiles.pop_back();check(rejects([&]{adopt_source_uv_bake(bad);}),"Adoption rejects incomplete sparse tile tables");
  bad=result;bad.active_layer=999;check(rejects([&]{adopt_source_uv_bake(bad);}),"Adoption rejects invented active layer identity");
  bad=result;bad.baked.reset();check(rejects([&]{adopt_source_uv_bake(bad);}),"Adoption cannot silently discard a retained baked base");
  bad=result;bad.source_vertices[0]=999;check(rejects([&]{adopt_source_uv_bake(bad);}),"Adoption revalidates exact source corner identities");
  check(rejects([&]{adopt_source_uv_bake(result,[]{return true;});}) && paint->encode()==original,
        "Cancellation before adoption publishes no partially constructed surface or source changes");
  auto malformed=encoded;malformed["source_materials"]["bindings"][0]["normal_frame"]="canonical-mikk";
  check(rejects([&]{Scene::decode(malformed);}),"Native decoding cannot relabel derivative role normals as canonical Mikk data");
  malformed=encoded;malformed["source_materials"]["bindings"][0]["origin"]=999;
  check(rejects([&]{Scene::decode(malformed);}),"Native decoding rejects missing immutable original-source identities");
  malformed=encoded;auto original_paint=malformed["source_materials"]["paints"][0];original_paint["resolution"]=2048;
  malformed["source_materials"]["paints"]=nlohmann::json::array();for(int n=0;n<5;++n)malformed["source_materials"]["paints"].push_back(original_paint);
  check(rejects([&]{Scene::decode(malformed);}),"Aggregate original raster quotas are checked before any oversized SurfaceMap is allocated");
  paint->layer_properties("Changed external source owner",false,.1f);
  check(adopted.material_bindings->uv_origin->paint->encode()==original,
        "First-source ownership is a detached COW paint snapshot even if the caller later edits its original class");
  return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
