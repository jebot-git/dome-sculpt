// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {size_t checks=0;void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;std::cout<<"PASS "<<m<<'\n';}}
int main()try{
 const auto root=std::filesystem::temp_directory_path()/("dome-projection-held-"+std::to_string(getpid()));std::filesystem::create_directories(root);setenv("HOME",root.c_str(),1);setenv("XDG_STATE_HOME",root.c_str(),1);setenv("XDG_CONFIG_HOME",root.c_str(),1);struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{root};
 Mesh mesh;mesh.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};mesh.normals.assign(4,{0,0,1});mesh.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};mesh.indices={0,1,2,0,2,3};Scene scene;scene.active().painted=std::make_shared<PaintedMesh>(mesh,64);scene.active().mesh_active=true;auto matrix=AffineTransform{}.matrix();matrix[0]=-1.5;matrix[4]=.2;scene.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(matrix));
 const auto file=root/"gradient.png";auto png=Image{2,1,{255,0,0,255,0,0,255,255}}.png();{std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(png.data()),png.size());}
 Document d;d.replace(scene);check(d.wait_mesh(),"Affine source preview ready");ProjectionCamera camera;camera.pose.position={0,0,2};camera.width=camera.height=2;d.load_projection_image(file,camera);check(d.wait_projection_image(),"Fixed scene-world gradient projector ready");d.configure(Tool::Clay,.2f);d.configure_object_grab(true,true);check(d.wait_mesh(),"Hold-aware worker preview ready");const auto before=d.copy_scene().encode();
 SpatialFrame f;f.focused=f.head_valid=true;f.head.position={0,1.6f,0};for(auto &h:f.hands)h.tracked.valid=true;f.hands[0].tracked.grip.position={0,1.4f,-.3f};
 auto tool=[&](Vec3 p){f.hands[1].tracked.grip=ob::compose(d.frame().state->canvas,CanvasTransform{{p.x,p.y,p.z},{},1});};
 auto send=[&](float trigger){++f.sequence;f.time+=.05;f.hands[1].tracked.trigger=trigger;d.submit(f);check(d.wait_input(),"Tracked held paint packet drains");};
 send(0);tool({0,0,2});send(0);f.hands[0].tracked.squeeze=1;send(0);check(d.frame().state->object_grabbing,"Physical first-hand squeeze owns affine object");send(1);check(d.frame().state->native_pending,"Second hand starts detached projected contact away from surface");
 const auto delta=ob::rotate(d.frame().state->canvas.orientation,{.4f*d.frame().state->canvas.scale,0,0});f.hands[0].tracked.grip.position=f.hands[0].tracked.grip.position+delta;tool({.4f,0,0});send(1);check(d.copy_scene().encode()==before,"Held motion and image paint remain detached before release");
 f.hands[0].tracked.squeeze=0;send(1);const auto path=root/"held.dsculpt";auto saved=d.save_async(path);saved.get();check(d.wait_native_stroke()&&d.wait_mesh(),"Held/image release plus ordered Save finishes");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);
 const auto after=d.copy_scene();const auto pixel=after.active().painted->pixel(1,32,32);const float x=-1.5f*(32.5f/64-.5f)+.2f*(32.5f/64-.5f)+.4f;const float blue=x+.5f;const int red_expected=std::lround(linear_to_srgb(1-blue)*255),blue_expected=std::lround(linear_to_srgb(blue)*255);
 std::cout<<"Gradient actual "<<int(pixel.data[0])<<','<<int(pixel.data[2])<<" expected "<<red_expected<<','<<blue_expected<<'\n';
 check(std::abs(int(pixel.data[0])-red_expected)<=2&&std::abs(int(pixel.data[2])-blue_expected)<=2,"Held reference camera samples independent CURRENT WORLD gradient, not frozen original point");
 check(std::abs(int(pixel.data[0])-188)>30&&pixel.data[9]>200,"Wrong held/reference frame is visibly discriminated at the original UV center");
 auto expected=matrix;expected[12]=.4;for(size_t i=0;i<16;++i)check(std::abs(after.world_matrix(1).matrix()[i]-expected[i])<2e-6,"Release preserves full reflected/sheared source matrix with world delta");
 check(after.active().painted->mesh_owner()==scene.active().painted->mesh_owner(),"Held image edit retains original immutable rest/UV owner");check(Scene::load(path).encode()==after.encode(),"Immediately accepted Save/restart retains complete held projected endpoint");check(d.undo()&&d.copy_scene().encode()==before,"Combined held placement and image paint is one exact Undo");
 std::cout<<checks<<" held projection checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
