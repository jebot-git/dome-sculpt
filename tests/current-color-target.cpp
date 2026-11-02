// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-color-target-fixture.hpp"
#include "dome/bake-authoring.hpp"
#include "dome/document.hpp"
#include "dome/color-preparation.hpp"
#include "polygon-paint.hpp"
#include "polygon-color-target.hpp"
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool pass,const char *why){if(!pass)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<'\n';}
template<class F>void refused(F f,const char *why){bool failed=false;try{f();}catch(const std::invalid_argument&){failed=true;}check(failed,why);}
}
int main(int argc,char **argv)try{
 if(argc==3&&std::string_view(argv[1])=="--ui-fixture"){current_color_target_fixture::fixture().save(argv[2]);return 0;}
 if(argc==3&&std::string_view(argv[1])=="--inspect"){
  auto scene=Scene::load(argv[2]);auto encoded=scene.encode();auto isolated=nlohmann::json::array();
  for(const auto &o:scene.objects())isolated.push_back(current_color_bake_fixture::source_only(scene.snapshot(),o.id));
  encoded["isolated_objects"]=std::move(isolated);std::cout<<encoded.dump()<<'\n';return 0;
 }
 auto scene=current_color_bake_fixture::fixture(true,false);const auto id=scene.objects().front().id;
 auto &target=scene.object(id);target.visible=true;target.locked=false;scene.select(id);
 auto before=scene.encode();auto field=target.polygon->colors;
 PaintSurfaceOptions surface_options;
 auto strict=polygon_paint_geometry(*target.painted,target.painted->mesh(),surface_options);
 auto descriptor=std::make_shared<PolygonColorSurface>();descriptor->topology=std::make_shared<const PolygonMesh>(target.polygon->topology);descriptor->field=field;
 ColorPreparationOptions options;options.resolve_identity=true;
 ColorPreparationInput input{strict,target.appearance,target.material_bindings,target.deformation,target.material,descriptor};
 const auto input_before=input.paint->encode();
 auto output=prepare_source_colors(input,options);
 check(output.converted&&output.paint->color_projection()&&output.material_bindings->color_preparation->polygon_colors->field==descriptor->field&&output.material_bindings->color_preparation->polygon_colors.get()!=descriptor.get(),"Detached preparation produces an immutable geometry-owned current-corner projection descriptor");
 check(scene.encode()==before&&target.polygon->colors==field,"Detached target preparation retains complete input and immutable current field");
 const auto &recipe=*output.material_bindings->color_preparation;
 check(recipe.receipt.version==2&&recipe.receipt.source_field==SourceColorField::PolygonCorners,"Current corners use their own versioned receipt, never Identity or a fake raw accessor");
 check(recipe.original_paint->mesh().uv==target.painted->mesh().uv&&recipe.original_paint->layers().size()==target.painted->layers().size(),"Preparation retains same UV and every original editable paint layer");
 recipe.validate_current(strict->mesh(),target.appearance.get());check(true,"Projection proves the actual ordered REST field and accepted analytic quality bound");
 target.painted=output.paint;target.material_bindings=output.material_bindings;scene.validate();
 auto encoded=scene.encode();check(encoded.at("version")==20&&encoded.at("source_materials").at("version")==6&&encoded.at("polygon_sources").at("version")==5,"New required authority is explicitly Scene20/material6/polygon5");
 auto loaded=Scene::decode(encoded);check(loaded.encode()==encoded,"Current target geometry/projection/source data survives exact native restart");
 auto &current=loaded.object(id);const auto saved=current.material_bindings->color_preparation->polygon_colors;
 check(saved->field==current.polygon->colors&&saved->field->source_owner()==current.polygon->colors->source_owner(),"Restart deduplicates exact field and initial immutable corner basis owners");
 auto old=encoded;old["version"]=19;refused([&]{Scene::decode(old);},"Older outer version cannot silently ignore current-corner preparation");
 auto legacy=encoded;legacy["source_materials"]["version"]=5;refused([&]{Scene::decode(legacy);},"Legacy material version cannot acquire undeclared current-corner authority");
 auto unreferenced=encoded;unreferenced["polygon_sources"]["color_fields"].push_back(unreferenced["polygon_sources"]["color_fields"][0]);refused([&]{Scene::decode(unreferenced);},"Unreachable color fields are rejected before bulk paint decoding");
 auto orphan_recipe=encoded;orphan_recipe["source_materials"]["color_preparations"].push_back(orphan_recipe["source_materials"]["color_preparations"][0]);refused([&]{Scene::decode(orphan_recipe);},"Unreferenced corner preparation cannot smuggle retained paint or field authority");
 auto orphan_paint=encoded;orphan_paint["source_materials"]["paints"].push_back(orphan_paint["source_materials"]["paints"][0]);refused([&]{Scene::decode(orphan_paint);},"Unreferenced corner before-paint records are refused before SurfaceMap construction");
 auto wrong_history=encoded;wrong_history["source_materials"]["color_preparations"][0]["before_binding"]=0;refused([&]{Scene::decode(wrong_history);},"Self-referential material history is rejected during bounded preflight");
 auto mismatch=encoded;mismatch["source_materials"]["color_preparations"][0]["polygon_colors"]["topology"]["faces"][0]["corners"][0][2][0]=.01;
 refused([&]{Scene::decode(mismatch);},"Serialized descriptor cannot borrow an equal-sized current mesh with different corner UVs");
 auto field_alias=encoded;auto &fields=field_alias["polygon_sources"]["color_fields"];fields.push_back(fields[0]);auto index=field_alias["objects"][0]["polygon"].get<size_t>();field_alias["polygon_sources"]["authorities"][index]["colors"]=fields.size()-1;
 refused([&]{Scene::decode(field_alias);},"Equal-valued serialized field records cannot replace the exact shared projection owner");
 auto quota=encoded;auto &recipes=quota["source_materials"]["color_preparations"];while(recipes.size()<257)recipes.push_back(recipes[0]);bool bounded=false;try{Scene::decode(quota);}catch(const std::length_error&){bounded=true;}check(bounded,"Aggregate material node quota rejects257 corner recipes before bulk decoding");
 auto bad=loaded.snapshot();auto wrong=std::make_shared<SourceColorPreparation>(*current.material_bindings->color_preparation);auto other=std::make_shared<PolygonColorSurface>(*saved);other->field=std::make_shared<const PolygonColorField>(*saved->field);wrong->polygon_colors=other;auto binding=std::make_shared<SourceMaterialBindings>(*current.material_bindings);binding->color_preparation=wrong;bad.object(id).material_bindings=binding;
 refused([&]{bad.validate(false);},"Cheap publication rejects equal-valued unowned replacement color authority");
 auto changed=strict->mesh();changed.positions[0].x+=.01f;refused([&]{detail::polygon_target_colors(changed,*descriptor);},"Equal-sized evaluated positions cannot replace exact REST identity");
 changed=strict->mesh();changed.uv[0][0]+=.001f;refused([&]{detail::polygon_target_colors(changed,*descriptor);},"Equal-sized reordered UV domain cannot acquire the prior projection");
 size_t calls=0;options.cancelled=[&]{return ++calls>80;};bool cancelled=false;try{prepare_source_colors(input,options);}catch(const std::runtime_error&){cancelled=true;}
 check(cancelled&&calls>80&&input.paint->encode()==input_before&&input.polygon_colors==descriptor&&descriptor->field==field&&Scene::decode(before).encode()==before,"Actual cancellable target preparation retains exact input paint/field owners and restartable source");
 options.cancelled={};options.max_projection_error=.001f;
 refused([&]{prepare_source_colors(input,options);},"Actual default64 atlas refuses a stricter-than-representable projection bound without silently degrading current colors");
 check(input.paint->encode()==input_before&&input.polygon_colors==descriptor,"Quality refusal retains original editable paint and current field exactly");
 {
  auto empty=loaded.snapshot();auto object=empty.active();PolygonEdit erase;erase.operation=PolygonOperation::DeleteFaces;erase.selection.mode=PolygonSelectionMode::Face;for(const auto &face:object.polygon->topology.faces())erase.selection.ids.push_back(face.id);
  auto edit=edit_colored_polygon(object.polygon->topology,*object.polygon->colors,erase);
  adopt_polygon_edit(empty,object.id,*edit.geometry.replacement,std::make_shared<const PolygonColorField>(*edit.colors),object,empty.world_transform(object.id),false);
  check(!empty.active().painted&&!empty.active().mesh_active&&empty.active().polygon->origin->material_bindings->color_preparation->polygon_colors->field==loaded.active().polygon->colors,"Delete-all captures complete prepared current field in its own retained Origin without stale Volume fallback");
  auto retained=empty.encode();check(retained.at("version")==20&&Scene::decode(retained).encode()==retained,"Recursive Scene20 feature and shared field authority survive only in a retained Origin");
  auto restart=Scene::decode(retained);restore_polygon_origin(restart,object.id);check(restart.encode()==loaded.encode(),"Restoring a saved empty authority recovers exact prepared geometry, field, layers and material interpretation");
 }

 for(bool seam:{false,true}){
  auto original=current_color_target_fixture::fixture(seam);const auto source=original.objects().back().id;
  auto before=original.encode(),source_before=current_color_bake_fixture::source_only(original.snapshot(),source);
  auto authority=original.active().polygon;auto field=authority->colors;auto shape=original.active().shape_source;
  ObjectBakeSettings settings;settings.ao_samples=0;settings.padding=4;
  Document document;document.replace(original.snapshot());check(document.wait_mesh()&&document.frame().state->can_bake,"Actual native target readiness and source picker admit supported current corners");
  document.bake_from_object(source,settings);check(document.wait_asset()&&!document.frame().state->error.empty()&&document.copy_scene().encode()==before&&!document.frame().state->can_undo,"Native target Bake requires explicit preparation and preserves exact input/history on refusal");
  settings.prepare_colors=true;document.bake_from_object(source,settings);check(document.wait_asset()&&document.wait_mesh()&&document.frame().state->error.empty(),"Actual guarded native worker prepares and bakes a current-corner target");
  auto baked=document.copy_scene();auto accepted=baked.encode();auto preparation=baked.active().material_bindings->color_preparation;
  check(accepted.at("version")==20&&preparation->polygon_colors->field==field&&baked.active().polygon->colors==field&&baked.active().shape_source==shape,"Native target keeps actual immutable current field, shape source and Scene20 receipt");
  check(baked.active().polygon->origin->polygon==authority&&baked.active().polygon->origin->paint->encode()==original.active().painted->encode(),"Actual same-mesh strict conversion retains complete prior PA authority and paint in its own Origin");
  check(current_color_bake_fixture::source_only(baked.snapshot(),source)==source_before,"Native target conversion never mutates independently archived hidden read-only source");
  check(document.undo()&&document.copy_scene().encode()==before&&!document.frame().state->can_undo&&document.redo()&&document.copy_scene().encode()==accepted,"Target strict conversion, projection and real Bake are one exact native Undo/Redo");
  auto path=std::filesystem::path(argc>1?argv[1]:"current-color-target");std::filesystem::create_directories(path);path/="target-"+std::to_string(seam)+".dsculpt";document.save(path);
  Document reopened;reopened.open(path);check(reopened.wait_asset()&&reopened.wait_mesh()&&reopened.copy_scene().encode()==accepted,"Actual target project Save and native restart retain all current/original authorities");
  auto glb=path;glb.replace_extension(".glb");reopened.export_asset(glb).get();check(std::filesystem::file_size(glb)>1000,"Actual current-field target exports through unchanged normal-frame representability guards");
  settings.prepare_colors=false;reopened.bake_from_object(source,settings);check(reopened.wait_asset()&&reopened.wait_mesh()&&reopened.frame().state->error.empty()&&reopened.copy_scene().active().material_bindings->color_preparation->receipt.encode()==preparation->receipt.encode(),"Repeat native Bake reuses its actual current-corner preparation without recapture or fake Identity");
  reopened.restore_polygon_source();check(reopened.wait_asset()&&reopened.wait_mesh()&&reopened.frame().state->error.empty()&&reopened.copy_scene().encode()==before,"Actual native restore after reopening returns complete pre-preparation PA/current-field source exactly");
 }
 std::cout<<checks<<" current-corner target preparation/native checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
