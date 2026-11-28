// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_BAKE_CAGE_HELPERS_ONLY
#include "bake-cage.cpp"
#include "dome/bake-recipe.hpp"
#include "dome/material-bindings.hpp"
#include "dome/native-scene-append.hpp"
#include "dome/native-authoring.hpp"
#include <fstream>
#include <cstdlib>
#include "polygon-conversion.hpp"
#include "dome/shapekey-authoring.hpp"
#include "dome/texture-sets.hpp"
#include "texture-set-paint-access.hpp"

namespace {
Mesh material_quads(float height) {
  Mesh mesh;
  for(float x : {0.f,3.f}) {
    const auto first=uint32_t(mesh.positions.size());
    mesh.positions.insert(mesh.positions.end(),{{x-.5f,-.5f,height},{x+.5f,-.5f,height},
        {x+.5f,.5f,height},{x-.5f,.5f,height}});
    mesh.normals.insert(mesh.normals.end(),4,{0,0,1});
    mesh.uv.insert(mesh.uv.end(),{{.125f,.125f},{.875f,.125f},{.875f,.875f},{.125f,.875f}});
    mesh.indices.insert(mesh.indices.end(),{first,first+1,first+2,first,first+2,first+3});
  }
  return mesh;
}

Scene texture_set_bake_scene(float source_height,float cage_height) {
  Scene scene;
  const auto target=scene.active().id;
  const auto source=scene.add("Two source materials",Volume{});
  const auto cage=scene.add("Full matched cage",Volume{});
  auto &cage_object=scene.object(cage);
  cage_object.mesh_active=true;
  cage_object.visible=false;
  auto cage_mesh=material_quads(cage_height);
  for(size_t vertex=0;vertex<cage_mesh.uv.size();++vertex)
    cage_mesh.uv[vertex][0]=cage_mesh.uv[vertex][0]*.5f+(vertex<4?0.f:.5f);
  cage_object.painted=std::make_shared<PaintedMesh>(std::move(cage_mesh),64);
  std::array<TextureSetInitialField,2> fields;
  fields[0].resolution=fields[1].resolution=64;
  fields[0].material.name="Left set";
  fields[1].material.name="Right set";
  const std::array<TextureSetId,4> membership{0,0,1,1};
  initialize_texture_sets(scene,target,material_quads(0),fields,membership);
  fields[0].material.color={.9f,.02f,.03f,1};
  fields[1].material.color={.02f,.8f,.03f,1};
  initialize_texture_sets(scene,source,material_quads(source_height),fields,membership);
  scene.object(source).visible=false;
  scene.select(target);
  select_texture_set(scene,target,1);
  require_cage(edit_texture_set(scene,target,1,[](PaintedMesh &paint,Material &) {
    paint.add_layer("Editable brush layer");
    Brush brush;brush.from=brush.to={3,0,0};brush.normal={0,0,1};brush.radius=.3f;brush.strength=1;
    PaintSettings ink;ink.channel=PaintChannel::Color;ink.color={.03f,.04f,.9f,1};
    paint.begin_stroke();paint.stamp(brush,ink);
    return paint.end_stroke();
  }),"The selected field carries a real editable brush stroke");
  return scene;
}

ObjectBakeSettings texture_set_probe_settings(uint64_t cage) {
  ObjectBakeSettings settings;
  settings.front_mm=80;settings.padding=0;settings.ao_samples=4;settings.cage_object=cage;
  return settings;
}

void texture_set_bake(const std::filesystem::path &output) {
  std::filesystem::create_directories(output);
  auto scene=texture_set_bake_scene(.012f,.030f);
  const auto source=scene.objects()[1].id;
  const auto cage=scene.objects()[2].id;
  const auto target=scene.active().id;
  const auto original=scene.encode();
  const auto original_left=scene.active().painted;
  const auto original_right=dome::active_texture_set(scene.active()).painted;
  const auto source_collection=scene.object(source).texture_sets;
  const auto cage_paint=scene.object(cage).painted;
  const auto right_settings=texture_set_probe_settings(cage);

  auto cancelled=scene.snapshot();
  bool armed=false;
  require_cage(rejected_cage([&]{bake_object(cancelled,source,right_settings,[&]{return armed;},
      [&](float p){armed|=p>.12f;});})&&armed&&cancelled.encode()==original,
      "Cancellation during active-set projection preserves the complete Scene");

  Document doc;
  doc.replace(scene);
  ready_cage(doc);
  require_cage(doc.frame().state->can_bake&&doc.frame().state->active_texture_set==1,
      "The shared editor enables Bake for the selected right field");
  doc.bake_from_object(source,right_settings);
  doc.save_async(output/"right-ordered.dsculpt").get();
  ready_cage(doc);
  const auto right_scene=doc.copy_scene();
  const auto right_wire=right_scene.encode();
  const auto right=active_texture_set(right_scene.active());
  require_cage(doc.frame().state->error.empty()&&right.painted->baked()&&doc.frame().state->can_clear_bake&&
      Scene::load(output/"right-ordered.dsculpt").encode()==right_wire,
      "Active-set Bake and ordered Save publish the exact complete Scene");
  const auto &right_bake=*right.painted->baked();
  const auto pixel=(32*64+32)*4;
  require_cage(right_bake.coverage[32*64+32]&&right_bake.maps.color.rgba[pixel+1]>right_bake.maps.color.rgba[pixel]&&
      !right_bake.maps.normal.rgba.empty()&&!right_bake.maps.metal_rough.rgba.empty(),
      "Overlapping UVs bake only the right recipient using its green source material and native normal and AO channels");
  require_cage(detail::TextureSetPaintAccess::same_current(*right_scene.active().painted,*original_left)&&
      right.painted->layers().back().tiles==original_right->layers().back().tiles&&
      right_scene.object(source).texture_sets==source_collection&&
      right_scene.object(cage).painted->mesh_owner()==cage_paint->mesh_owner(),
      "Bake leaves the other raster, editable paint, source collection and cage geometry intact");
  require_cage(right.bake_recipe&&right.bake_recipe->source.object==source&&right.bake_recipe->settings.cage_object==cage&&
      !right_scene.active().bake_recipe&&doc.frame().state->bake_recipe==right.bake_recipe,
      "The completed recipe belongs to the right field and is available to the shared editor");
  require_cage(doc.undo()&&doc.copy_scene().encode()==original&&doc.redo()&&doc.copy_scene().encode()==right_wire,
      "Active-set Bake is exactly one Undo and Redo transition");
  ready_cage(doc);

  TextureSetEdit selection;selection.set=0;
  doc.edit_texture_set(selection);ready_cage(doc);
  require_cage(!doc.frame().state->bake_recipe&&!doc.frame().state->can_clear_bake&&
      doc.frame().state->bake_settings.cage_object==0,
      "Selecting the unbaked primary field presents independent settings and Clear state");
  ObjectBakeSettings left_settings;left_settings.front_mm=55;left_settings.ao_samples=0;left_settings.padding=0;
  doc.bake_from_object(source,left_settings);ready_cage(doc);
  const auto both=doc.copy_scene();
  const auto both_wire=both.encode();
  const auto &left_bake=*both.active().painted->baked();
  require_cage(left_bake.maps.color.rgba[pixel]>left_bake.maps.color.rgba[pixel+1]&&
      both.active().texture_sets->at(1).painted->baked()==right.painted->baked(),
      "Primary Bake samples the red source and leaves the right bake owner alone");

  doc.clear_bake();ready_cage(doc);
  const auto cleared=doc.copy_scene();
  const auto cleared_wire=cleared.encode();
  require_cage(!cleared.active().painted->baked()&&cleared.active().bake_recipe==both.active().bake_recipe&&
      cleared.active().texture_sets->at(1).painted->baked()==right.painted->baked()&&
      !doc.frame().state->can_clear_bake,
      "Clear affects the primary bake alone and keeps its recallable recipe and editable layers");
  require_cage(doc.undo()&&doc.copy_scene().encode()==both_wire&&doc.redo()&&doc.copy_scene().encode()==cleared_wire,
      "Clearing the active bake has exact Undo and Redo");
  ready_cage(doc);

  selection.set=1;doc.edit_texture_set(selection);ready_cage(doc);
  const auto recalled_scene=doc.copy_scene().encode();
  require_cage(doc.frame().state->bake_recipe&&doc.frame().state->bake_settings.front_mm==80&&
      doc.frame().state->bake_settings.cage_object==cage,
      "Returning to the right field restores its own saved projection settings");
  Driver hand(doc);hand.open();hand.sector(1);hand.value(.2f,0);hand.confirm();
  const float modified=doc.frame().state->bake_settings.front_mm;
  hand.sector(0);hand.ray(.5f,432.f/640);
  require_cage(modified!=80&&doc.frame().state->controls.bake_preset&&
      doc.frame().state->controls.bake_preset->settings.cage_object==cage,
      "Controller Recall stages the selected material's complete saved recipe");
  hand.confirm();
  require_cage(doc.frame().state->bake_settings.front_mm==80&&doc.copy_scene().encode()==recalled_scene,
      "Controller confirmation recalls the active recipe without modifying scene or rebaking");
  std::cout<<"PASS "<<cage_checks<<" texture-set Bake, Clear and Recall controls\n";
}
} // namespace

