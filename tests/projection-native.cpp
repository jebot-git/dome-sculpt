// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
#include "dome/source-maps.hpp"
#include <nlohmann/json.hpp>
#include <png.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
void png(const std::filesystem::path &path){png_image p{};p.version=PNG_IMAGE_VERSION;p.width=p.height=2;p.format=PNG_FORMAT_RGBA;const uint8_t bytes[]={255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255};if(!png_image_write_to_file(&p,path.c_str(),0,bytes,0,nullptr))throw std::runtime_error(p.message);}
Scene source(bool posed){Scene s;const auto m=quad();s.active().painted=std::make_shared<PaintedMesh>(m,64);s.active().mesh_active=true;if(posed){auto a=AffineTransform{}.matrix();a[0]=-2;a[2]=.5;a[4]=.3;a[12]=.7;a[13]=-.2;a[14]=.3;s.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(a));DeformationState d(m);d.skeleton=Skeleton{{{"joint",-1,{},1}}};d.skin=SkinWeights{};d.skin->vertices.assign(4,{{0,1}});d.pose.resize(1);d.pose[0].translation={1,0,0};d.pose[0].scale={.5f,1.5f,1.2f};d.morphs=MorphSet(m);auto moved=m.positions;for(auto&p:moved)p+=Vec3{.2f,.1f,.1f};d.morphs->set_target(m,"Moved",moved);d.morph_weights={1};s.set_deformation(1,d);}return s;}
void ready(Document &d){if(!d.wait_mesh())throw std::runtime_error("Mesh timeout");}
ProjectionCamera camera(Vec3 center){ProjectionCamera c;c.pose.position={center.x,center.y,center.z+2};c.width=c.height=2;return c;}
void load(Document &d,const std::filesystem::path &path,Vec3 center){d.load_projection_image(path,camera(center));check(d.wait_projection_image(),"Detached PNG load completes");check(d.frame().state->projection&&d.frame().state->projection->image&&d.frame().state->projection->settings.enabled&&!d.frame().state->asset_busy,"Immutable image publication enables projection without owning Scene result");}
void stroke(Document &d,Vec3 center,bool accept=true){check(d.begin_native_stroke(),"Projection native ray stroke admitted");NativeStrokeSample s;s.origin={center.x,center.y,center.z+1};s.direction={0,0,-1};s.brush.radius=.2f;s.brush.blend=0;d.sample_native_stroke(s);d.end_native_stroke(accept);check(d.wait_native_stroke(),"Projection stroke worker drains");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);ready(d);}
}
int main(int argc,char **argv)try{
 if(argc==3&&std::string(argv[1])=="--image-fixture"){png(argv[2]);return 0;}
 if(argc==3&&std::string(argv[1])=="--ui-fixture"){auto scene=source(false);scene.active().material.unlit=true;scene.active().material.color={0,0,1,1};scene.save(argv[2]);return 0;}
 if(argc==3&&std::string(argv[1])=="--inspect"){auto scene=Scene::load(argv[2]);auto p=scene.active().painted->pixel(1,32,32);std::cout<<nlohmann::json{{"scene",scene.encode()},{"red",p.data[0]},{"blue",p.data[2]},{"coverage",p.data[9]}}.dump()<<'\n';return 0;}

 const auto root=std::filesystem::temp_directory_path()/("dome-projection-"+std::to_string(getpid()));std::filesystem::create_directories(root);setenv("HOME",root.c_str(),1);setenv("XDG_STATE_HOME",root.c_str(),1);setenv("XDG_CONFIG_HOME",root.c_str(),1);
 struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{root};const auto image=root/"紅色.png";png(image);
 for(int variant=0;variant<3;++variant){
  const bool posed=variant==1;auto scene=source(posed);Vec3 center=posed?Vec3{-1.455f,-.05f,.97f}:Vec3{};
  if(variant==2){CanvasTransform root;root.position={1,0,0};root.scale=2;auto parent=scene.add_group("Parent",root);CanvasTransform nested;nested.position={0,1,0};scene.active().group=scene.add_group("Nested",nested,parent);scene.active().transform.position.z=.5f;center={1,2,1};}Document d;d.replace(scene);ready(d);auto before=d.copy_scene().encode();load(d,image,center);
  PaintSettings brush;brush.color={0,0,1,1};d.paint_settings(brush);stroke(d,center);auto result=d.copy_scene();const auto pixel=result.active().painted->pixel(1,32,32);
  check(pixel.data[9]>200&&pixel.data[0]>240&&pixel.data[2]<5,"Actual image replaces contrasting brush color at evaluated UV center");
  check(result.active().painted->mesh_owner()==scene.active().painted->mesh_owner()&&result.active().deformation==scene.active().deformation&&result.active().affine_edit==scene.active().affine_edit,"Paint retains exact REST mesh, skin/morph and full reflected-sheared placement owners");
  check(result.active().painted->pixel(1,5,5).data[9]==0,"World-metric brush limits projection footprint");
  const auto after=result.encode();check(d.undo()&&d.copy_scene().encode()==before&&d.redo()&&d.copy_scene().encode()==after,"One accepted projection gesture is one exact global Undo/Redo");ready(d);
  stroke(d,center,false);check(d.copy_scene().encode()==after,"Cancelled projection keeps authoritative scene exact");
  d.undo();ready(d);check(d.begin_native_stroke(),"Ordered save projection starts");NativeStrokeSample sample;sample.origin={center.x,center.y,center.z+1};sample.direction={0,0,-1};sample.brush.radius=.2f;d.sample_native_stroke(sample);const auto file=root/(posed?"posed.dsculpt":"plain.dsculpt");auto saved=d.save_async(file);saved.get();check(d.wait_native_stroke(),"Save accepts and waits for projection endpoint");check(Scene::load(file).encode()==d.copy_scene().encode(),"Immediate ordered Save/restart retains exact projected pixels and source archives");
 }
 for(int policy=0;policy<7;++policy){
  auto scene=source(false);const auto other=scene.add("Occluder",Volume(.02f));scene.object(other).painted=std::make_shared<PaintedMesh>(quad(),64);scene.object(other).mesh_active=true;scene.object(other).transform.position.z=.5f;
  if(policy==1)scene.object(other).visible=false;
  if(policy==2){scene.object(other).material.alpha=Material::AlphaMode::Blend;scene.object(other).material.color[3]=0;}
  if(policy==5||policy==6){auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{1,1,{255,255,255,uint8_t(policy==5?0:255)}});scene.object(other).painted=PaintedMesh::from_maps(quad(),maps,64);scene.object(other).material.alpha=Material::AlphaMode::Mask;}
  scene.select(1);Document d;d.replace(scene);ready(d);load(d,image,{});if(policy==3){auto settings=d.projection_settings();settings.occlusion=ProjectionOcclusion::TargetOnly;d.configure_projection(settings);}if(policy==4)d.configure_object_grab(true,true); // Selection focus ghosts below.
  if(policy==4){const uint64_t selected=1;d.select_objects(std::span<const uint64_t>(&selected,1));ready(d);check(d.frame().state->objects[1].ghost,"Occluder is deliberately an editor ghost");}
  stroke(d,{});const bool painted=d.copy_scene().active().painted->pixel(1,32,32).data[9]>200;
  check(painted==(policy!=0&&policy!=6),"Visible-scene policy honors opaque/hidden/material-transparent/target-only/editor-ghost distinctions");
 }
 {Document d;d.replace(source(false));ready(d);auto before=d.copy_scene().encode();std::ofstream(root/"bad.png")<<"not a png";d.load_projection_image(root/"bad.png");check(d.wait_projection_image()&&!d.frame().state->projection->error.empty(),"Malformed PNG reports worker error without stuck loading");check(d.copy_scene().encode()==before&&!d.frame().state->can_undo,"Detached decode failure changes no Scene/history");load(d,image,{});check(d.copy_scene().encode()==before,"Image retry succeeds without Scene mutation");d.browse_projection_images(root);check(d.wait_projection_image()&&d.frame().state->projection->entries.size()>=2,"Bounded worker browser exposes PNG images");}
 {Document d;d.replace(source(true));ready(d);load(d,image,{});d.configure_shape_sculpt(true);ready(d);const auto before=d.copy_scene().encode();check(d.begin_native_stroke(),"Remembered image tool does not block explicit shapekey sculpt mode");d.end_native_stroke(false);check(d.wait_native_stroke()&&d.copy_scene().encode()==before,"Cancelling shapekey mode preserves all source and image state");}
 {Document d;d.replace(source(false));ready(d);load(d,image,{});d.use_source();ready(d);const auto before=d.copy_scene().encode();check(d.begin_native_stroke(),"Remembered paint projection does not block switching to source-volume sculpting");d.end_native_stroke(false);check(d.wait_native_stroke()&&d.copy_scene().encode()==before,"Source sculpt cancellation retains dormant painted mesh exactly");}
 std::cout<<checks<<" native projection workflow checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
