// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-recipe.hpp"
#include "dome/texture-sets.hpp"
#include "dome/polygon-scene.hpp"
#include "texture-set-paint-access.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace {
using namespace dome;size_t checks=0;
void check(bool v,const char *why){if(!v)throw std::runtime_error(why);++checks;}
void refuses(auto action){try{action();}catch(const std::exception &e){std::cout<<"EXPECTED_RECIPE_REFUSAL "<<e.what()<<'\n';++checks;return;}throw std::runtime_error("Expected genuine recipe refusal");}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
Scene plain(){Scene s;s.active().mesh_active=true;s.active().painted=std::make_shared<PaintedMesh>(quad(),64);return s;}
std::vector<uint8_t> bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 auto legacy=plain();create_texture_set(legacy,legacy.active().id,std::array<uint64_t,1>{2},"Legacy plain set",64);const auto old=legacy.encode();
 check(old.at("version")==40&&old.at("texture_set_geometry").at("version")==1,"Existing ordinary Scene40 keeps its exact legacy wire form");check(Scene::decode(old).encode()==old,"Existing plain Scene40 replay remains exact");
 auto scene=plain();const auto actor=scene.active().id;const auto a=scene.add("Actual source A",Volume{}),b=scene.add("Actual source B",Volume{}),c=scene.add("Actual cage/source C",Volume{});scene.select(actor);
 ObjectBakeSettings settings;settings.normal_filter_quality=0;settings.emission_transport=0;scene.object(actor).bake_recipe=capture_bake_recipe(scene,a,settings);
 check(create_texture_set(scene,actor,std::array<uint64_t,1>{2},"Source-A material",64)==1,"Real creation captures the actual original source-A recipe");
 settings.cage_object=c;scene.object(actor).bake_recipe=capture_bake_recipe(scene,b,settings);
 const auto before=scene.encode();check(before.at("version")==40&&before.at("texture_set_geometry").at("version")==2,"Recipe-owning fields use the actual shared Scene40 codec");
 auto reopened=Scene::decode(before);check(reopened.encode()==before,"Primary and additional recipes roundtrip without wire changes");
 check(reopened.object(actor).bake_recipe->source.object==b&&reopened.object(actor).bake_recipe->settings.cage_object==c&&reopened.object(actor).texture_sets->additional().at(0).bake_recipe->source.object==a,"Distinct source and cage contexts survive in their actual fields");
 scene.object(actor).shape_source=capture_polygon_origin(scene,actor);settings.cage_object=0;scene.object(actor).bake_recipe=capture_bake_recipe(scene,c,settings);scene.validate();
 const auto nested=scene.encode();reopened=Scene::decode(nested);check(reopened.encode()==nested,"Different current and retained-history recipes share one collection without losing either context");
 const auto &current=reopened.object(actor);check(current.bake_recipe->source.object==c&&current.shape_source->bake_recipe->source.object==b&&current.texture_sets->additional().at(0).bake_recipe->source.object==a,"Current C, retained B and additional A remain independently editable contexts");
 const auto raster=detail::TextureSetPaintAccess::raster_owner(*current.painted),old_raster=detail::TextureSetPaintAccess::raster_owner(*current.shape_source->paint);
 check(raster==old_raster&&!raster.owner_before(old_raster)&&!old_raster.owner_before(raster),"Metadata variation preserves the actual shared raster control block");
 scene.save(out/"recipes.dsculpt");auto loaded=Scene::load(out/"recipes.dsculpt");loaded.save(out/"recipes-reopened.dsculpt");check(bytes(out/"recipes.dsculpt")==bytes(out/"recipes-reopened.dsculpt"),"Actual native Save/reopen is byte exact");
 auto &painted=nested.at("texture_set_geometry").at("paints");size_t index=0;while(index<painted.size()&&!painted[index].at("painted").contains("field_recipe"))++index;check(index<painted.size(),"Negative controls target a genuine emitted recipe record");
 auto corrupt=[&](auto change){auto bad=nested;change(bad["texture_set_geometry"]["paints"][index]["painted"]);refuses([&]{Scene::decode(bad);});};
 corrupt([](auto&j){j.erase("field_recipe");});corrupt([](auto&j){j["field_kind"]="foreign";});corrupt([](auto&j){j["field_recipe"]=nullptr;});corrupt([](auto&j){j["field_source"]=0;});corrupt([](auto&j){j["set"]=999;});corrupt([](auto&j){j["field_recipe"]["source"]["object"]=uint64_t(999999);});corrupt([](auto&j){j["field_recipe"]["settings"].erase("normal_error");});
 auto bad=nested;for(auto &o:bad["objects"])if(o["id"]==actor)o["bake_recipe"]=capture_bake_recipe(scene,a,settings)->encode();refuses([&]{Scene::decode(bad);});
 check(scene.encode()==nested,"All invalid wire controls leave the original native scene unchanged");
 std::cout<<"PASS "<<checks<<" texture-set recipe controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
