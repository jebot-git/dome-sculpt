// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_normal_color_fixture
#include "normal-color-uv.cpp"
#undef main
#include "import-authoring-fixtures.hpp"
#include "polygon-conversion.hpp"
#include "polygon-scene-budget.hpp"
#include "scene-feature-version.hpp"
#include <fstream>
namespace hdr_scene {
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
void ink(SculptObject &o){o.painted=o.painted->snapshot();Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=5;b.strength=.73f;PaintSettings p;p.channel=PaintChannel::Emission;p.emission_units=PaintEmissionUnits::Physical;p.emission={12,.005f,10000};p.front_only=false;o.painted->begin_stroke();o.painted->stamp(b,p);check(o.painted->end_stroke()&&o.painted->has_physical_emission(),"Actual HDR stroke changes owned sparse data");}
Scene plain(){Scene s;s.active().mesh_active=true;s.active().painted=std::make_shared<PaintedMesh>(quad(),64);return s;}
Scene restart(const Scene &scene,const std::filesystem::path &path){const auto value=scene.encode();auto bytes=nlohmann::json::to_cbor(value);std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());out.close();if(!out)throw std::runtime_error("HDR scene write failed");auto read=Scene::decode(nlohmann::json::from_cbor(bytes));check(read.encode()==value,"Complete native scene restarts exactly through CBOR");return read;}
Scene retained_domain(){auto scene=prepared(false);const auto recipe=scene.active().bake_recipe;const auto &o=scene.active();UvWorkspace workspace(o.painted->mesh(),o.painted->resolution());auto p=front(o.painted->mesh());workspace.edit(UvAction::Pick,p[0],p[1]);workspace.edit(UvAction::Scale,.7f);workspace.edit(UvAction::Rotate,1.57079632679f);auto view=workspace.view();check(view.valid,"Genuine current-colour/normal chart edit validates");auto derivative=prepare_polygon_uv_derivative(o,*view.mesh,*view.source_vertices,{});
  SourceUvBakeInput in;in.paint=o.painted;in.current_material=o.material_bindings;in.current_deformation=o.deformation;in.polygon_authority=o.polygon;in.target_polygon=derivative.authority;in.target=derivative.mesh;in.source_vertices=derivative.source_vertices;
  SourceUvBakeOptions options;options.normal_sampling=SourceNormalUvMethod::RetainSource;auto result=adopt_source_uv_bake(bake_source_uv(in,options));auto &target=scene.active();target.painted=result.paint;target.material_bindings=result.material_bindings;target.deformation=result.deformation;target.appearance=result.appearance;target.polygon=derivative.authority;scene.validate();check(recipe&&target.bake_recipe==recipe&&scene.encode().at("version")==30&&target.painted->paint_sampling_domain(),"Actual factories preserve the genuine Bake recipe and paired writable domain in owning Scene30");return scene;
}
}
#ifndef DOME_HDR_SCENE_FIXTURE_ONLY
int main(int argc,char **argv)try {
  if(argc!=2)return 2;const std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
  auto ordinary=hdr_scene::plain();const auto legacy=ordinary.encode();check(legacy.at("version")<27,"Ordinary native material/paint does not select HDR schema");
  auto current=ordinary.snapshot();hdr_scene::ink(current.active());auto saved=current.encode();check(saved.at("version")==27&&saved.at("objects")[0].at("painted").at("version")==3,"Current physical paint selects actual Scene27/Paint3");hdr_scene::restart(current,dir/"current.dsculpt");
  for(int version:{5,24,25,26}){auto bad=saved;bad["version"]=version;check(refuses([&]{Scene::decode(bad);}),"Older outer scene cannot admit physical data");}
  auto phantom=saved;phantom["objects"][0]["painted"]=phantom["objects"][0]["painted"]["payload"];for(auto &layer:phantom["objects"][0]["painted"]["layers"])layer.erase("physical_emission_tiles");check(refuses([&]{Scene::decode(phantom);}),"A version marker cannot fabricate HDR authority");
  current.active().visible=false;current.active().painted->layer_properties("Hidden physical source",false,0);auto hidden=hdr_scene::restart(current,dir/"hidden.dsculpt");check(hidden.active().painted->has_physical_emission(),"Hidden zero-opacity HDR survives native persistence");
  auto source=hdr_scene::plain();hdr_scene::ink(source.active());auto capsule=capture_polygon_origin(source,source.active().id);auto retained=ordinary.snapshot();retained.active().rig_source=retained.active().shape_source=capsule;
  auto history=hdr_scene::restart(retained,dir/"retained.dsculpt");check(history.active().rig_source==history.active().shape_source&&history.active().rig_source->paint->has_physical_emission(),"Actual shared shape/rig source capsule retains HDR alias and all paint");
  check(retained.encode().at("polygon_sources").at("version")==6,"HDR uses the existing actual source capsule archive rather than inventing topology");
  auto origin=std::make_shared<SourceUvOrigin>();origin->paint=source.active().painted;
  auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->uv_origin=origin;binding->validate();
  auto before=ordinary.snapshot();before.active().material_bindings=binding;auto before_data=before.encode();
  check(before_data.at("version")==27&&before_data.at("source_materials").at("version")==11&&before_data.at("source_materials").at("base_version")==2,"Dormant UV before-paint owns HDR over its actual historical material2 layout");
  auto before_read=hdr_scene::restart(before,dir/"before-material.dsculpt");check(before_read.active().material_bindings->uv_origin->paint->has_physical_emission(),"Before-material physical paint is not discarded on restart");
  auto bad=before_data;bad["source_materials"]["base_version"]=10;check(refuses([&]{Scene::decode(bad);}),"Material11 does not implicitly authorize unknown key/frame capability10");
  bad=before_data;bad["source_materials"]["base_version"]=1;check(refuses([&]{Scene::decode(bad);}),"Declared base cannot discard actual retained frame fields");
  bad=before_data;bad["source_materials"]["paints"].push_back(bad["source_materials"]["paints"][0]);check(refuses([&]{Scene::decode(bad);}),"Unused physical paint cannot manufacture or lose authority in an archive");
  bad=before_data;bad["objects"][0]["material_bindings"]=nullptr;check(refuses([&]{Scene::decode(bad);}),"Unreachable HDR binding roots cannot disappear during scene loading");
  detail::PolygonSceneBudget budget;budget.owning_physical_emission=true;budget.paint(source.active().painted->encode(),false,27);check(budget.paint_bytes>=source.active().painted->storage_bytes(),"Aggregate preflight charges actual seven-float tiles and common planes");
  detail::PolygonSceneBudget tight;tight.owning_physical_emission=true;tight.paint_bytes=256*1024*1024-budget.paint_bytes+1;check(refuses([&]{tight.paint(source.active().painted->encode(),false,27);}),"Physical data obeys unchanged aggregate retained-byte cap before reconstruction");
  import_fixture::Fixture input;while(input.bin.size()%4)input.bin.push_back(0);const auto uv=input.values({0,0,0,1,1,1,1,0},2,"VEC2");input.j["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_2"]=uv;auto path=dir/"raw-uv2.glb";input.save(path);ImportAuthoringOptions options;options.paint_resolution=64;
  auto raw=prepare_authoring_import(Scene{},path,options).scene;uint64_t id=0;for(const auto &o:raw.objects())if(o.appearance){id=o.id;break;}check(id!=0,"Actual importer captures immutable original source and raw extra UV2");raw.select(id);const auto original=raw.active().appearance->original_bytes;hdr_scene::ink(raw.active());auto raw_read=hdr_scene::restart(raw,dir/"raw-uv2-hdr.dsculpt");check(*raw_read.active().appearance->original_bytes==*original&&!raw_read.active().appearance->asset->primitives[0].extra_uv_sets.empty(),"HDR preserves original encoded source and unused authenticated UV2");
  auto domain=hdr_scene::retained_domain();const auto old_domain=domain.active().painted->paint_sampling_domain();const auto old_normal=domain.active().painted->normal_projection();const auto old_baked=domain.active().painted->baked();hdr_scene::ink(domain.active());check(domain.active().painted->paint_sampling_domain()==old_domain&&domain.active().painted->normal_projection()==old_normal&&domain.active().painted->baked()==old_baked,"HDR edits retain actual independent paint/normal/bake owners");
  auto domain_read=hdr_scene::restart(domain,dir/"genuine-uv24-hdr.dsculpt");check(domain_read.active().painted->paint_sampling_domain()&&domain_read.active().painted->normal_projection()&&domain_read.active().painted->has_physical_emission(),"Actual Scene24 factory result composes with physical data through native restart");
  std::cout<<"HDR owning scene: "<<checks<<" checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}

#endif
