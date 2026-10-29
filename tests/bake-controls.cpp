// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <thread>
using namespace dome;
namespace {
void check(bool b,const char*label){if(!b)throw std::runtime_error(label);std::cout<<"PASS "<<label<<'\n';}
Scene fixture(uint32_t resolution=128){
  Mesh m;m.positions={{-.1f,-.1f,0},{.1f,-.1f,0},{.1f,.1f,0},{-.1f,.1f,0}};
  m.normals.assign(4,{0,0,1});m.indices={0,1,2,0,2,3};m.uv={{{.125f,.125f}},{{.875f,.125f}},{{.875f,.875f}},{{.125f,.875f}}};
  Scene s;s.active().name="Bake target";s.active().mesh_active=true;s.active().painted=std::make_shared<PaintedMesh>(m,resolution);
  s.active().painted->add_layer("Keep editable paint");s.active().painted->layer_properties("Keep editable paint",false,.5f);
  auto target=s.active().id;
  for(int k=0;k<2;++k){s.add(k?"Green source":"Blue source",Volume{});auto &o=s.active();o.mesh_active=true;o.visible=false;o.locked=true;
    auto high=m;for(auto &p:high.positions)p.z=.01f;
    o.painted=std::make_shared<PaintedMesh>(high,64);o.material.color=k?std::array<float,4>{.1f,.8f,.2f,1}:std::array<float,4>{.1f,.2f,.8f,1};
  }
  s.select(target);return s;
}
struct Driver {
  Document &doc;SpatialFrame f;
  Driver(Document&d):doc(d){f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[1].tracked.valid=true;f.hands[1].tracked.grip.position={0,1.48f,-.5f};send();}
  void send(float x=0,float y=0,bool press=false,float force=0){++f.sequence;f.time+=.05;auto &h=f.hands[1];h.x=x;h.y=y;h.joystick=press;h.tracked.trigger=force;doc.submit(f);if(!doc.wait_input())throw std::runtime_error("Controller input timeout");}
  void sector(int k){float a=k*3.14159265359f/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
  void open(){send();send(0,0,true);send();sector(8);sector(9);sector(6);}
  void ray(float u,float v){auto state=doc.frame().state;auto &h=f.hands[1];h.tracked.aimValid=true;h.tracked.aim.position=ob::transform(state->panel,{(u-.5f)*.6f,(.5f-v)*.6f,.25f});h.tracked.aim.orientation=state->panel.orientation;send(0,0,false,.8f);send();}
  void value(float t,int component=0){auto c=doc.frame().state->controls;float y=c.components==1?300:c.components==4?210+component*70:240+component*80;ray((100+420*t)/640.f,y/640.f);}
  void confirm(){send(0,0,true);send();}
};
}
int main(int argc,char**argv)try{
  if(argc==3 && (std::string(argv[1])=="--ui-fixture" || std::string(argv[1])=="--ui-large-fixture")){fixture(std::string(argv[1])=="--ui-large-fixture"?512:128).save(argv[2]);return 0;}
  check(!spatial_parameter(12,-1) && !spatial_parameter(12,12) && !spatial_action(12,-1),"Baking menu mappings reject invalid sectors");
  auto scene=fixture();auto before=scene.encode();Document doc;doc.replace(scene);
  check(doc.frame().state->can_bake && doc.frame().state->bake_sources.size()==2 && !doc.frame().state->can_clear_bake,"Prepared targets expose both hidden locked sources and no premature Clear command");
  Driver driver(doc);driver.open();check(doc.frame().state->menu_page==12,"Joystick menu reaches Objects / Mesh tools / Baking");
  driver.sector(0);check(doc.frame().state->controls.parameter==VrParameter::BakeSource,"Source selection opens a controller slider");driver.value(1);driver.confirm();
  check(doc.frame().state->bake_source==scene.objects()[2].id && doc.copy_scene().encode()==before && !doc.frame().state->dirty,"Source choice is staged configuration with no scene or history mutation");
  driver.sector(1);auto c=doc.frame().state->controls;
  check(c.components==3 && c.logarithmic,"Front, back and AO world distances share three logarithmic ray sliders");
  driver.value(std::log(20/c.minimum)/std::log(c.maximum/c.minimum));driver.ray(.25f,.95f);
  check(doc.frame().state->bake_settings.front_mm==50 && doc.copy_scene().encode()==before,"Ray Cancel discards staged projection bounds exactly");
  driver.sector(1);c=doc.frame().state->controls;driver.value(std::log(20/c.minimum)/std::log(c.maximum/c.minimum));driver.confirm();
  check(std::abs(doc.frame().state->bake_settings.front_mm-20)<.01f,"Joystick confirmation retains the chosen world-millimetre bounds");
  driver.sector(2);check(doc.frame().state->controls.components==2,"AO sampling and seam padding expose independent controller rows");driver.value(0);driver.value(.125f,1);driver.confirm();
  check(doc.frame().state->bake_settings.ao_samples==0 && doc.frame().state->bake_settings.padding==8 && !doc.frame().state->dirty,"Quality confirmation selects integer AO samples and padding without dirtying the scene");
  driver.sector(3);check(doc.wait_asset() && doc.frame().state->error.empty(),"Controller Bake runs the guarded native worker");
  auto baked=doc.copy_scene().encode();check(doc.copy_scene().active().painted->baked() && baked["objects"][1]==before["objects"][1] && baked["objects"][2]==before["objects"][2] && doc.frame().state->can_clear_bake,"Accepted baking retains both complete hidden sources and enables Clear");
  check(doc.undo() && doc.copy_scene().encode()==before && doc.redo() && doc.copy_scene().encode()==baked,"Baking is one exact global Undo/Redo transaction");
  doc.clear_bake();check(doc.wait_asset() && !doc.copy_scene().active().painted->baked() && doc.undo() && doc.copy_scene().encode()==baked,"Explicit Clear removes only the baked base and is exactly undoable");
  auto confirmed_front=doc.frame().state->bake_settings.front_mm;
  doc.replace(scene);Driver focus(doc);focus.open();focus.sector(1);focus.value(.8f);focus.f.focused=false;focus.send();
  check(doc.frame().state->controls.parameter==VrParameter::None && doc.frame().state->bake_settings.front_mm==confirmed_front && doc.copy_scene().encode()==before,"Focus loss discards staged bake settings and retains previously confirmed defaults");
  doc.replace(scene);std::atomic_bool started=false;
  doc.edit_scene("Cancellable fixture",[&](Scene&s,const auto&cancelled){started.store(true);while(!cancelled())std::this_thread::yield();s.active().name="Must never publish";});
  auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!started && std::chrono::steady_clock::now()<until)std::this_thread::yield();
  check(started && doc.frame().state->can_cancel_asset,"An executing detached job exposes cancellation without blocking input");doc.cancel_asset_work();
  check(doc.wait_asset() && doc.copy_scene().encode()==before && !doc.frame().state->can_undo && doc.frame().state->error.empty() && doc.frame().state->message.find("cancelled")!=std::string::npos,"Cancellation rejects even a returned mutated candidate with no scene/history changes");
  auto large=fixture(512);auto original=large.encode();doc.replace(large);ObjectBakeSettings options;options.ao_samples=256;
  doc.bake_from_object(large.objects()[1].id,options);
  until=std::chrono::steady_clock::now()+std::chrono::seconds(5);float previous=0;bool monotonic=true;
  while(doc.frame().state->asset_busy && doc.asset_status().progress<.13f && std::chrono::steady_clock::now()<until){auto p=doc.asset_status().progress;monotonic&=p>=previous;previous=p;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(doc.frame().state->asset_busy && doc.asset_status().progress>=.13f && monotonic,"Live bake progress advances monotonically into actual projection work");
  Driver cancel(doc);cancel.open();check(doc.frame().state->menu_page==12 && doc.frame().state->menu_enabled[5],"The headset baking page exposes Cancel while the worker is running");cancel.sector(5);
  check(doc.wait_asset() && doc.copy_scene().encode()==original && !doc.frame().state->dirty && doc.frame().state->error.empty(),"Cancelling a running AO/projection bake retains the exact pre-bake target and sources");
  return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
