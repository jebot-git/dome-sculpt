// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/shapekey-authoring.hpp"
#include "dome/source-placement.hpp"
#include "dome/polygon-scene.hpp"
#include "normal-preparation-fixtures.hpp"
#include "polygon-color-current-fixture.hpp"
#include "polygon-conversion.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char*message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
template<class F>void rejects(F f,const char*message){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,message);}
Scene colored(){
 auto surface=polygon_current_fixture::surface(true);Scene scene;auto &o=scene.active();
 o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(surface.mesh,64);o.material=surface.material;
 auto authority=std::make_shared<PolygonAuthority>();authority->topology=*surface.polygon_colors->topology;authority->colors=surface.polygon_colors->field;o.polygon=authority;
 auto m=AffineTransform{}.matrix();m[0]=-1.2;m[4]=.25;o.affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(m));scene.validate();return scene;
}
}
int main()try{
 auto scene=colored();const auto original=scene.encode();const auto rgba=scene.active().polygon->colors->packed(scene.active().polygon->topology);const auto world=scene.world_matrix(scene.active().id).matrix();
 check(original.at("version")==14&&original.at("polygon_sources").at("version")==3,"Affine prefix plus current corner colors retains the exact Scene14 declaration");
 author_shapekey(scene,scene.active().id,{});const auto authored=scene.encode();
 check(authored.at("version")==16&&authored.at("polygon_sources").at("version")==4&&authored.at("polygon_sources").contains("color_fields"),"Shape source roots and native corner colors coexist in archive4 without lowering feature minima");
 auto reopened=Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(authored)));
 check(reopened.encode()==authored&&reopened.world_matrix(reopened.active().id).matrix()==world,"Combined color, shape source and reflected/sheared placement survive exact CBOR restart");
 check(reopened.active().shape_source->polygon->colors==reopened.active().polygon->colors&&reopened.active().polygon->colors->packed(reopened.active().polygon->topology)==rgba,"Current and retained color aliases preserve exact linear RGBA and corner provenance");
 check(reopened.active().shape_source->placement&&reopened.active().shape_source->placement->world_matrix().matrix()==world,"The pre-shape capsule retains its own complete affine world authority");
 auto bad=authored;bad["polygon_sources"]["color_fields"].push_back(bad["polygon_sources"]["color_fields"][0]);rejects([&]{Scene::decode(bad);},"Archive4 rejects orphan color fields before admission");
 bad=authored;bad["polygon_sources"].erase("color_bases");rejects([&]{Scene::decode(bad);},"Archive4 refuses partial color tables rather than ignoring their retained contents");
 bad=authored;bad["polygon_sources"]["version"]=3;rejects([&]{Scene::decode(bad);},"Color archive3 cannot claim the direct shape-source edges of archive4");
 ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;author_shapekey(reopened,reopened.active().id,restore);check(reopened.encode()==original,"Restore recovers the exact before-shape colored affine scene after restart");
 Scene retained;retained.active().shape_source=scene.active().shape_source;const auto only=retained.encode();auto retained_again=Scene::decode(only);
 check(retained_again.encode()==only&&retained_again.active().shape_source->polygon->colors->packed(retained_again.active().shape_source->polygon->topology)==rgba,"A retained-only shape root still charges and preserves color and affine dependencies");
 auto normal=normal_preparation_fixture::model();auto &o=normal.active();auto prepared=prepare_source_normals({o.painted,o.appearance,o.material_bindings,o.deformation,o.material});o.painted=prepared.paint;o.material_bindings=prepared.material_bindings;normal.validate();const auto before_normal=normal.encode();
 check(before_normal.at("version")==15&&before_normal.at("source_materials").at("version")==4,"Normal preparation retains its Scene15/material4 feature declaration");
 o.shape_source=capture_polygon_origin(normal,o.id);const auto combined=normal.encode();auto normal_again=Scene::decode(combined);
 check(combined.at("version")==16&&normal_again.encode()==combined&&normal_again.active().shape_source->material_bindings->normal_preparation,"Archive4 retains complete normal preparation planes, recipes and original attributes");
 author_shapekey(normal_again,normal_again.active().id,restore);check(normal_again.encode()==before_normal,"Restoring a normal-prepared before-state recovers exact Scene15 data without recipe relabelling");
 std::cout<<"PASS "<<checks<<" combined authoring source checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
