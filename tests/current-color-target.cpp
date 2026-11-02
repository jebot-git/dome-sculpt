// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-color-bake-fixture.hpp"
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
int main()try{
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
 auto bad=loaded.snapshot();auto wrong=std::make_shared<SourceColorPreparation>(*current.material_bindings->color_preparation);auto other=std::make_shared<PolygonColorSurface>(*saved);other->field=std::make_shared<const PolygonColorField>(*saved->field);wrong->polygon_colors=other;auto binding=std::make_shared<SourceMaterialBindings>(*current.material_bindings);binding->color_preparation=wrong;bad.object(id).material_bindings=binding;
 refused([&]{bad.validate(false);},"Cheap publication rejects equal-valued unowned replacement color authority");
 auto changed=strict->mesh();changed.positions[0].x+=.01f;refused([&]{detail::polygon_target_colors(changed,*descriptor);},"Equal-sized evaluated positions cannot replace exact REST identity");
 changed=strict->mesh();changed.uv[0][0]+=.001f;refused([&]{detail::polygon_target_colors(changed,*descriptor);},"Equal-sized reordered UV domain cannot acquire the prior projection");
 size_t calls=0;options.cancelled=[&]{return ++calls>80;};bool cancelled=false;try{prepare_source_colors(input,options);}catch(const std::runtime_error&){cancelled=true;}
 check(cancelled&&calls>80&&input.paint->encode()==input_before&&input.polygon_colors==descriptor&&descriptor->field==field&&Scene::decode(before).encode()==before,"Actual cancellable target preparation retains exact input paint/field owners and restartable source");
 std::cout<<checks<<" current-corner target preparation foundation checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
