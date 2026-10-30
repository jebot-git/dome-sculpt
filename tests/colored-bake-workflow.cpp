// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/color-preparation.hpp"
#include "dome/tangents.hpp"
#include "source-color-fixtures.hpp"
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *text){if(!ok)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
Image solid(uint32_t r,std::array<uint8_t,4> pixel){Image i{r,r,std::vector<uint8_t>(size_t(r)*r*4)};for(size_t p=0;p<i.rgba.size();++p)i.rgba[p]=pixel[p%4];return i;}
Scene fixture(bool directional=false){
  auto low=source_color_fixture::model(true,directional);auto high=source_color_fixture::model(false,directional);
  auto target=low.active().id,source=low.add("Hidden read-only source",Volume{});
  auto object=high.active();object.id=source;object.name="Hidden read-only source";object.visible=false;object.locked=true;object.transform.position.z=.005f;
  low.object(source)=std::move(object);low.select(target);low.validate();return low;
}
Scene curved_fixture(){
  auto scene=source_color_fixture::model(false,false,true);auto &o=scene.active();auto m=o.painted->mesh();
  auto target=o.id;auto high=scene.add("Curved fixture source",Volume{});auto mesh=m;mesh.positions={{-1,-1,.3f},{1,-1,.3f},{1,1,.3f},{-1,1,.3f}};mesh.normals.assign(4,{0,0,1});auto &s=scene.object(high);s.mesh_active=true;s.visible=false;s.locked=true;s.painted=std::make_shared<PaintedMesh>(mesh,64);scene.select(target);scene.validate();return scene;
}
}
int main(int argc,char **argv)try{
  if(argc==3&&std::string(argv[1])=="--inspect"){
    auto scene=Scene::load(argv[2]);auto encoded=scene.encode();auto independent=nlohmann::json::array();
    for(const auto &object:scene.objects()){auto isolated=scene;for(const auto &other:scene.objects())if(other.id!=object.id)isolated.erase(other.id);isolated.select(object.id);independent.push_back(isolated.encode());}
    encoded["isolated_objects"]=std::move(independent);std::cout<<encoded.dump()<<'\n';return 0;
  }
  if(argc==3&&std::string(argv[1])=="--ui-fixture"){fixture().save(argv[2]);return 0;}
  if(argc<2)return 2;std::filesystem::path prefix=argv[1];auto scene=fixture();auto before=scene.encode();auto source=scene.objects().back().id;
  Document document;document.replace(scene);check(document.wait_mesh(),"Real source-colored target publishes before native bake admission");
  ObjectBakeSettings settings;settings.ao_samples=0;settings.padding=4;
  document.bake_from_object(source,settings);check(document.wait_asset()&&!document.frame().state->error.empty()&&document.copy_scene().encode()==before&&!document.frame().state->can_undo,"Native colored bake requires explicit preparation and rejects without changing target, sources or history");
  settings.prepare_colors=true;document.bake_from_object(source,settings);
  check(document.wait_asset()&&document.frame().state->error.empty()&&document.wait_mesh(),"Actual Document worker prepares target colors and bakes all channels in one accepted operation");
  auto baked=document.copy_scene();auto first=baked.active().material_bindings->color_preparation;
  check(baked.active().painted->baked()&&!baked.active().painted->baked()->maps.normal.rgba.empty()&&baked.active().material_bindings->normal_frames->canonical&&baked.active().material_bindings->normal_frames->pbr==scene.active().material_bindings->normal_frames->pbr,"The real normal payload captures its computed compatible Mikk basis while preserving original PBR direction authority");
  check(baked.object(source).painted->encode()==scene.object(source).painted->encode()&&baked.object(source).appearance==scene.object(source).appearance&&baked.object(source).material_bindings==scene.object(source).material_bindings&&baked.object(source).deformation==scene.object(source).deformation&&baked.object(source).locked&&!baked.object(source).visible&&baked.active().painted->baked()->color_interpretation==BakedColorInterpretation::ResolvedPbr,"Read-only hidden source stays exact and the accepted base declares its resolved PBR interpretation");
  auto baked_json=baked.encode();check(document.undo()&&document.copy_scene().encode()==before&&document.redo()&&document.copy_scene().encode()==baked_json,"Preparation and actual normal/color bake share one exact global Undo/Redo transaction");
  document.begin_uv();check(document.wait_uv()&&document.frame().state->uv->valid,"Native UV workspace initializes from the actual baked source target");
  document.edit_uv(UvAction::Scale,.5f);document.edit_uv(UvAction::Rotate,1.5707963267948966f);
  check(document.wait_uv()&&document.frame().state->uv->valid,"Actual Scale and Rotate commands validate the edited sampling atlas");
  document.end_uv(true);bool uv_finished=document.wait_asset();std::cout<<"Native UV result: "<<document.frame().state->error<<" | "<<document.frame().state->message<<'\n';check(uv_finished&&document.frame().state->error.empty()&&document.wait_mesh(),"Native Apply transfers PBR/common layers and real normal detail using the retained captured frame");
  auto uv=document.copy_scene();check(uv.active().material_bindings->color_preparation->initial_projection==first->initial_projection&&uv.active().material_bindings->normal_frames->canonical==baked.active().material_bindings->normal_frames->canonical&&!uv.active().painted->baked()->maps.normal.rgba.empty(),"UV publication preserves the initial conversion receipt and exact captured normal-frame identity");
  auto project=prefix;project+=".dsculpt";document.save(project);Document restarted;restarted.open(project);check(restarted.wait_asset()&&restarted.wait_mesh()&&restarted.copy_scene().encode()==uv.encode(),"Actual file save and a new Document restart retain the complete normal/color/UV source state");
  auto output=prefix;output+=".glb";restarted.export_asset(output).get();check(std::filesystem::file_size(output)>1000,"Actual Document exports the real baked normal target under the existing canonical representability guards");
  settings.prepare_colors=false;restarted.bake_from_object(source,settings);check(restarted.wait_asset()&&restarted.frame().state->error.empty()&&restarted.wait_mesh(),"Repeat native bake reuses the retained basis after UV rotation instead of encoding against the new atlas tangent direction");
  auto repeated=restarted.copy_scene();check(repeated.active().material_bindings->color_preparation->receipt.encode()==first->receipt.encode()&&repeated.active().material_bindings->normal_frames->canonical&&!repeated.active().painted->baked()->maps.normal.rgba.empty(),"Repeat bake keeps immutable first conversion quality, current color policy and real normal detail");
  auto repeat_project=prefix;repeat_project+="-repeat.dsculpt";restarted.save(repeat_project);auto repeat_output=prefix;repeat_output+="-repeat.glb";restarted.export_asset(repeat_output).get();check(std::filesystem::file_size(repeat_output)>1000,"Repeated real normal/color bake still exports without relaxing original-frame matching");
  auto authored=fixture(true);Document negative;negative.replace(authored);settings.prepare_colors=true;negative.bake_from_object(source,settings);check(negative.wait_asset()&&negative.frame().state->error.empty()&&!negative.copy_scene().active().material_bindings->normal_frames->canonical,"Artist normal/tangent direction targets do not acquire fabricated portable canonical authority");
  auto artist_before=negative.copy_scene().encode();auto denied=prefix;denied+="-artist-denied.glb";check(rejects([&]{negative.export_asset(denied).get();})&&negative.copy_scene().encode()==artist_before,"Unrepresentable artist-direction normal export refuses while retaining the complete native source state");
  auto historical=fixture();BakedTextures prior;prior.maps.color=solid(64,{255,255,255,255});prior.maps.metal_rough=solid(64,{255,128,0,255});prior.maps.emission=solid(64,{0,0,0,255});prior.maps.normal=solid(64,{128,128,255,255});prior.coverage.assign(64*64,255);historical.active().painted=historical.active().painted->snapshot();historical.active().painted->install_bake(prior);Document legacy;legacy.replace(historical);legacy.bake_from_object(source,settings);
  check(legacy.wait_asset()&&legacy.frame().state->error.empty()&&!legacy.copy_scene().active().material_bindings->normal_frames->canonical&&legacy.copy_scene().active().material_bindings->color_preparation->original_paint->baked()->maps.normal.rgba==prior.maps.normal.rgba,"Historical normal payloads retain their original interpretation and exact captured data instead of acquiring retroactive canonical authority");
  auto mixed=fixture();auto weights=*mixed.active().deformation;weights.skeleton->bones.push_back({"Second",0,{},.1f,true});weights.pose.push_back({});weights.skin->vertices.assign(4,{{0,.5f},{1,.5f}});mixed.set_deformation(mixed.active().id,weights);Document blended;blended.replace(mixed);blended.bake_from_object(source,settings);
  check(blended.wait_asset()&&blended.frame().state->error.empty()&&!blended.copy_scene().active().material_bindings->normal_frames->canonical,"Multi-joint skin never passes the conservative single-rigid-joint canonical capture proof");
  auto curved=curved_fixture();Document curvature;curvature.replace(curved);auto curve_options=settings;curve_options.front_mm=500;curvature.bake_from_object(curved.objects().back().id,curve_options);
  check(curvature.wait_asset()&&curvature.wait_mesh()&&curvature.frame().state->error.empty()&&!curvature.copy_scene().active().material_bindings->normal_frames->canonical&&curvature.copy_scene().active().material_bindings->normal_frames->pbr==curved.active().material_bindings->normal_frames->pbr,"Actual smoothly curved normal directions remain untouched rather than being flattened to manufacture portability");
  auto curve_before=curvature.copy_scene().encode();curvature.begin_uv();check(curvature.wait_uv(),"Unsupported curved UV case reaches the actual native workspace");curvature.edit_uv(UvAction::Scale,.5f);check(curvature.wait_uv()&&curvature.frame().state->uv->valid,"Curved draft UVs validate independently before frame-policy admission");curvature.end_uv(true);
  check(curvature.wait_asset()&&!curvature.frame().state->error.empty()&&curvature.copy_scene().encode()==curve_before,"Native Apply refuses unproven curved baked-frame transfer without dropping color, normal, source data or the editable draft");curvature.end_uv(false);
  std::cout<<"Colored native workflow checks "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
