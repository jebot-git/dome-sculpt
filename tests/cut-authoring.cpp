// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-maps.hpp"
#include "dome/import.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception &){return true;}}
Mesh cube(){
  Mesh m;m.positions={{-.2f,-.2f,-.2f},{.2f,-.2f,-.2f},{.2f,.2f,-.2f},{-.2f,.2f,-.2f},
                      {-.2f,-.2f,.2f},{.2f,-.2f,.2f},{.2f,.2f,.2f},{-.2f,.2f,.2f}};
  for(auto p:m.positions)m.normals.push_back(normalized(p));
  m.indices={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
  unwrap(m,128);return m;
}
Scene fixture(){
  Scene scene;scene.active().name="Painted source";
  auto maps=std::make_shared<PaintSourceMaps>();auto image=std::make_shared<Image>();
  image->width=image->height=8;image->rgba.resize(8*8*4);
  for(int y=0;y<8;++y)for(int x=0;x<8;++x){auto at=(y*8+x)*4;image->rgba[at]=uint8_t(24+x*26);image->rgba[at+1]=uint8_t(20+y*28);image->rgba[at+2]=70;image->rgba[at+3]=255;}
  maps->color.image=image;maps->color.wrap_s=maps->color.wrap_t=33071;
  scene.active().painted=PaintedMesh::from_maps(cube(),maps,128);scene.active().mesh_active=true;
  scene.active().material.color={1,1,1,1};scene.active().material.roughness=.4f;
  scene.active().material.metallic=.25f;scene.active().material.emissive={.02f,.04f,.08f};return scene;
}
BakeSource high(const Scene &scene){BakeSource source;auto o=scene.active();source.mesh=o.painted->mesh();
  source.materials[0].material=o.material;source.materials[0].maps=o.painted->preview(o.material);return source;}
Mesh combined(const Scene &scene){Mesh mesh;for(auto &s:scene.surfaces()){
  auto transform=s.world_transform();auto offset=uint32_t(mesh.positions.size());
  for(auto p:s.mesh.positions){auto v=ob::transform(transform,{p.x,p.y,p.z});mesh.positions.push_back({v.x,v.y,v.z});}
  for(auto n:s.mesh.normals){auto v=ob::rotate(transform.orientation,{n.x,n.y,n.z});mesh.normals.push_back({v.x,v.y,v.z});}
  for(auto v:s.mesh.indices)mesh.indices.push_back(offset+v);
}return mesh;}
struct Controller {
  Document &doc;SpatialFrame frame;int side=1;
  explicit Controller(Document &d,int index=1):doc(d),side(index){frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};frame.hands[side].tracked.valid=true;send();}
  void send(float x=0,float y=0,bool click=false){++frame.sequence;frame.time+=.02;
    frame.hands[side].x=x;frame.hands[side].y=y;frame.hands[side].joystick=click;
    doc.submit(frame);if(!doc.wait_input())throw std::runtime_error("Controller input timed out");}
  void sector(int index){float a=index*6.283185307f/12;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
};
}
int main(int argc,char **argv)try{
  if(argc==3&&std::string(argv[1])=="--ui-fixture"){fixture().save(argv[2]);return 0;}
  if(argc==3&&std::string(argv[1])=="--inspect"){
    auto scene=Scene::load(argv[2]);auto mesh=combined(scene);auto minimum=mesh.positions.front().x,maximum=minimum;
    for(auto point:mesh.positions){minimum=std::min(minimum,point.x);maximum=std::max(maximum,point.x);}
    std::cout<<nlohmann::json{{"scene",scene.encode()},{"x_min",minimum},{"x_max",maximum},
      {"closed",inspect_mesh(mesh).closed_manifold()}}.dump()<<'\n';return 0;
  }
  check(spatial_parameter(14,0)==VrParameter::SkinSource&&spatial_parameter(16,9)==VrParameter::RetopoDistance&&
        spatial_parameter(17,1)==VrParameter::CutOffset&&!spatial_action(15,0)&&!spatial_action(18,0),
        "Skin, reserved masks, retopology and plane-cut pages retain independent native command routing");
  // Two nearby surfaces distinguish exact provenance from a nearest ray.
  Mesh low;low.positions={{0,0,0},{1,0,0},{0,1,0}};low.normals.assign(3,{0,0,1});low.uv={{0,0},{1,0},{0,1}};low.indices={0,1,2};
  BakeSource layers;layers.mesh=low;for(auto p:low.positions){p.z+=.001f;layers.mesh.positions.push_back(p);}
  layers.mesh.normals.insert(layers.mesh.normals.end(),3,{0,0,1});layers.mesh.uv.insert(layers.mesh.uv.end(),low.uv.begin(),low.uv.end());
  layers.mesh.indices.insert(layers.mesh.indices.end(),{3,4,5});layers.materials.resize(2);
  layers.materials[0].material.color={1,0,0,1};layers.materials[1].material.color={0,0,1,1};layers.triangle_materials={0,1};
  BakeOptions options;options.resolution=16;options.padding=options.ao_samples=0;
  auto rays=bake_mesh(low,layers,options);options.source_triangles={0};options.source_corner_barycentrics={{1,0,0},{0,1,0},{0,0,1}};
  auto exact=bake_mesh(low,layers,options);bool colors=true;
  for(size_t n=0;n<exact.coverage.size();++n)if(exact.coverage[n]==1)
    colors&=exact.maps.color.rgba[n*4]==255&&exact.maps.color.rgba[n*4+2]==0&&rays.maps.color.rgba[n*4+2]==255;
  check(colors&&exact.rays==0&&exact.projected==rays.projected,"Exact source provenance preserves the intended red surface instead of a nearby blue projection hit");
  auto invalid=options;invalid.source_corner_barycentrics[0]={.5f,.5f,.5f};
  check(rejects([&]{bake_mesh(low,layers,invalid);}),"Mapped baking rejects unnormalized source barycentrics");
  invalid=options;invalid.source_triangles[0]=1;
  check(rejects([&]{bake_mesh(low,layers,invalid);}),"Mapped baking rejects a geometrically mismatched source triangle");
  auto source=fixture();auto before=source.encode();auto baking=high(source);CutSettings settings;settings.offset_mm=30;
  auto preview=preview_cut(baking.mesh,settings);
  check(preview.changed&&preview.result->cap_triangles==6&&inspect_mesh(preview.result->mesh).closed_manifold(),"Manual plane preview produces the expected closed cut with six cap triangles");
  check(std::abs(preview.plane.offset-.03f)<1e-7f&&preview.cap_mesh.indices.size()==18,"Plane offset uses document millimetres and separates actual cap geometry");
  auto output=source.snapshot();auto report=publish_cut(output,preview,baking,128);
  check(report.changed&&report.shell&&report.cap&&report.projected&&output.objects().size()==3&&output.groups().size()==1,
        "Cut publication creates editable grouped shell/cap objects with mapped surface paint");
  check(!output.object(1).visible&&output.object(1).painted->encode()==source.object(1).painted->encode()&&source.encode()==before,
        "Original paint, geometry and source image bindings remain retained exactly");
  check(inspect_mesh(combined(output)).closed_manifold(),"Separately editable shell and cap export as a geometrically closed combined surface");
  check(output.object(report.cap).material.color==settings.cap_material.color&&output.object(report.shell).painted->baked(),
        "Cap material is independent while the shell retains a persistent PBR bake");
  auto maps=output.object(report.shell).painted->preview(output.object(report.shell).material);
  bool varied=false;std::array<uint8_t,3> first{};bool have=false;
  for(size_t n=0;n<output.object(report.shell).painted->baked()->coverage.size();++n)
    if(output.object(report.shell).painted->baked()->coverage[n]){auto at=n*4;std::array<uint8_t,3> color{maps->color.rgba[at],maps->color.rgba[at+1],maps->color.rgba[at+2]};if(have&&color!=first)varied=true;first=color;have=true;}
  check(varied,"The clipped shell carries varied source texels rather than an intrinsic flat material");
  bool channels=true;
  for(size_t n=0;n<output.object(report.shell).painted->baked()->coverage.size();++n)
    if(output.object(report.shell).painted->baked()->coverage[n])channels&=std::abs(int(maps->metal_rough.rgba[n*4+1])-102)<=1&&
      std::abs(int(maps->metal_rough.rgba[n*4+2])-64)<=1&&maps->emission.rgba[n*4+2]>60;
  check(channels&&maps->normal.width==128,"Mapped transfer retains independent roughness, metallic, emission and canonical normal channels");
  auto named=source.snapshot();named.active().name=std::string(239,'a')+"€"+std::string(10,'b');
  auto full_name=named.active().name;publish_cut(named,preview,baking,64);
  check(named.groups()[0].name==full_name+" cut"&&!named.encode().dump().empty(),
        "Cut group naming preserves complete UTF-8 characters at the native name limit");
  auto layered=source.snapshot();auto paint=layered.active().painted;paint->add_layer("Red detail");
  PaintSettings red;red.color={1,0,0,1};Brush dab;dab.from=dab.to={.1f,0,.2f};dab.radius=.16f;dab.normal={0,0,1};
  paint->begin_stroke();paint->stamp(dab,red);paint->end_stroke();auto layered_before=layered.encode();
  auto layered_high=high(layered);auto layered_preview=preview_cut(layered_high.mesh,settings);
  auto layered_result=publish_cut(layered,layered_preview,layered_high,128);
  auto layered_maps=layered.object(layered_result.shell).painted->preview(layered.object(layered_result.shell).material);
  size_t painted_red=0;for(size_t n=0;n<layered_maps->color.rgba.size();n+=4)
    painted_red+=layered_maps->color.rgba[n]>240&&layered_maps->color.rgba[n+1]<20&&layered_maps->color.rgba[n+2]<20;
  check(layered.encode()["objects"][0]["painted"]==layered_before["objects"][0]["painted"],
        "Original editable paint tiles remain exact after cut publication");
  check(painted_red>4,"Actual editable red paint transfers onto the cut shell");
  auto aborted=source.snapshot();bool cancel=false;
  check(rejects([&]{publish_cut(aborted,preview,baking,128,[&]{return cancel;},[&](float p){if(p>.3f)cancel=true;});})&&aborted.encode()==before,
        "Cancellation during mapped paint transfer preserves the complete detached scene");
  Document doc;doc.replace(source.snapshot());doc.begin_cut();
  check(doc.wait_cut()&&doc.wait_mesh()&&doc.frame().state->error.empty(),"Document cut source and cap preview complete on the dedicated worker");
  auto baseline=doc.frame().surface;
  check(baseline->clipped_object==1&&baseline->cut_session&&baseline->ranges.size()==2&&doc.copy_scene().encode()==before,
        "Live preview retains material bindings and appends only an ephemeral cap without editing the scene");
  check(!doc.begin_desktop()&&rejects([&]{doc.select_object(1);}),"Unconfirmed cut isolates sculpt strokes and source-changing operations");
  for(int n=0;n<20;++n){auto s=doc.frame().state->cut->settings;s.offset_mm=10+n;doc.edit_cut(s);}
  check(doc.wait_cut()&&doc.wait_mesh()&&std::abs(doc.frame().surface->clip_plane.offset-.029f)<1e-7f&&baseline->clip_plane.offset!=doc.frame().surface->clip_plane.offset,
        "Coalesced slider requests reach the latest plane while retaining earlier immutable previews");
  auto s=doc.frame().state->cut->settings;s.flipped=true;doc.edit_cut(s);check(doc.wait_cut()&&doc.wait_mesh()&&doc.frame().surface->clip_plane.normal.x<-.99f,
        "Flip side reverses the retained half-space and preview plane normal");
  check(doc.undo()&&doc.wait_cut()&&!doc.frame().state->cut->settings.flipped,"Universal Undo restores the local cutting-plane draft");
  check(doc.redo()&&doc.wait_cut()&&doc.frame().state->cut->settings.flipped,"Universal Redo restores the local flip setting");
  s=doc.frame().state->cut->settings;s.flipped=false;s.offset_mm=30;doc.edit_cut(s);check(doc.wait_cut(),"A valid cutting plane is restored");
  auto original_offset=doc.frame().state->cut->settings.offset_mm;
  doc.begin_cut_parameter(VrParameter::CutOffset);doc.edit_cut_parameter(0,.55f);doc.end_cut_parameter(false);
  check(doc.wait_cut()&&doc.frame().state->cut->settings.offset_mm==original_offset&&doc.copy_scene().encode()==before,
        "Cancel from a live native plane slider restores settings without changing the scene");
  s=doc.frame().state->cut->settings;s.offset_mm=2000;doc.edit_cut(s);
  check(doc.wait_cut()&&!doc.frame().state->cut->error.empty()&&rejects([&]{doc.end_cut(true);})&&doc.copy_scene().encode()==before,
        "An empty/invalid preview reports feedback and cannot apply the previous valid plane accidentally");
  s.offset_mm=30;doc.edit_cut(s);check(doc.wait_cut()&&doc.wait_mesh()&&doc.frame().state->error.empty(),"Correcting the plane recovers the same workspace");
  doc.end_cut(true);check(doc.wait_asset()&&doc.wait_cut()&&!doc.frame().state->cut&&doc.wait_mesh()&&doc.frame().state->error.empty(),
        "Apply publishes mapped shell/cap objects through the ordered guarded asset worker");
  auto accepted=doc.copy_scene().encode();check(doc.undo()&&doc.copy_scene().encode()==before,"Exactly one global Undo restores the entire pre-cut scene");
  check(doc.redo()&&doc.copy_scene().encode()==accepted,"Global Redo restores output geometry, cap material, bake and source visibility exactly");
  auto directory=std::filesystem::temp_directory_path()/("dome-cut-authoring-"+std::to_string(getpid()));std::filesystem::create_directories(directory);
  doc.save(directory/"cut.dsculpt");check(Scene::load(directory/"cut.dsculpt").encode()==accepted,"Native save/open retains grouped cut outputs and hidden editable source");
  auto exported=doc.export_asset(directory/"cut.glb");exported.get();auto imported=import_mesh(directory/"cut.glb");
  check(imported.primitives.size()==2&&imported.materials.size()>=2,"glTF reimport retains separate shell/cap material primitives without exporting the hidden source");
  std::filesystem::remove_all(directory);
  Document cancel_doc;cancel_doc.replace(source.snapshot());cancel_doc.begin_cut();check(cancel_doc.wait_cut(),"Workspace cancellation fixture initializes");
  auto reference=cancel_doc.frame().surface;cancel_doc.end_cut(false);
  check(!cancel_doc.frame().state->cut&&!cancel_doc.frame().surface->clipped_object&&cancel_doc.copy_scene().encode()==before&&reference->clipped_object,
        "Cancel immediately restores the unclipped immutable baseline while old preview readers remain valid");
  cancel_doc.begin_cut();cancel_doc.replace(source.snapshot());
  check(cancel_doc.wait_cut()&&!cancel_doc.frame().state->cut&&cancel_doc.copy_scene().encode()==before,"Document replacement rejects late cut source/preview publications");
  cancel_doc.begin_cut();check(cancel_doc.wait_cut(),"No-op cut fixture initializes");
  s=cancel_doc.frame().state->cut->settings;s.offset_mm=-1000;cancel_doc.edit_cut(s);
  check(cancel_doc.wait_cut()&&!cancel_doc.frame().state->cut->preview->changed,"A fully retained half-space reports an unchanged preview");
  cancel_doc.end_cut(true);
  check(!cancel_doc.frame().state->cut&&!cancel_doc.undo()&&cancel_doc.copy_scene().encode()==before,"No-op Apply closes the workspace without allocating outputs or global history");
  auto transformed=source.snapshot();auto parent=transformed.add_group("Posed group",{{1,0,.2f},{0,.38268343f,0,.92387953f},2});
  transformed.active().group=parent;const auto &rest=transformed.active().painted->mesh();DeformationState deformation(rest);
  deformation.skeleton=Skeleton{{{"root",-1,{{},{},{1,1,1}},.5f}}};deformation.skin=SkinWeights{};
  deformation.skin->vertices.assign(rest.positions.size(),{{0,1}});deformation.pose.resize(1);deformation.pose[0].translation.y=.05f;
  deformation.morphs=MorphSet(rest);auto edited=rest.positions;for(auto &p:edited)p.x+=.02f;
  deformation.morphs->set_target(rest,"offset",edited);deformation.morph_weights={.5f};transformed.set_deformation(1,std::move(deformation));
  auto placed_before=transformed.encode();Document placed;placed.replace(transformed.snapshot());placed.begin_cut();check(placed.wait_cut(),"Grouped posed-source fixture initializes");
  auto expected_center=ob::transform(transformed.world_transform(1),{.01f,.05f,0});
  check(length(placed.frame().state->cut->center-Vec3{expected_center.x,expected_center.y,expected_center.z})<1e-5f,
        "Cut bounds follow the independently expected world-space skin/morph pose and grouped scale");
  s=placed.frame().state->cut->settings;s.yaw=90;s.offset_mm=20;placed.edit_cut(s);
  check(placed.wait_cut()&&std::abs(placed.frame().state->cut->preview->plane.offset-(expected_center.z+.02f))<1e-5f,
        "Plane orientation and millimetres stay in document space under rotated/scaled source groups");
  placed.end_cut(true);check(placed.wait_asset()&&placed.frame().state->error.empty(),"Posed cut publication completes with mapped source paint");
  auto posed_result=placed.copy_scene();auto joined=combined(posed_result);bool kept=true;
  for(auto p:joined.positions)kept&=p.z>=expected_center.z+.02f-1e-5f;
  check(kept&&inspect_mesh(joined).closed_manifold()&&posed_result.encode()["objects"][0]["deformation"]==placed_before["objects"][0]["deformation"],
        "Grouped cut parts retain the world-space half-space and closed boundary while the original rig/morph data stays exact");
  Document xr;xr.replace(source.snapshot());Controller hand(xr);
  hand.send(0,0,true);hand.send();hand.sector(8);hand.sector(9);hand.sector(4);
  check(xr.frame().state->menu_page==17,"Joystick navigation reaches the native plane-cut page");
  hand.sector(0);check(xr.wait_cut()&&xr.frame().state->cut,"Controller Start cut initializes the same native worker workspace");
  hand.send(0,0,true);hand.send();hand.sector(2);
  check(xr.frame().state->controls.parameter==VrParameter::CutDirection&&xr.frame().state->controls.components==2,
        "Plane direction uses two real adjustable controller slider rows");
  hand.send(1,0);hand.send(0,0,true);hand.send();check(xr.wait_cut(),"Controller direction adjustment completes its live preview");
  hand.sector(3);check(xr.frame().state->controls.parameter==VrParameter::CutCapColor&&xr.frame().state->controls.components==4,
        "Cap material uses four real RGBA controller slider rows");
  hand.send(0,-1);hand.send();
  check(xr.frame().state->controls.selected==1,"Down selects the cap Green row according to the common controller convention");
  hand.send(1,0);hand.send(0,0,true);hand.send();
  check(xr.wait_cut()&&xr.frame().state->cut->settings.cap_material.color[1]>.12f,"Controller row selection and fine adjustment change the actual cap material");
  hand.frame.hands[1].kind=DeviceKind::SpatialStylus;
  hand.frame.hands[1].tracked.grip.position=ob::transform(xr.frame().state->canvas,{.03f,0,0});
  hand.frame.hands[1].tracked.grip.orientation={0,.70710678f,0,.70710678f};hand.send();hand.sector(4);
  check(xr.wait_cut()&&std::abs(xr.frame().state->cut->settings.offset_mm-30)<1e-3f&&xr.frame().state->cut->preview->plane.normal.x>.99f,
        "From hand captures the spatial stylus pose in document space with the expected 30 mm plane");
  // Return to the workspace page, then cap toggle and exact local Undo.
  hand.send(0,0,true);hand.send();
  hand.sector(6);check(xr.wait_cut()&&!xr.frame().state->cut->settings.cap,"Joystick Cap command produces an open trim preview");
  hand.send(0,0,true);hand.send();hand.sector(9);check(xr.wait_cut()&&xr.frame().state->cut->settings.cap,"Controller Undo restores the capped draft");
  hand.send(0,0,true);hand.send();hand.sector(7);xr.finish_spatial();
  check(xr.wait_asset()&&!xr.frame().state->cut&&xr.copy_scene().objects().size()==3&&xr.frame().state->error.empty(),
        "Controller-only Apply survives XR shutdown and publishes editable cut parts");
  Document left_doc;left_doc.replace(source.snapshot());Controller left(left_doc,0);
  left.frame.hands[1].tracked.valid=true;left.send();
  left.send(0,0,true);left.send();left.sector(8);left.sector(9);left.sector(4);left.sector(0);
  check(left_doc.wait_cut()&&left_doc.frame().state->cut,"Left-hand cut workspace initializes with both controllers tracked");
  left.frame.hands[0].tracked.grip.position=ob::transform(left_doc.frame().state->canvas,{0,0,.025f});
  left.frame.hands[1].tracked.grip.position=ob::transform(left_doc.frame().state->canvas,{.15f,0,0});
  left.frame.hands[1].tracked.grip.orientation={0,.70710678f,0,.70710678f};
  left.send(0,0,true);left.send();left.sector(4);
  check(left_doc.wait_cut()&&left_doc.frame().state->cut->preview->plane.normal.z>.99f&&std::abs(left_doc.frame().state->cut->settings.offset_mm-25)<1e-3f,
        "From hand uses the left menu owner rather than the idle tracked right controller");
  std::cout<<checks<<" checks passed\n";
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
