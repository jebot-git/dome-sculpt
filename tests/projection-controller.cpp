// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "import-controller-driver.hpp"
#include <nlohmann/json.hpp>
#include <png.h>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;std::cout<<"PASS "<<m<<'\n';}
Scene fixture(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};Scene s;s.active().painted=std::make_shared<PaintedMesh>(m,64);s.active().mesh_active=true;return s;}
void settled(Document &d){check(d.wait_native_stroke()&&d.wait_mesh(),"Controller/native workers settle");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
}
int main()try{
 auto root=std::filesystem::temp_directory_path()/("dome-projection-controller-"+std::to_string(getpid()));std::filesystem::create_directories(root);setenv("HOME",root.c_str(),1);setenv("XDG_STATE_HOME",root.c_str(),1);setenv("XDG_CONFIG_HOME",root.c_str(),1);struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{root};
 const auto image=root/"stencil.png";png_image png{};png.version=PNG_IMAGE_VERSION;png.width=png.height=1;png.format=PNG_FORMAT_RGBA;const uint8_t rgba[]={255,0,0,255};if(!png_image_write_to_file(&png,image.c_str(),0,rgba,0,nullptr))throw std::runtime_error(png.message);
 Document d;d.replace(fixture());settled(d);d.load_projection_image(image);check(d.wait_projection_image(),"Actual detached image worker ready");const auto before=d.copy_scene().encode();
 ImportDriver driver(d);driver.open();driver.sector(9);driver.sector(3);driver.sector(11);driver.sector(9);
 check(d.frame().state->menu_page==25&&d.frame().state->menu,"Actual controller navigates Materials / Paint / Controls / Image projection");
 driver.sector(9);check(d.frame().state->menu_page==4,"Existing color presets remain accessible through projection page");driver.sector(8);driver.sector(9);
 driver.sector(3);check(d.frame().state->projection->panel==ProjectionPanel::Parameters,"Controller enters shared parameter panel");auto old=d.projection_settings().camera.width;driver.sector(2);driver.sector(2);driver.sector(11);check(d.projection_settings().camera.width>old,"Actual joystick changes selected world-width parameter");
 driver.sector(6);check(d.projection_settings().image.usage==ProjectionUsage::Stencil,"Actual joystick selects image stencil mode");driver.sector(6);
 const auto original=d.projection_settings().camera.pose;
 driver.sector(7);check(d.frame().state->projection->placing&&!d.frame().state->menu,"Place projector owns input separately from painting");
 driver.frame.hands[1].tracked.grip=ob::compose(d.frame().state->canvas,CanvasTransform{{.1f,.2f,1.5f},{},1});driver.send(0,0,false,.8f);driver.send();
 check(!d.frame().state->projection->placing&&d.frame().state->menu_page==25&&std::abs(d.projection_settings().camera.pose.position.x-.1f)<1e-5,"Fresh trigger press/release places projector in scene frame");
 check(d.copy_scene().encode()==before&&!d.frame().state->can_undo,"Image and projector placement are ephemeral, with no scene/history mutation");
 driver.sector(7);driver.frame.hands[1].tracked.grip.position.x+=.3f;driver.send(0,0,false,.8f);driver.frame.focused=false;driver.send(0,0,false,.8f);
 check(!d.frame().state->projection->placing&&std::abs(d.projection_settings().camera.pose.position.x-.1f)<1e-5,"Focus loss restores exact prior projector placement");
 driver.frame.focused=true;driver.send();auto settings=d.projection_settings();settings.camera.pose=original;settings.camera.width=settings.camera.height=2;d.configure_projection(settings);
 // Restart input with neutral tracked hands; the projection tool also routes an
 // ordinary undeformed mesh through the existing world-space native worker.
 driver.frame.hands[1].tracked.grip=ob::compose(d.frame().state->canvas,CanvasTransform{});driver.send();driver.send(0,0,false,1);check(d.frame().state->native_pending,"Image tool admits ordinary controller contact on native worker");driver.send();
 const auto file=root/"contact.dsculpt";auto saved=d.save_async(file);saved.get();settled(d);auto result=d.copy_scene();check(result.active().painted->pixel(1,32,32).data[0]>240&&result.active().painted->pixel(1,32,32).data[9]>200,"Actual controller contact writes projected image pixels");check(Scene::load(file).encode()==result.encode(),"Controller release plus immediate Save captures exact image endpoint");check(d.undo()&&d.copy_scene().encode()==before,"Actual image stroke is one exact Undo");
 std::cout<<checks<<" projection controller checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
