// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-color-bake-fixture.hpp"
#include "dome/bake-authoring.hpp"
#include "dome/document.hpp"
#include <iostream>
using namespace dome;
using namespace current_color_bake_fixture;
namespace {
size_t checks=0;
void check(bool yes,const char *why){if(!yes)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<std::endl;}
template<class F>bool refuses(F &&f){try{f();return false;}catch(const std::exception&){return true;}}

}
int main(int argc,char **argv)try{
 if(argc==3&&std::string_view(argv[1])=="--inspect"){
  auto scene=Scene::load(argv[2]);auto value=scene.encode();auto isolated=nlohmann::json::array();
  for(const auto &o:scene.objects()){auto only=scene.snapshot();std::vector<uint64_t> erase;for(const auto &other:scene.objects())if(other.id!=o.id)erase.push_back(other.id);for(auto id:erase)only.erase(id);only.select(o.id);isolated.push_back(only.encode());}
  value["isolated_objects"]=std::move(isolated);std::cout<<value.dump()<<'\n';return 0;
 }
 if(argc==3&&std::string_view(argv[1])=="--ui-fixture"){fixture().save(argv[2]);return 0;}
 std::filesystem::path out=argc>1?argv[1]:"current-color-bake";std::filesystem::create_directories(out);
 for(bool seam:{false,true})for(bool posed:{false,true}){
  auto original=fixture(seam,posed);const auto source=original.objects().front().id;auto before=original.encode();auto source_value=source_only(original.snapshot(),source);auto field=original.object(source).polygon->colors;auto source_owner=field->source_owner();
  ObjectBakeSettings settings;settings.ao_samples=0;settings.padding=4;settings.front_mm=settings.back_mm=50;
  auto refused=original.snapshot();check(refuses([&]{bake_object(refused,source,settings);})&&refused.encode()==before,"Nonidentity current source requires explicit PBR color preparation without changing either object");
  settings.prepare_colors=true;auto direct=original.snapshot();auto report=bake_object(direct,source,settings);
  check(report.projected&&!report.missed&&direct.active().painted->baked()&&direct.active().painted->color_projection()&&direct.active().painted->baked()->color_interpretation==BakedColorInterpretation::ResolvedPbr,"Authenticated REST corner field projects into a separately declared prepared target color plane");
  check(source_only(direct.snapshot(),source)==source_value&&direct.object(source).polygon->colors==field&&field->source_owner()==source_owner,"Source current field, original basis, pose, paint, raw asset, shape and placement capsules stay exact");
  bool cancelled=false;auto stopped=original.snapshot();check(refuses([&]{bake_object(stopped,source,settings,[&]{return cancelled;},[&](float p){if(p>=.05f)cancelled=true;});})&&cancelled&&stopped.encode()==before,"Cancellation after actual current-field preparation leaves the complete original scene unchanged");
  bool final_callback=false;auto callback_failure=original.snapshot();
  check(refuses([&]{bake_object(callback_failure,source,settings,{},[&](float p){if(p==1){final_callback=true;throw std::runtime_error("Final source Bake callback failure");}});})&&final_callback&&callback_failure.encode()==before,"Throwing final quality callback cannot publish a changed detached source/target scene");
  bool late_cancel=false;auto cancelled_final=original.snapshot();
  check(refuses([&]{bake_object(cancelled_final,source,settings,[&]{return late_cancel;},[&](float p){if(p==1)late_cancel=true;});})&&late_cancel&&cancelled_final.encode()==before,"Final progress cancellation is arbitrated before the no-throw scene commit");
  Document d;d.replace(original.snapshot());check(d.wait_mesh()&&d.frame().state->can_bake&&std::find(d.frame().state->bake_sources.begin(),d.frame().state->bake_sources.end(),source)!=d.frame().state->bake_sources.end(),"Actual native source picker offers the supported current-field source before Bake");d.bake_from_object(source,settings);
  check(d.wait_asset()&&d.wait_mesh()&&d.frame().state->error.empty(),"Actual guarded asset worker accepts current-field source color baking");auto accepted=d.copy_scene().encode();
  check(d.undo()&&d.copy_scene().encode()==before&&!d.frame().state->can_undo&&d.redo()&&d.copy_scene().encode()==accepted,"Current-source Bake is one exact Undo/Redo operation with no source conversion");
  auto path=out/("case-"+std::to_string(seam)+"-"+std::to_string(posed)+".dsculpt");d.save(path);Document reopened;reopened.open(path);check(reopened.wait_asset()&&reopened.wait_mesh()&&reopened.copy_scene().encode()==accepted,"Saved current-source baked target restarts with the complete original source authority");
  {auto glb=path;glb.replace_extension(".glb");reopened.export_asset(glb).get();check(std::filesystem::file_size(glb)>1000,"Actual current-source Bake exports its resolved target through existing frame guards");}
 }
 {
  auto scene=fixture();auto source=scene.objects().front().id;auto before=scene.encode();scene.select(source);scene.object(source).visible=true;scene.object(source).locked=false;auto target_field=scene.encode();ObjectBakeSettings settings;settings.prepare_colors=false;settings.ao_samples=0;
  check(refuses([&]{bake_object(scene,scene.objects().back().id,settings);})&&scene.encode()==target_field,"Current-field target requires explicit projection preparation before accepting a nonidentity field");
 }
 {
  auto scene=fixture();auto source=scene.objects().front().id;auto &target=scene.active();const auto &mesh=target.painted->mesh();DeformationState state(mesh);state.morphs=MorphSet(mesh);auto positions=mesh.positions;for(auto &p:positions)p.z+=.02f*p.x;state.morphs->set_target(mesh,"Target mode authority",positions);state.morph_weights={0};scene.set_deformation(target.id,std::move(state));auto before=scene.encode();
  Document d;d.replace(scene);check(d.wait_mesh()&&d.frame().state->can_bake,"Saved rest target with a shapekey advertises valid Bake before key-edit mode");d.configure_shape_sculpt(true);
  check(d.wait_mesh()&&d.frame().state->shape_sculpt&&!d.frame().state->can_bake,"Selected-key preview disables Bake readiness rather than substituting saved rest positions");
  check(refuses([&]{ObjectBakeSettings settings;settings.prepare_colors=true;d.bake_from_object(source,settings);})&&d.copy_scene().encode()==before&&!d.frame().state->can_undo,"Direct native Bake refuses selected-key preview without source/target/history mutation");
  d.configure_shape_sculpt(false);check(d.wait_mesh()&&d.frame().state->can_bake&&d.copy_scene().encode()==before,"Finishing target key-edit mode restores valid Bake and exact saved authoring state");
 }
 std::cout<<checks<<" current-corner source Bake checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
