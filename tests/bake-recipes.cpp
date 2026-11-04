// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_BAKE_CAGE_HELPERS_ONLY
#include "bake-cage.cpp"
#include "dome/bake-recipe.hpp"
#include "dome/native-scene-append.hpp"
#include "dome/polygon-conversion.hpp"
#include "dome/shapekey-authoring.hpp"
int main(int argc,char **argv)try{
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
  document.replace(missing);ready_cage(document);auto frame=document.frame().state;
  require_cage(frame->bake_source==unavailable_bake_object&&frame->bake_settings.cage_object==unavailable_bake_object,
    "Opening missing-dependency recipe cannot fall back to another source or automatic cage");
  require_cage(rejected_cage([&]{document.bake_from_object(unavailable_bake_object,options);})&&
    document.frame().state==frame&&!frame->asset_busy,"Unavailable recipe refuses before caller admission and leaves the exact publication intact");
  std::cout<<"PASS "<<cage_checks<<" durable bake recipe checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