int main(int argc,char **argv)try{
  if(argc==3&&std::string_view(argv[1])=="--texture-set-bake"){
    texture_set_bake(argv[2]);return 0;
  }
  if(argc==4&&std::string_view(argv[1])=="--texture-set-xr-fixture"){
    auto scene=texture_set_bake_scene(.060f,.070f);
    if(std::getenv("DOME_BAKE_FIXTURE_MTOON")) {
      const auto material=active_texture_set(scene.active()).material;
      SourceMaterialBindings binding;
      binding.material=MToonMaterial::from_vrm1({
          {"pbrMetallicRoughness",{{"baseColorFactor",material.color}}},
          {"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"},
              {"shadeColorFactor",{.2f,.3f,.4f}}}}}}
      });
      binding.native_reference={material.color,material.emissive,int(material.alpha),
                                material.alpha_cutoff,material.double_sided};
      require_cage(bind_texture_set_mtoon(scene,scene.active().id,1,binding),
                   "The active source/cage fixture uses its own typed MToon material");
    }
    const auto source=scene.objects()[1].id;
    const auto cage=scene.objects()[2].id;
    const auto settings=texture_set_probe_settings(cage);
    bake_object(scene,source,settings);
    require_cage(edit_texture_set(scene,scene.active().id,1,[](PaintedMesh &paint,Material &) {
      paint.clear_bake();
      return true;
    }),"The XR seed retains its recipe after Clear");
    scene.save(argv[2]);
    auto reference=Scene::load(argv[2]);
    auto changed=settings;changed.front_mm=40;
    const auto original=reference.encode();
    require_cage(rejected_cage([&]{bake_object(reference,source,changed);})&&reference.encode()==original,
      "The altered controller range cannot reach the authored cage");
    bake_object(reference,source,settings);
    require_cage(dome::active_texture_set(reference.active()).painted->baked()&&
      !reference.active().painted->baked()&&!reference.active().bake_recipe,
      "The reference Bake affects only the selected additional field");
    reference.save(argv[3]);
    return 0;
  }
  if(argc==3&&(std::string_view(argv[1])=="--ui-fixture"||std::string_view(argv[1])=="--ui-wide-fixture")){
    auto scene=cage_scene(128);auto settings=cage_settings();if(std::string_view(argv[1])=="--ui-wide-fixture"){settings.front_mm=100000;settings.back_mm=0;settings.ao_distance_mm=.00001f;}bake_object(scene,2,settings);scene.active().painted->clear_bake();scene.save(argv[2]);return 0;
  }
  if(argc==4&&std::string_view(argv[1])=="--ui-supersample-reference"){
    auto scene=Scene::load(argv[2]);
    const auto recipe=scene.active().bake_recipe;
    require_cage(bool(recipe),"Supersampled fixture needs a saved source and projection recipe");
    auto settings=recipe->settings;
    settings.supersample=true;
    bake_object(scene,recipe->source.object,settings);
    scene.save(argv[3]);
    return 0;
  }
  if(argc==3&&std::string_view(argv[1])=="--inspect"){
    auto scene=Scene::load(argv[2]);std::cout<<nlohmann::json{{"scene",scene.encode()},{"sources",cage_sources(scene)},{"baked",bool(scene.active().painted->baked())}}.dump()<<'\n';return 0;
  }
  const auto out=std::filesystem::path(argc>1?argv[1]:"bake-recipes");std::filesystem::create_directories(out);
  auto scene=cage_scene();const auto source=scene.object(2).painted,cage=scene.object(3).painted;
  const auto options=cage_settings();bake_object(scene,2,options);
  const auto recipe=scene.active().bake_recipe;
  require_cage(recipe&&recipe->source.object==2&&recipe->source.original_object==2&&recipe->cage&&recipe->cage->object==3,
    "Successful native Bake records exact source and custom cage identities");
  require_cage(recipe->settings.front_mm==options.front_mm&&recipe->settings.padding==options.padding&&
    scene.object(2).painted==source&&scene.object(3).painted==cage,
    "Recipe captures actual projection settings without editing readonly source or cage owners");
  auto encoded=scene.encode();require_cage(encoded["version"]==30,"Actual recipe capability selects native Scene30");
  scene.save(out/"recipe.dsculpt");auto reopened=Scene::load(out/"recipe.dsculpt");
  require_cage(reopened.encode()==encoded&&reopened.active().bake_recipe->encode()==recipe->encode(),
    "Recipe, geometry, paint and complete source data restart exactly in the native project");
  auto sampling=*recipe;sampling.settings.supersample=true;
  auto sampling_record=sampling.encode();
  require_cage(!recipe->settings.supersample&&sampling_record["version"]==3&&
      ObjectBakeRecipe::decode(sampling_record)->settings.supersample,
      "Four-sample recipes round-trip while legacy recipes default to one sample");
  sampling_record["settings"]["supersample"]=false;
  require_cage(rejected_cage([&]{ObjectBakeRecipe::decode(sampling_record);}),
      "Version three requires a true, typed supersampling flag");
  sampling.cage.reset();sampling.settings.cage_object=0;sampling.settings.corresponding_topology=true;
  const auto matched=ObjectBakeRecipe::decode(sampling.encode());
  require_cage(matched->settings.supersample&&matched->settings.corresponding_topology,
      "Matching source triangles and supersampling can share one saved recipe");
  auto malformed=encoded;malformed["version"]=28;
  require_cage(rejected_cage([&]{Scene::decode(malformed);}),"Recipe fields cannot be relabelled as an older Scene capability");
  malformed=encoded;for(auto &o:malformed["objects"])o["bake_recipe"]=nullptr;
  require_cage(rejected_cage([&]{Scene::decode(malformed);}),"Scene30 requires actual recipe ownership rather than a version stamp");
  for(int kind=0;kind<5;++kind){auto invalid=recipe->encode();
    if(kind==0)invalid["source"]["object"]=-1;
    if(kind==1)invalid["source"]["name"]=std::string(257,'x');
    if(kind==2)invalid["settings"]["padding"]=65;
    if(kind==3)invalid["settings"]["allow_partial"]=1;
    if(kind==4)invalid["settings"]["normal_error"]=.5;
    require_cage(rejected_cage([&]{ObjectBakeRecipe::decode(invalid);}),"Invalid recipe IDs, names, integer quality, flags and tolerances refuse");
  }
  auto cleared=scene;cleared.active().painted=cleared.active().painted->snapshot();cleared.active().painted->clear_bake();
  require_cage(cleared.active().bake_recipe==recipe&&!cleared.active().painted->baked(),"Clear baked pixels retains the reusable recipe");
  auto missing=scene;missing.erase(2);missing.erase(3);
  require_cage(missing.active().bake_recipe->source.object==unavailable_bake_object&&
    missing.active().bake_recipe->source.original_object==2&&missing.active().bake_recipe->cage->object==unavailable_bake_object&&
    missing.active().bake_recipe->settings.cage_object==unavailable_bake_object,
    "Source/cage deletion preserves original identities as explicit unavailable references");
  missing.save(out/"unavailable.dsculpt");require_cage(Scene::load(out/"unavailable.dsculpt").encode()==missing.encode(),
    "Unavailable custom cage remains distinct from automatic projection after restart");
  auto appended=append_native_scene(Scene{},scene);
  const auto &rebased=*appended.scene.active().bake_recipe;
  require_cage(rebased.source.object==appended.objects.at(1)&&rebased.cage->object==appended.objects.at(2)&&
    rebased.source.original_object==2&&rebased.cage->original_object==3,
    "Actual native package append rebases live bindings and preserves original provenance");
  Scene destination;destination.add("Unrelated old source number",Volume{});destination.add("Unrelated old cage number",Volume{});
  auto subset=append_native_scene(destination,missing);
  require_cage(subset.scene.active().bake_recipe->source.object==unavailable_bake_object&&
    subset.scene.active().bake_recipe->cage->object==unavailable_bake_object,
    "Subset append cannot bind missing dependencies to unrelated coincident recipient IDs");
  auto retained=scene;retained.active().shape_source=capture_polygon_origin(retained,retained.active().id);retained.active().bake_recipe.reset();
  auto retained_json=retained.encode();require_cage(retained_json["version"]==30&&retained_json["polygon_sources"]["version"]==7,
    "A retained recipe selects the owning Scene30/history7 capability even without a current recipe");
  const auto retained_source=retained.active().shape_source;
  auto retained_appended=append_native_scene(destination,retained);
  const auto &retained_ref=*retained_appended.scene.active().shape_source->bake_recipe;
  require_cage(retained_ref.source.object==retained_appended.objects.at(1)&&
    retained_ref.cage->object==retained_appended.objects.at(2)&&
    retained_ref.source.original_object==2&&retained_ref.cage->original_object==3&&
    retained.active().shape_source==retained_source&&retained.encode()==retained_json,
    "Append rebases immutable retained recipe references while preserving the complete input history");
  auto grouped=retained;const auto source_group=grouped.add_group("Source dependencies");
  const auto child_group=grouped.add_group("Nested cage",{},source_group);
  grouped.object(2).group=source_group;grouped.object(3).group=child_group;
  const auto grouped_source=grouped.active().shape_source;
  auto ungrouped=grouped;ungrouped.erase_group(source_group,false);ungrouped.validate();
  require_cage(ungrouped.groups().empty()&&ungrouped.objects().size()==3&&
    ungrouped.active().shape_source==grouped_source&&
    ungrouped.active().shape_source->bake_recipe->source.object==2&&
    ungrouped.active().shape_source->bake_recipe->settings.cage_object==3,
    "Removing only a group preserves live dependencies and their shared immutable recipe owner");
  grouped.erase_group(source_group,true);grouped.validate();
  require_cage(grouped.groups().empty()&&grouped.objects().size()==1&&
    grouped.active().shape_source!=grouped_source&&
    grouped.active().shape_source->bake_recipe->source.object==unavailable_bake_object&&
    grouped.active().shape_source->bake_recipe->cage->object==unavailable_bake_object&&
    grouped_source->bake_recipe->source.object==2&&grouped_source->bake_recipe->cage->object==3,
    "Deleting nested dependency groups invalidates retained bindings with copy-on-write source history");
  grouped.save(out/"group-deleted.dsculpt");
  require_cage(Scene::load(out/"group-deleted.dsculpt").encode()==grouped.encode(),
    "Grouped dependency deletion and unavailable retained recipes restart exactly");
  NativeAuthoringExport portable;portable.scene=std::make_shared<const Scene>(scene.snapshot());
  portable.allow_portable_fallback=true;portable.scope=NativeAuthoringExport::Scope::WholeScene;
  auto read_native=[&](const std::filesystem::path &file){
    std::ifstream input(file,std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
    auto result=extract_native_authoring(bytes);if(!result)throw std::runtime_error("Recipe package has no native owner");return *result;
  };
  export_glb_authoring(portable,out/"recipe-whole.glb");
  require_cage(read_native(out/"recipe-whole.glb").encode()==scene.encode(),
    "Real native-authoring GLB retains complete hidden source, cage and recipe through its authenticated payload");
  portable.scope=NativeAuthoringExport::Scope::ExportedObjects;portable.objects={scene.active().id};
  export_glb_authoring(portable,out/"recipe-subset.glb");auto subset_recipe=read_native(out/"recipe-subset.glb");
  require_cage(subset_recipe.objects().size()==1&&
    subset_recipe.active().bake_recipe->source.object==unavailable_bake_object&&
    subset_recipe.active().bake_recipe->cage->object==unavailable_bake_object&&
    subset_recipe.active().bake_recipe->source.original_object==2,
    "Real exported-object package marks omitted source and cage unavailable instead of keeping invalid bindings");
  auto subset_rebound=append_native_scene(destination,subset_recipe);
  require_cage(subset_rebound.scene.active().bake_recipe->source.object==unavailable_bake_object&&
    subset_rebound.scene.active().bake_recipe->cage->object==unavailable_bake_object,
    "Importing the actual subset payload cannot bind missing dependencies to coincident destination IDs");
  retained=Scene::decode(retained_json);retained.erase(2);
  require_cage(retained.active().shape_source->bake_recipe->source.object==unavailable_bake_object,
    "Deletion invalidates live references throughout immutable retained source histories");
  auto restored=author_shapekey(retained,retained.active().id,{ShapeKeyAction::Restore});
  require_cage(restored.changed&&retained.active().bake_recipe&&retained.active().bake_recipe->source.object==unavailable_bake_object,
    "Complete original-source Restore carries its original recipe and current explicit unavailability");
  Document document;document.replace(reopened);ready_cage(document);
  require_cage(document.frame().state->bake_source==2&&document.frame().state->bake_settings.cage_object==3&&
    document.frame().state->bake_settings.front_mm==100&&!document.frame().state->can_undo,
    "Opening a project restores target-specific settings without adding history");
  Driver hand(document);hand.open();hand.sector(1);hand.value(.2f,0);hand.confirm();
  const auto changed=document.frame().state->bake_settings.front_mm;
  require_cage(changed!=100,"Actual controller changes the session projection distance before Recall");
  hand.sector(0);hand.ray(.5f,432.f/640);require_cage(document.frame().state->controls.bake_preset&&
    document.frame().state->bake_settings.front_mm==changed,"Actual ray Recall stages the full recipe without committing session settings");
  hand.ray(.25f,.95f);require_cage(document.frame().state->bake_settings.front_mm==changed&&document.copy_scene().encode()==encoded,
    "Cancel discards a recalled recipe draft with exact scene and current settings preserved");
  hand.sector(0);hand.ray(.5f,432.f/640);hand.confirm();
  require_cage(document.frame().state->bake_settings.front_mm==100&&document.frame().state->bake_source==2&&
    document.frame().state->bake_settings.cage_object==3&&document.copy_scene().encode()==encoded&&!document.frame().state->can_undo,
    "Done applies the full saved settings but never implicitly Bakes or edits the Scene");
  auto wide=cage_scene();auto wide_options=cage_settings();wide_options.front_mm=100000;wide_options.back_mm=0;wide_options.ao_distance_mm=.00001f;
  bake_object(wide,2,wide_options);const auto wide_before=wide.encode();document.replace(wide);ready_cage(document);Driver limits(document);limits.open();limits.sector(1);
  const auto distances=document.frame().state->controls;
  require_cage(distances.minimum==0&&distances.maximum==100000&&distances.logarithmic&&
    distances.value[0]==100000&&distances.value[1]==0&&distances.value[2]==wide_options.ao_distance_mm,
    "Opening XR distance controls preserves the complete native zero/tiny/100000mm recipe domain");
  limits.confirm();require_cage(document.frame().state->bake_settings.front_mm==100000&&document.frame().state->bake_settings.back_mm==0&&
    document.frame().state->bake_settings.ao_distance_mm==wide_options.ao_distance_mm&&document.copy_scene().encode()==wide_before&&!document.frame().state->can_undo,
    "Unchanged XR confirmation preserves every saved distance exactly without Scene or history mutation");
  limits.sector(1);limits.value(0,0);limits.value(1,1);limits.confirm();
  require_cage(document.frame().state->bake_settings.front_mm==0&&document.frame().state->bake_settings.back_mm==100000&&document.copy_scene().encode()==wide_before,
    "Actual controller rays reach exact zero and maximum endpoints without changing the saved Scene");
  document.replace(missing);ready_cage(document);auto frame=document.frame().state;
  require_cage(frame->bake_source==unavailable_bake_object&&frame->bake_settings.cage_object==unavailable_bake_object,
    "Opening missing-dependency recipe cannot fall back to another source or automatic cage");
  require_cage(rejected_cage([&]{document.bake_from_object(unavailable_bake_object,options);})&&
    document.frame().state==frame&&!frame->asset_busy,"Unavailable recipe refuses before caller admission and leaves the exact publication intact");
  std::cout<<"PASS "<<cage_checks<<" durable bake recipe checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
