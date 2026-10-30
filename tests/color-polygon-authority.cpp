// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/color-preparation.hpp"
#include "dome/source-uv-bake.hpp"
#include "dome/import.hpp"
#include "polygon-color-preparation.hpp"
#include "polygon-scene-budget.hpp"
#include "polygon-fixture.hpp"
#include "source-color-fixtures.hpp"
#include <fstream>
#include <iostream>
#include <numeric>
#ifdef DOME_TEST_ONEAPI
#include <sycl/sycl.hpp>
#endif
using namespace dome;
namespace {
unsigned checks=0;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
template<class F> bool refuses(F f,std::string_view diagnostic={}) {
  try {f();return false;} catch(const std::exception &e) {
    if(!diagnostic.empty()&&std::string_view(e.what()).find(diagnostic)==std::string_view::npos)
      throw std::runtime_error(std::string("Wrong refusal: ")+e.what());
    return true;
  }
}
ColorPreparationInput input(const SculptObject &o){return {o.painted,o.appearance,o.material_bindings,o.deformation,o.material};}
ColorPreparationResult prepare(const SculptObject &o){ColorPreparationOptions options;options.resolve_identity=true;return prepare_source_colors(input(o),options);}
Image image(unsigned r,std::array<uint8_t,4> color){Image out{r,r,std::vector<uint8_t>(size_t(r)*r*4)};for(size_t k=0;k<out.rgba.size();++k)out.rgba[k]=color[k%4];return out;}
void paint(PaintedMesh &p){p.begin_stroke();Brush b;b.from=b.to={0,0,.5f};b.radius=2;PaintSettings ink;ink.front_only=false;ink.color={.1f,.9f,.25f,.8f};p.stamp(b,ink);p.end_stroke();}
Scene with_origin(const SculptObject &o) {
  auto s=polygon_test::fixture();auto source=std::make_shared<PolygonOrigin>();
  source->volume=std::make_shared<const Volume>(o.volume->geometry_snapshot());source->paint=o.painted;
  source->material=o.material;source->appearance=o.appearance;source->deformation=o.deformation;
  source->material_bindings=o.material_bindings;source->retopology=o.retopology;
  source->mesh_active=o.mesh_active;source->name="Prepared prior source";source->source_object=o.id;
  auto authority=std::make_shared<PolygonAuthority>(*s.active().polygon);authority->origin=source;s.active().polygon=authority;
  return s;
}
Scene changed_origin(Scene s,const std::function<void(PolygonOrigin&)> &edit) {
  auto authority=std::make_shared<PolygonAuthority>(*s.active().polygon);
  auto source=std::make_shared<PolygonOrigin>(*authority->origin);edit(*source);authority->origin=source;s.active().polygon=authority;return s;
}
Scene native_fixture(bool overlap=false) {
  auto s=polygon_test::fixture();const auto target=s.active().id;
  auto original=s.active().painted->mesh();auto high=original;
  for(size_t i=0;i<high.positions.size();++i){high.positions[i]+=high.normals[i]*.02f;
    const auto n=high.normals[i];Vec3 axis=std::abs(n.y)<.8f?Vec3{0,1,0}:Vec3{1,0,0};high.normals[i]=normalized(n+normalized(cross(axis,n))*.35f);}
  const auto source=s.add("Read-only prepared high source",Volume{});auto &o=s.object(source);
  o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(high,64);o.material.color={.2f,.6f,.9f,.85f};
  auto prepared=prepare(o);o.painted=prepared.paint;o.material_bindings=prepared.material_bindings;
  auto authority=std::make_shared<PolygonAuthority>(*s.object(target).polygon);
  if(overlap)authority->topology=PolygonMesh::cube();
  PaintSurfaceOptions options;options.policy=PaintUvPolicy::PolygonAuthored;
  s.object(target).painted=polygon_paint_geometry(*s.object(target).painted,authority->topology.triangulate().mesh,options);
  s.object(target).polygon=authority;s.select(target);s.validate();return s;
}
void admission_and_origins() {
  auto s=native_fixture();auto before=s.encode();const auto &o=s.active();
  check(!prepare_source_colors(input(o)).converted&&s.encode()==before,"An unchanged no-color polygon input remains a no-op without minting a receipt");
  check(refuses([&]{prepare(o);},"explicit strict conversion")&&s.encode()==before,"Standalone color preparation cannot borrow a Scene polygon witness or change source paint");
  SourceUvBakeInput uv;uv.paint=o.painted;uv.target=o.painted->mesh();uv.source_vertices.resize(uv.target.positions.size());std::iota(uv.source_vertices.begin(),uv.source_vertices.end(),0);
  check(refuses([&]{bake_source_uv(uv);},"owning authority")&&s.encode()==before,"Source UV origins cannot silently capture polygon-authored paint without its direct authority");
  auto colored=source_color_fixture::model();auto converted=prepare(colored.active());
  auto raw=std::make_shared<SourceColorPreparation>(*converted.material_bindings->color_preparation);
  PaintSurfaceOptions options;options.policy=PaintUvPolicy::PolygonAuthored;
  raw->original_paint=polygon_paint_geometry(*raw->original_paint,raw->original_paint->mesh(),options);
  check(refuses([&]{raw->validate(false);},"polygon authority cannot be inferred")&&refuses([&]{raw->validate();},"polygon authority cannot be inferred"),"Cheap and deep receipt validation both reject polygon paint in original_paint");
  raw=std::make_shared<SourceColorPreparation>(*converted.material_bindings->color_preparation);
  auto prior=std::make_shared<SourceMaterialBindings>(*raw->original_material);auto origin=std::make_shared<SourceUvOrigin>(*prior->uv_origin);
  origin->paint=polygon_paint_geometry(*origin->paint,origin->paint->mesh(),options);prior->uv_origin=origin;raw->original_material=prior;
  check(refuses([&]{raw->validate(false);},"Source UV origin paint"),"A cheap color-receipt publication also checks PA hidden in beforeBinding's UV origin");
  colored.active().painted=converted.paint;colored.active().material_bindings=converted.material_bindings;
  auto retained=with_origin(colored.active());retained.validate();auto encoded=retained.encode();
  auto restored=Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(encoded)));
  check(encoded["version"]==12&&restored.encode()==encoded,"Scene12 retains a genuine prepared Strict source with exact color3 receipt, images and beforeBinding");
  auto missing=changed_origin(retained,[](auto &p){p.material_bindings.reset();});
  check(refuses([&]{missing.validate(false);},"Polygon source color projection")&&refuses([&]{missing.encode();},"Polygon source color projection"),"Retained prepared paint without its matching receipt is refused before cheap publication or save");
  auto unprepared=changed_origin(retained,[](auto &p){p.paint=p.material_bindings->color_preparation->original_paint;});
  check(refuses([&]{unprepared.validate(false);},"Polygon source color projection"),"A retained receipt cannot be attached to an unprepared current source paint");
  auto no_accessor=changed_origin(retained,[](auto &p){p.appearance.reset();});
  check(refuses([&]{no_accessor.validate();},"original accessor authority"),"Deep retained-source validation verifies its current appearance, not merely the initial raw receipt");
  auto remapped=changed_origin(retained,[](auto &p){auto a=std::make_shared<CreatorAppearance>(*p.appearance);std::swap(a->source_vertices[0],a->source_vertices[2]);p.appearance=a;});
  check(refuses([&]{remapped.validate();},"recorded source-field error bound"),"A same-count remapped retained accessor cannot impersonate its recorded current projected field");
  auto bad=encoded;bad["polygon_sources"]["origins"][0]["material_bindings"]=nullptr;
  check(refuses([&]{Scene::decode(bad);},"Polygon source color projection"),"Actual Scene12 decoding refuses a retained projected source with its binding removed");
  bad=encoded;auto &p=bad["polygon_sources"]["paints"][0];auto im=p["color_projection"]["image"].get<size_t>();
  bad["source_images"].push_back(bad["source_images"][im]);p["color_projection"]["image"]=bad["source_images"].size()-1;
  check(refuses([&]{Scene::decode(bad);},"recorded prepared field"),"Equal-valued but differently owned serialized images cannot bypass retained paint/receipt pairing");
  bad=encoded;auto &sources=bad["source_materials"];auto paint_id=sources["color_preparations"][0]["paint"].get<size_t>();sources["paints"][paint_id]["uv_policy"]="polygon-authored";
  check(refuses([&]{Scene::decode(bad);},"Source UV origins"),"A direct unrelated polygon witness never admits PA smuggled into a color receipt's original paint table");
}
void budgets() {
  auto native=polygon_test::fixture();auto state=prepare(native.active());paint(*state.paint);
  BakedTextures baked;baked.maps.color=image(64,{200,30,40,255});baked.maps.metal_rough=image(64,{255,128,0,255});baked.maps.emission=image(64,{0,0,0,255});baked.maps.pbr_color=image(64,{20,40,220,180});baked.coverage.assign(64*64,255);baked.color_interpretation=BakedColorInterpretation::ResolvedPbr;state.paint->install_bake(baked);
  auto p=state.paint->encode();size_t pbr_tiles=0;for(const auto &l:p["layers"])pbr_tiles+=l["pbr_color_tiles"].size();
  detail::PolygonSceneBudget full;full.paint(p,true);auto stripped=p;for(auto &l:stripped["layers"])l.erase("pbr_color_tiles");stripped["baked"].erase("pbr_color");detail::PolygonSceneBudget ordinary;ordinary.paint(stripped,true);
  const size_t extra=pbr_tiles*sizeof(PaintColorTile)+p["baked"]["pbr_color"].get_binary().size();
  check(pbr_tiles>0&&full.paint_bytes==ordinary.paint_bytes+extra,"Retained budget charges every independent PBR paint tile and baked plane exactly once");
  constexpr size_t limit=256*1024*1024;
  detail::PolygonSceneBudget near;near.paint_bytes=limit-full.paint_bytes;near.paint(p,true);check(near.paint_bytes==limit,"The exact retained storage boundary remains legal without allocating a giant fixture");
  near={};near.paint_bytes=limit-full.paint_bytes+1;
  check(refuses([&]{near.paint(p,true);},"PBR"),"One byte beyond the aggregate retained quota is rejected before decoded PBR allocations");
  native.active().painted=state.paint;native.active().material_bindings=state.material_bindings;auto s=with_origin(native.active());auto encoded=s.encode();
  auto bad=encoded;bad["polygon_sources"]["paints"][0]["layers"][0]["pbr_color_tiles"]=nlohmann::json::object();
  check(refuses([&]{Scene::decode(bad);},"Invalid polygon PBR color tile count"),"Actual Scene12 preflight rejects malformed retained PBR tiles before SurfaceMap decoding");
  bad=encoded;bad["polygon_sources"]["paints"][0]["baked"]["pbr_color"]=nlohmann::json::array();
  check(refuses([&]{Scene::decode(bad);},"Invalid retained baked PBR buffer"),"Actual Scene12 preflight rejects a malformed retained baked PBR plane before allocation");
  check(Scene::decode(encoded).encode()==encoded,"Proper nested prepared PBR layers and differing baked/common planes survive Scene12 restart");
}
void workflow(const std::filesystem::path &prefix) {
  auto s=native_fixture();const auto target=s.active().id,source=s.objects().back().id;const auto before=s.encode();
  Document d;d.replace(s);check(d.wait_mesh(),"Actual Scene12 PA target prepares its native renderer snapshot");
  auto unbound=s.snapshot();unbound.active().polygon.reset();
  check(refuses([&]{d.replace(unbound);},"own")&&d.copy_scene().encode()==before&&!d.frame().state->can_undo,"An unbound PA replacement cannot clear the real existing document or history");
  auto malformed=before;malformed["objects"][0]["polygon"]=nullptr;auto malformed_path=prefix;malformed_path+="-unbound.dsculpt";
  {auto data=nlohmann::json::to_cbor(malformed);std::ofstream f(malformed_path,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()));}
  auto malformed_open=d.open_async(malformed_path);const bool rejected_open=refuses([&]{malformed_open.get();},"Unreachable polygon source archive records");
  check(rejected_open&&d.wait_asset()&&d.copy_scene().encode()==before&&!d.frame().state->can_undo,"Malformed unbound Scene12 file admission leaves the real current scene and history exact");
  d.configure_object_grab(true,true);d.hold_object(target);
  auto held_state=d.frame().state;auto ghost=std::find_if(held_state->objects.begin(),held_state->objects.end(),[&](const auto &o){return o.id==source;});
  check(ghost!=held_state->objects.end()&&ghost->ghost&&!ghost->editable&&s.object(source).visible&&!s.object(source).locked,"The ordinary visible unlocked source becomes read-only solely through held-object ghost authority");
  ObjectBakeSettings settings;settings.ao_samples=0;settings.padding=4;settings.front_mm=settings.back_mm=80;
  d.bake_from_object(source,settings);check(d.wait_asset()&&!d.frame().state->error.empty()&&d.copy_scene().encode()==before&&!d.frame().state->can_undo,"Without explicit preparation, the real native bake refuses the prepared source and keeps exact PA/history");
  settings.prepare_colors=true;d.bake_from_object(source,settings);const bool done=d.wait_asset();std::cout<<"Native polygon color bake: "<<d.frame().state->error<<'\n';
  check(done&&d.frame().state->error.empty()&&d.wait_mesh(),"Actual held-target Bake proves strict UVs, prepares colors and creates its normal payload in one worker transaction");
  auto accepted=d.copy_scene();const auto &o=accepted.active();auto origin=o.polygon->origin;
  check(o.painted->surface_diagnostics().policy==PaintUvPolicy::Strict&&o.painted->color_projection()&&o.painted->baked()&&!o.painted->baked()->maps.normal.rgba.empty(),"The accepted derivative has actual Strict raster authority, independent resolved PBR and baked normal data");
  check(origin&&origin->polygon&&origin->paint->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored&&origin->paint->encode()==s.active().painted->encode()&&origin->polygon->topology.encode()==s.active().polygon->topology.encode(),"Exact original PA paint, topology and direct witness remain in the owning PolygonOrigin");
  check(o.material_bindings->color_preparation->original_paint->surface_diagnostics().policy==PaintUvPolicy::Strict&&accepted.object(source).painted->encode()==s.object(source).painted->encode(),"The color receipt owns the proven Strict pre-color state while the read-only source stays byte-exact");
  auto encoded=accepted.encode();check(d.undo()&&d.copy_scene().encode()==before&&d.redo()&&d.copy_scene().encode()==encoded,"Strict conversion, retained PA origin, preparation and bake have one exact native Undo/Redo transaction");
  d.begin_uv();check(d.wait_uv()&&d.frame().state->uv&&d.frame().state->uv->valid,"The prepared polygon enters the actual native UV workspace with its retained normal payload");
  d.edit_uv(UvAction::Scale,.8f);check(d.wait_uv()&&d.frame().state->uv->valid,"Native UV scaling validates the same persistent N-gon corner domain");d.end_uv(true);
  const bool uv_done=d.wait_asset();std::cout<<"Native prepared polygon UV: "<<d.frame().state->error<<'\n';
  check(uv_done&&d.frame().state->error.empty()&&d.wait_mesh()&&d.copy_scene().active().polygon->origin->paint->encode()==origin->paint->encode(),"Actual UV Apply transfers dual color and normal planes without changing the first direct PA source witness");
  auto persisted=d.copy_scene().encode();auto project=prefix;project+=".dsculpt";d.save(project);Document reopened;reopened.open(project);check(reopened.wait_asset()&&reopened.wait_mesh()&&reopened.copy_scene().encode()==persisted,"Actual file save/new Document reopening preserves the entire Scene12/color3/direct-PA authority graph");
  auto output=prefix;output+=".glb";reopened.export_asset(output).get();std::ifstream stream(output,std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)),{});ImportLimits limits;limits.mode=ImportMode::PreserveGltf;auto asset=import_glb(bytes,limits);
  check(!asset.primitives.empty()&&!asset.images.empty()&&std::filesystem::file_size(output)>1000,"Actual reopened Document exports the prepared polygon through real GLB encoding and independent parsing");
  bool nonflat=false;
  for(const auto &material:asset.materials)if(material.normal_texture>=0){auto im=decode_projection_png(asset.images.at(asset.textures.at(size_t(material.normal_texture)).image).bytes);for(size_t k=0;k<im.rgba.size();k+=4)nonflat|=std::abs(int(im.rgba[k])-128)>8||std::abs(int(im.rgba[k+1])-128)>8;}
  check(nonflat,"The independently parsed GLB actually binds a non-flat baked normal texture");
  reopened.configure_object_grab(true,true);reopened.hold_object(target);PaintSettings ink;ink.front_only=false;ink.color={.9f,.1f,.25f,1};reopened.paint_settings(ink);
  SpatialFrame f;f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[0].tracked.valid=f.hands[1].tracked.valid=true;f.hands[0].tracked.grip.position={0,1.4f,-.3f};f.hands[1].tracked.grip.position={2,2,2};
  auto send=[&]{++f.sequence;f.time+=.05;reopened.submit(f);if(!reopened.wait_input())throw std::runtime_error("Held color input timed out");};send();f.hands[0].tracked.squeeze=1;send();f.hands[0].tracked.grip.position.x+=.2f;send();
  auto held=reopened.frame().state;auto object=std::find_if(held->objects.begin(),held->objects.end(),[&](const auto &i){return i.id==target;});auto world=ob::compose(held->canvas,object->world_transform);f.hands[1].tracked.grip.position=ob::transform(world,{0,0,.5f});
  auto prior=reopened.copy_scene().active().painted->encode();f.hands[1].tracked.trigger=1;send();send();f.hands[1].tracked.trigger=0;send();
  check(reopened.frame().state->object_grabbing&&reopened.copy_scene().active().painted->encode()!=prior&&reopened.copy_scene().object(source).painted->encode()==s.object(source).painted->encode(),"Other-hand painting edits only the physically held prepared polygon while its ghost source remains exact");
  auto motion=reopened.copy_scene().encode();auto denied_motion=refuses([&]{reopened.bake_from_object(source,settings);},"Release object motion");
  check(denied_motion&&!reopened.frame().state->asset_busy&&reopened.copy_scene().encode()==motion,"World-distance bake remains gated while a held pose is only a GPU preview");
  f.hands[0].tracked.squeeze=0;send();check(!reopened.frame().state->object_grabbing&&reopened.copy_scene().active().transform.position.x>.19f,"Physical release commits the held transform without dropping accepted prepared paint");
  auto held_project=prefix;held_project+="-held.dsculpt";reopened.save(held_project);auto held_output=prefix;held_output+="-held.glb";reopened.export_asset(held_output).get();
  check(Scene::load(held_project).encode()==reopened.copy_scene().encode()&&std::filesystem::file_size(held_output)>1000,"Accepted held painting and pose save/reopen/export with the complete original PA origin still intact");
  auto overlap=native_fixture(true);auto overlap_before=overlap.encode();Document denied;denied.replace(overlap);denied.bake_from_object(overlap.objects().back().id,settings);
  check(denied.wait_asset()&&denied.frame().state->error.find("Unwrap polygon UVs")!=std::string::npos&&denied.copy_scene().encode()==overlap_before&&!denied.frame().state->can_undo,"Overlapping PA UVs fail actual strict preparation with guidance and exact source/history retention");
}
}
int main(int argc,char **argv)try {
#ifdef DOME_TEST_ONEAPI
  sycl::queue device{sycl::gpu_selector_v};
  std::cout<<"ONEAPI GPU "<<device.get_device().get_info<sycl::info::device::name>()<<'\n';
#endif
  if(argc==3&&std::string_view(argv[1])=="--ui-fixture"){native_fixture().save(argv[2]);return 0;}
  if(argc!=2)return 2;
  admission_and_origins();budgets();workflow(argv[1]);std::cout<<"PASS "<<checks<<" polygon/color authority checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
