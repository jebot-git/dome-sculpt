// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-maps.hpp"
#include <chrono>
#include <iostream>
#include <numbers>
#include <unistd.h>
#include <nlohmann/json.hpp>
using namespace dome;
void require(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
Mesh rectangle() {
  Mesh mesh;mesh.positions={{0,0,0},{2,0,0},{0,1,0},{2,1,0}};
  mesh.normals.assign(4,{0,0,1});mesh.indices={0,1,2,1,3,2};
  mesh.uv={{.1,.1},{.8,.1},{.1,.8},{.6,.9}};mesh.uv_pins={1,1,0,0};return mesh;
}
Scene fixture(bool source_maps=false, uint32_t resolution=128) {
  auto mesh=rectangle();Scene scene;
  auto &object=scene.active();object.mesh_active=true;object.name="UV workflow fixture";
  if(source_maps){
    auto image=std::make_shared<Image>();image->width=image->height=2;
    image->rgba={255,0,0,255,0,255,0,255,0,0,255,255,255,255,0,255};
    auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image;
    auto normal=std::make_shared<Image>();normal->width=normal->height=2;
    normal->rgba={204,128,230,255,128,204,230,255,80,128,230,255,128,80,230,255};
    maps->normal.image=normal;maps->normal_scale=.6f;
    maps->normal.uv_transform={0,-1,1,1,0,0};
    auto ao=std::make_shared<Image>();ao->width=ao->height=2;ao->rgba.assign(16,120);
    maps->occlusion.image=ao;maps->occlusion_strength=.65f;
    object.painted=PaintedMesh::from_maps(mesh,maps,resolution);
  }else{
    object.painted=std::make_shared<PaintedMesh>(mesh,resolution);
    PaintSettings settings;settings.color={.02f,.04f,.9f,.7f};
    Brush brush;brush.from=brush.to={1,.5f,0};brush.radius=3;brush.normal={0,0,1};
    object.painted->begin_stroke();object.painted->stamp(brush,settings);object.painted->end_stroke();
    object.painted->add_layer("Preserved detail");
  }
  DeformationState rig(mesh);
  rig.skeleton=Skeleton{{{"root",-1,{},1,true},{"limb",0,{{0,1,0},{},{1,1,1}},1,true}}};
  rig.skin=SkinWeights{{{{0,1}},{{1,1}},{{0,.4f},{1,.6f}},{{0,.7f},{1,.3f}}}};
  rig.morphs=MorphSet(mesh);auto changed=mesh.positions;changed[2].z=.2f;
  rig.morphs->set_target(mesh,"Example shape",changed);rig.morph_weights={.4f};
  rig.pose={rig.skeleton->bones[0].rest,rig.skeleton->bones[1].rest};
  rig.pose[1].rotation={std::cos(.1f),0,0,std::sin(.1f)};
  scene.set_deformation(object.id,std::move(rig));scene.validate();return scene;
}
Scene baked_fixture() {
  auto result=fixture();auto &object=result.active();
  auto mesh=object.painted->mesh();mesh.uv={{.1,.1},{.8,.1},{.1,.45},{.8,.45}};
  mesh.uv_pins.assign(mesh.positions.size(),0);
  object.painted=object.painted->reparameterize(mesh);
  const uint32_t resolution=object.painted->resolution();
  auto image=[&](std::array<uint8_t,4> color){
    Image result{resolution,resolution,{}};result.rgba.resize(size_t(resolution)*resolution*4);
    for(size_t i=0;i<result.rgba.size();i+=4)std::copy(color.begin(),color.end(),result.rgba.begin()+i);
    return result;
  };
  BakedTextures baked;
  baked.maps.color=image({85,163,44,220});
  baked.maps.metal_rough=image({170,90,30,255});
  baked.maps.emission=image({30,10,5,255});
  baked.maps.normal=image({205,153,226,255});
  baked.coverage.assign(size_t(resolution)*resolution,255);
  object.painted->install_bake(std::move(baked));result.validate();return result;
}
struct Driver {
  Document &doc;SpatialFrame frame;
  Driver(Document &document):doc(document){frame.sequence=1;frame.time=1;frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};frame.hands[1].tracked.valid=true;frame.hands[1].tracked.grip.position={0,1.48f,-.5f};send();}
  void send(float x=0,float y=0,bool click=false,float pressure=0){++frame.sequence;frame.time+=.05;auto&h=frame.hands[1];h.x=x;h.y=y;h.joystick=click;h.tracked.trigger=pressure;doc.submit(frame);if(!doc.wait_input())throw std::runtime_error("Controller worker timed out");}
  void open(){send();send(0,0,true);send();}
  void sector(int i){float a=i*std::numbers::pi_v<float>/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
  void point(float x,float y,bool press=true){auto state=doc.frame().state;auto&h=frame.hands[1];h.tracked.aimValid=true;h.tracked.aim.position=ob::transform(state->panel,{(x/640-.5f)*.6f,(.5f-y/640)*.6f,.25f});h.tracked.aim.orientation=state->panel.orientation;send(0,0,false,press?.8f:0);send();}
};
int main(int argc,char**argv)try{
  if(argc==3&&std::string(argv[1])=="--ui-fixture"){fixture().save(argv[2]);return 0;}
  auto scene=fixture();auto original=scene.encode();Document document;document.replace(scene);
  document.begin_uv();require(document.wait_uv(),"Rigged UV draft validates asynchronously");
  auto start=std::chrono::steady_clock::now();document.edit_uv(UvAction::Conformal);
  require(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(250),"Conformal scheduling returns without running solver on model/input thread");
  require(document.wait_uv()&&document.frame().state->uv->valid,"Background conformal result validates before Apply");
  auto solved=document.frame().state->uv;
  require(solved->mesh->uv[0]==rectangle().uv[0]&&solved->mesh->uv[1]==rectangle().uv[1]&&std::abs(solved->mesh->uv[2][1]-.45f)<1e-6f,"Native draft uses analytic conformal shape with exact pins");
  require(document.copy_scene().encode()==original,"Background draft solve leaves committed scene/paint/deformation exact");
  document.end_uv(true);require(document.wait_asset()&&document.frame().state->error.empty(),"Apply publishes paint and remapped rig through ordered worker");
  auto committed=document.copy_scene();committed.validate();
  require(committed.active().painted->layers().size()==2&&committed.active().deformation->morphs->targets.size()==1&&committed.active().deformation->pose.size()==2,"UV publication retains editable paint layers, morph identity and current pose");
  auto updated=committed.encode();require(document.undo()&&document.copy_scene().encode()==original&&!document.frame().state->dirty,"One global undo restores exact complete pre-UV scene and clean state");
  require(document.redo()&&document.copy_scene().encode()==updated,"One global redo restores accepted UV/paint/rig state exactly");document.undo();
  document.begin_uv();require(document.wait_uv(),"Second UV session validates");document.edit_uv(UvAction::Pick,.45f,.45f);document.edit_uv(UvAction::Seam);require(document.wait_uv(),"Seam fan split runs on detached UV worker");
  auto cut=document.frame().state->uv;require(cut->mesh->positions.size()==6&&cut->source_vertices&&cut->source_vertices->size()==6,"Draft retains complete source map for all six seam-split vertices");
  document.edit_uv(UvAction::Pick,.45f,.45f);document.edit_uv(UvAction::Stitch);
  require(document.wait_uv()&&document.frame().state->uv->valid&&document.frame().state->uv->topology->islands.size()==1,
          "Selected explicit seam stitches and conformally reflows on the draft worker");
  auto stitched=document.frame().state->uv;
  document.edit_uv(UvAction::Undo);
  require(document.frame().state->uv->mesh->uv_seams==cut->mesh->uv_seams&&
          *document.frame().state->uv->source_vertices==*cut->source_vertices,
          "Draft Undo restores seam and its exact original-vertex remap together");
  document.edit_uv(UvAction::Redo);
  require(document.frame().state->uv->mesh->uv==stitched->mesh->uv,
          "Draft Redo restores accepted conformal stitching exactly");
  document.edit_uv(UvAction::Undo);
  document.end_uv(true);require(document.wait_asset()&&document.frame().state->error.empty(),"Seam topology publication safely remaps skin/morph data");
  auto split=document.copy_scene();auto&object=split.active();object.deformation->validate(object.painted->mesh());
  require(object.deformation->skin->vertices.size()==6&&object.deformation->morphs->targets[0].delta.size()==6,"Every duplicated source vertex receives exact skin and morph rows");
  auto posed_before=scene.active().deformation->preview(scene.active().painted->mesh());auto posed_after=object.deformation->preview(object.painted->mesh());bool same=true;
  for(size_t corner=0;corner<posed_before.indices.size();++corner)same&=length(posed_before.positions[posed_before.indices[corner]]-posed_after.positions[posed_after.indices[corner]])<1e-6f;
  require(same,"Independent skinned/morphed surface remains identical after UV seam remap");
  auto native=std::filesystem::temp_directory_path()/("dome-uv-workflow-native-"+std::to_string(getpid())+".dsculpt");split.save(native);require(Scene::load(native).encode()==split.encode(),"Remapped UV/skin/morph state survives exact native restart");std::filesystem::remove(native);
  require(document.undo()&&document.copy_scene().encode()==original,"Seam worker edit remains one complete exact global undo");
  for(int attempt=0;attempt<5;++attempt){document.begin_uv();require(document.wait_uv(),"UV initialization completes before solver cancellation fixture");document.edit_uv(UvAction::Conformal);document.end_uv(false);require(document.wait_uv(),"Cancellation closes asynchronous draft without stale resurrection");}
  require(document.copy_scene().encode()==original&&!document.frame().state->uv,"Cancelled solve never changes scene or restores a closed session");
  document.begin_uv();require(document.wait_uv(),"UV initialization completes before solver cancellation fixture");document.edit_uv(UvAction::Conformal);document.end_uv(false);document.begin_uv();require(document.wait_uv(),"New UV session rejects earlier solver result");require(document.frame().state->uv->mesh->uv==rectangle().uv,"Prior session cannot overwrite new draft UV identity");document.end_uv(false);
  Document accepted_cancel;auto detailed=fixture(false,512);auto detailed_original=detailed.encode();accepted_cancel.replace(detailed);
  accepted_cancel.begin_uv();require(accepted_cancel.wait_uv(),"Detailed cancellation draft validates");
  accepted_cancel.edit_uv(UvAction::Conformal);require(accepted_cancel.wait_uv(),"Detailed cancellation solve completes before Apply");
  accepted_cancel.end_uv(true);accepted_cancel.end_uv(false);
  require(accepted_cancel.wait_asset()&&accepted_cancel.copy_scene().encode()==detailed_original,
          "Explicit Cancel discards an accepted in-flight paint transfer before publication");
  accepted_cancel.edit_scene("New authoring after UV cancellation",[](Scene &next,const auto &cancelled){if(cancelled())throw std::runtime_error("cancelled");next.active().material.roughness=.31f;});
  require(accepted_cancel.wait_asset()&&accepted_cancel.copy_scene().active().material.roughness==.31f,
          "Cancelled old UV job cannot clear or overwrite newer guarded scene work");
  auto baked_scene=baked_fixture();auto baked_original=baked_scene.encode();
  require(baked_original.at("version")==6,"Rigged baked UV fixture uses source-aware native Scene v6");
  Document baked_document;baked_document.replace(baked_scene);baked_document.begin_uv();require(baked_document.wait_uv(),"Baked rigged UV draft validates");
  baked_document.edit_uv(UvAction::Scale,.7f);baked_document.edit_uv(UvAction::Rotate,std::numbers::pi_v<float>/2);
  require(baked_document.wait_uv()&&baked_document.frame().state->uv->valid,"Baked UV target rotates inside atlas without changing geometry");
  baked_document.end_uv(true);require(baked_document.wait_asset()&&baked_document.frame().state->error.empty(),"Baked color/ORM/emission/normal base transfers through guarded UV worker");
  auto baked_updated=baked_document.copy_scene();const auto &baked_object=baked_updated.active();
  auto bake=baked_object.painted->baked();require(bake&&!bake->maps.normal.rgba.empty()&&baked_object.deformation&&baked_object.painted->layers().size()==2,
      "UV transfer retains canonical baked normal, editable layers and complete native rig");
  const uint32_t resolution=baked_object.painted->resolution();size_t center=(size_t(.275f*resolution)*resolution+size_t(.45f*resolution))*4;
  Vec3 old_normal=normalized({205/127.5f-1,153/127.5f-1,226/127.5f-1});
  Vec3 tangent_normal={bake->maps.normal.rgba[center]/127.5f-1,bake->maps.normal.rgba[center+1]/127.5f-1,bake->maps.normal.rgba[center+2]/127.5f-1};
  // Independent analytic frame after UV90: tangent=-Y, bitangent=+X.
  Vec3 world_normal=normalized({tangent_normal.y,-tangent_normal.x,tangent_normal.z});
  require(dot(old_normal,world_normal)>.999f&&bake->maps.normal.rgba[center]<130&&bake->maps.normal.rgba[center+1]>180,
      "Rotated UV bake transports normal directions into new Mikk basis rather than copying stale RGB vectors");
  require(bake->coverage[center/4]>250&&bake->maps.metal_rough.rgba[center]==170,
      "UV transfer preserves baked coverage and AO/ORM channel values independently of paint overlays");
  auto baked_encoded=baked_updated.encode();require(Scene::decode(baked_encoded).encode()==baked_encoded,
      "Scene v6 restart retains UV-edited baked normal/coverage plus skin/morph identities exactly");
  require(baked_document.undo()&&baked_document.copy_scene().encode()==baked_original&&baked_document.redo()&&baked_document.copy_scene().encode()==baked_encoded,
      "Baked UV normal-frame transfer is one complete exact global undo/redo transaction");
  PaintedMesh bake_only(baked_scene.active().painted->mesh(),resolution);
  bake_only.install_bake(*baked_scene.active().painted->baked());
  auto baked_without_paint=bake_only.reparameterize(baked_object.painted->mesh());
  baked_without_paint->clear_bake();
  require(!baked_without_paint->has_paint_data()&&baked_without_paint->storage_bytes()==0,
      "Baked UV transfer does not miscount image storage as editable tiles after Clear bake");
  auto legacy=baked_original;legacy["version"]=5;
  for(auto key:{"groups","next_group_id","source_images","source_models"})legacy.erase(key);
  for(auto &object:legacy["objects"]){object.erase("group");object.erase("appearance");for(auto key:{"alpha","alpha_cutoff","unlit"})object["material"].erase(key);}
  auto legacy_loaded=Scene::decode(legacy);legacy_loaded.validate();
  require(legacy_loaded.active().painted->baked()&&legacy_loaded.active().deformation&&legacy_loaded.encode().at("version")==6,
      "Legacy baked-v5 native source upgrades while retaining rig and canonical normal maps");
  Document sourced;auto source=fixture(true);
  source.active().painted->install_bake(*baked_scene.active().painted->baked());
  auto preserved=source.encode();
  require(preserved.at("version")==6&&source.active().painted->source_maps()->normal.image&&source.active().painted->baked(),
      "Source guard fixture retains independent raw normal/AO bindings and canonical baked normal in Scene v6");
  sourced.replace(source);sourced.begin_uv();require(sourced.wait_uv(),"Source image draft validates without modifying source payload");sourced.edit_uv(UvAction::Conformal);require(sourced.wait_uv(),"Source UV solve may preview a detached draft");sourced.end_uv(true);require(sourced.wait_asset(),"Source transfer refusal completes worker state");
  require(sourced.frame().state->error.find("all-map bake")!=std::string::npos&&sourced.copy_scene().encode()==preserved&&sourced.frame().state->uv,"Changed source UV publication explicitly refuses before texture/tangent/provenance loss");sourced.end_uv(false);
  Document xr;xr.replace(scene);Driver hand(xr);hand.open();hand.sector(9);hand.sector(7);require(xr.frame().state->uv&&xr.frame().state->controls.parameter==VrParameter::UvEdit,"Real joystick navigation opens native rigged UV workspace");require(xr.wait_uv(),"Controller UV draft finishes validation");
  hand.point(55,165);require(xr.wait_uv(),"Controller side Unwrap button queues detached conformal worker");require(std::abs(xr.frame().state->uv->mesh->uv[2][1]-.45f)<1e-6f,"Controller UV Unwrap dispatches the same exact conformal action");hand.point(299,225.5f);hand.point(460,559);require(xr.wait_uv()&&xr.frame().state->uv->topology->islands.size()==2,
          "Controller Seam button runs the same remapped cut operation");
  hand.point(299,225.5f);hand.point(55,218);require(xr.wait_uv()&&xr.frame().state->uv->topology->islands.size()==1,
          "Controller side Stitch button joins the explicitly selected seam");
  hand.point(170,609);require(!xr.frame().state->uv&&xr.copy_scene().encode()==original,"Controller Cancel discards processed UV draft and keeps exact native scene");
  return 0;
}catch(const std::exception&error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
