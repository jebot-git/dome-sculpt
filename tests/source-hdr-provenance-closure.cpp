// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-authoring.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/polygon-scene.hpp"
#include "emission-source-closure.hpp"
#include "baked-emission-domain.hpp"
#include "polygon-conversion.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace {
using namespace dome;
size_t checks=0;
void check(bool v,const char*why){if(!v)throw std::runtime_error(why);++checks;}
bool rejected(auto action){try{action();return false;}catch(const std::exception&e){std::cout<<"EXPECTED_CLOSURE_REFUSAL "<<e.what()<<'\n';return true;}}
bool contains(const Scene&s,uint64_t id){return std::any_of(s.objects().begin(),s.objects().end(),[&](const auto&o){return o.id==id;});}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
std::vector<uint8_t> bytes(const std::filesystem::path&p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
uint64_t add(Scene&s,const char*name,uint32_t resolution=64){const auto id=s.add(name,Volume{});auto&o=s.object(id);o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(quad(),resolution);return id;}
void dependency_graph(){
 Scene s;const auto source=s.active().id;s.active().mesh_active=true;s.active().painted=std::make_shared<PaintedMesh>(quad(),64);
 const auto first=add(s,"Actual first bake source"),cage=add(s,"Actual cage"),unrelated=add(s,"Unrelated2048 destination",2048),nested=add(s,"Nested shape history source"),slot_source=add(s,"Additional texture-set bake source");
 ObjectBakeSettings historical;historical.normal_filter_quality=0;historical.emission_transport=0;historical.ao_samples=0;
 auto current=historical;current.cage_object=cage;
 s.object(source).bake_recipe=capture_bake_recipe(s,nested,historical);const auto origin=capture_polygon_origin(s,source);
 s.object(source).bake_recipe=capture_bake_recipe(s,slot_source,historical);
 const auto slot=create_texture_set(s,source,std::array<uint64_t,1>{2},"Retained source slot",64);
 check(slot==1&&s.object(source).texture_sets->additional().at(0).bake_recipe->source.object==slot_source,"Genuine material-slot creation preserves its independently captured live source recipe");
 s.object(source).bake_recipe=capture_bake_recipe(s,first,current);s.object(source).shape_source=origin;
 const auto parent=s.add_group("Kept source ancestor"),leaf=s.add_group("Kept source child",{},parent),discard=s.add_group("Unrelated group");s.object(source).group=leaf;s.object(unrelated).group=discard;s.group(parent).transform.position={.125f,.25f,-.5f};s.select(unrelated);s.validate();const auto original=s.encode();
 const auto context=detail::emission_source_closure(s,source);check(context.objects().size()==5&&!contains(context,unrelated),"Closure excludes the unrelated2048 raster and retains all five genuine source dependencies");for(auto id:{source,first,cage,nested,slot_source})check(contains(context,id),"Current/cage/polygon/texture-set live dependencies survive");check(context.groups().size()==2&&context.group(leaf).parent==parent&&context.world_matrix(source).float_matrix()==s.world_matrix(source).float_matrix(),"Exact source parent chain and world placement survive without unrelated groups");check(context.object(source).shape_source==s.object(source).shape_source&&context.object(source).texture_sets==s.object(source).texture_sets,"Dependency selection preserves complete immutable polygon and texture-set owners");check(s.encode()==original,"Closure selection does not mutate any original object/history/group");
 auto missing=s;auto forged=std::make_shared<ObjectBakeRecipe>(*missing.object(source).bake_recipe);forged->source.object=999999;missing.object(source).bake_recipe=forged;check(rejected([&]{detail::emission_source_closure(missing,source);}),"Missing live source dependency refuses rather than remapping it unavailable");
 auto hidden=s;hidden.object(source).shape_source=capture_polygon_origin(s,unrelated);const auto hidden_context=detail::emission_source_closure(hidden,source);
 check(!contains(hidden_context,unrelated)&&rejected([&]{detail::BakedEmissionDomain::preflight(hidden_context);}),"A detached2048 before-raster remains fully charged even without its old live object");
 bool stop=true;check(rejected([&]{detail::emission_source_closure(s,source,[&]{return stop;});})&&s.encode()==original,"Cancellation preserves the full original scene");
}
}
int main(int argc,char**argv)try{
 if(argc!=4)return 2;const std::filesystem::path before_file=argv[1],legacy_file=argv[2],out=argv[3];std::filesystem::create_directories(out);auto full=Scene::load(before_file);const auto target=full.active().id;uint64_t source=0;for(const auto&o:full.objects())if(o.id!=target&&o.painted&&o.painted->has_physical_emission()){check(!source,"The actual high-resolution joint input has one physical source");source=o.id;}check(source&&full.active().painted->resolution()==2048&&full.object(source).painted->resolution()==64,"Actual original failed input retains source64 and selected target2048");const auto original=full.encode();
 check(rejected([&]{detail::BakedEmissionDomain::preflight(full);}),"The original whole-scene quota failure remains a real bounded refusal");const auto context=detail::emission_source_closure(full,source);check(!contains(context,target)&&detail::BakedEmissionDomain::preflight(context)<32*1024*1024,"Only required source provenance is charged to the unchanged32MiB payload cap");
 const auto owner=detail::BakedEmissionDomain::capture(full,source,full.active().painted->mesh_owner(),2048,full.world_transform(target));check(owner->field()->resolution()==64&&owner->target_resolution()==2048&&owner->storage_bytes()<32*1024*1024,"Genuine certified source64 can now coexist with independent2048 target");auto encoded=owner->encode();const auto replay=detail::BakedEmissionDomain::decode(encoded,full.active().painted->mesh_owner(),2048);check(replay->encode()==encoded,"Minimal source-context native replay is exact");check(full.encode()==original,"Source capture never replaces or drops the full outer project");
 auto missing=encoded;missing["source"]=uint64_t(999999);check(rejected([&]{detail::BakedEmissionDomain::decode(missing,full.active().painted->mesh_owner(),2048);}),"Foreign source ID cannot borrow the retained context");
 auto legacy=Scene::load(legacy_file);const auto baked=legacy.active().painted?legacy.active().painted->baked():nullptr;const auto old_owner=baked?baked->emission_domain:nullptr;
 if(!old_owner)throw std::invalid_argument("Historical HDR replay requires an actual retained source owner");
 const auto old_context=old_owner->encode().at("before");const auto old_target=legacy.active().id;
 const bool retained_destination=old_context.at("active")==old_target&&std::any_of(old_context.at("objects").begin(),old_context.at("objects").end(),[&](const auto&o){return o.at("id")==old_target;});
 legacy.save(out/"legacy-reopened.dsculpt");check(retained_destination&&bytes(legacy_file)==bytes(out/"legacy-reopened.dsculpt"),"Actual old Scene43 retains its independent destination context and reopens/saves byte exactly; a current source-only seed is not historical proof");dependency_graph();std::cout<<"PASS "<<checks<<" HDR source dependency-closure controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
