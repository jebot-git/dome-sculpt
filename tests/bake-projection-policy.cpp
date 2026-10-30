// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<std::endl;}
template<class F> bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
Mesh quad(float low=-.2f,float high=.2f,float z=0,bool back=false){Mesh m;m.positions={{low,-.2f,z},{high,-.2f,z},{high,.2f,z},{low,.2f,z}};m.normals.assign(4,back?Vec3{0,0,-1}:Vec3{0,0,1});m.indices=back?std::vector<uint32_t>{0,2,1,0,3,2}:std::vector<uint32_t>{0,1,2,0,2,3};m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};return m;}
struct Fixture{Scene scene;uint64_t target,full,half,miss,back;Fixture(){target=scene.active().id;scene.active().painted=std::make_shared<PaintedMesh>(quad(),64);scene.active().mesh_active=true;auto add=[&](const char*name,Mesh m,std::array<float,4> color){auto id=scene.add(name,Volume{});auto &o=scene.object(id);o.mesh_active=true;o.visible=false;o.material.color=color;o.painted=std::make_shared<PaintedMesh>(std::move(m),64);return id;};full=add("Full red",quad(-.2f,.2f,.05f),{1,0,0,1});half=add("Half blue",quad(-.2f,0,.05f),{0,0,1,1});miss=add("No projection",quad(.5f,.8f,.05f),{0,1,0,1});back=add("Reversed green",quad(-.2f,.2f,.05f,true),{0,1,0,1});scene.select(target);}};
void ready(Document &d){if(!d.wait_asset()||!d.wait_mesh())throw std::runtime_error("Bake policy worker timeout");}
bool same_pixel(const Image &a,const Image &b,size_t at){return std::equal(a.rgba.begin()+at*4,a.rgba.begin()+at*4+4,b.rgba.begin()+at*4);}
struct Driver {Document &doc;SpatialFrame f;explicit Driver(Document&d):doc(d){f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[1].tracked.valid=true;f.hands[1].tracked.grip.position={0,1.48f,-.5f};send();}void send(float x=0,float y=0,bool click=false,float pressure=0){++f.sequence;f.time+=.05;auto &h=f.hands[1];h.x=x;h.y=y;h.joystick=click;h.tracked.trigger=pressure;doc.submit(f);if(!doc.wait_input())throw std::runtime_error("Policy input timeout");}void sector(int i){float a=i*3.14159265358979323846f/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}void open(){send();send(0,0,true);send();sector(8);sector(9);sector(6);sector(11);}void point(float x,float y){auto s=doc.frame().state;auto&h=f.hands[1];h.tracked.aimValid=true;h.tracked.aim.position=ob::transform(s->panel,{(x/640-.5f)*.6f,(.5f-y/640)*.6f,.25f});h.tracked.aim.orientation=s->panel.orientation;send(0,0,false,.8f);send();}void confirm(bool accept){point(accept?470:170,609);}};
}
int main(int argc,char**argv)try {
 if(argc==3&&std::string(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 Fixture f;ObjectBakeSettings options;options.front_mm=options.back_mm=100;options.ao_samples=0;options.padding=0;
 auto first=bake_object(f.scene,f.full,options);check(first.projected==4096&&!first.missed,"Default strict native bake projects the complete source");
 if(argc==3&&std::string(argv[1])=="--ui-fixture"){f.scene.erase(f.full);f.scene.erase(f.miss);f.scene.erase(f.back);f.scene.object(f.half).painted=std::make_shared<PaintedMesh>(quad(-.2f,0,.02f),64);f.scene.select(f.target);f.scene.save(argv[2]);return 0;}

 auto before=f.scene.encode();auto original=f.scene.active().painted->baked();auto layers=f.scene.active().painted->encode()["layers"];
 check(refuses([&]{bake_object(f.scene,f.half,options);})&&f.scene.encode()==before,"Strict projection still refuses partial hits without changing any target or source");
 options.allow_partial=true;auto partial=bake_object(f.scene,f.half,options);auto result=f.scene.active().painted->baked();
 check(partial.projected==2048&&partial.missed==2048&&partial.preserved_misses==2048,"Explicit partial mode reports exact projected, missed and retained texel counts");
 bool untouched=true,changed=true;for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x){size_t at=y*64+x;if(x>=32){for(auto images:{std::pair{&result->maps.color,&original->maps.color},std::pair{&result->maps.metal_rough,&original->maps.metal_rough},std::pair{&result->maps.emission,&original->maps.emission},std::pair{&result->maps.normal,&original->maps.normal}})untouched&=same_pixel(*images.first,*images.second,at);untouched&=result->coverage[at]==original->coverage[at];}else changed&=result->maps.color.rgba[at*4]==0&&result->maps.color.rgba[at*4+2]==255;}
 check(untouched&&changed,"All missed color/ORM/emission/normal/coverage bytes survive while actual hits update");
 check(f.scene.active().painted->encode()["layers"]==layers,"Partial rebake leaves every editable paint layer exact");
 auto endpoint=f.scene.encode();check(refuses([&]{bake_object(f.scene,f.miss,options);})&&f.scene.encode()==endpoint,"A completely missed source refuses explicitly instead of installing an empty or fake-success bake");
 options.allow_partial=false;check(refuses([&]{bake_object(f.scene,f.back,options);})&&f.scene.encode()==endpoint,"Backfaces remain refused by default");
 options.allow_backfaces=true;auto reversed=bake_object(f.scene,f.back,options);check(reversed.projected==4096&&!reversed.missed,"Explicit backface option projects the requested reversed surface");
 Fixture native;options={};options.front_mm=options.back_mm=100;options.ao_samples=0;options.padding=0;bake_object(native.scene,native.full,options);Document d;d.replace(native.scene);ready(d);auto baseline=d.copy_scene().encode();
 d.bake_from_object(native.half,options);ready(d);check(!d.frame().state->error.empty()&&!d.frame().state->can_undo&&d.copy_scene().encode()==baseline,"Document strict refusal preserves history and complete source/target authority");
 options.allow_partial=true;d.bake_from_object(native.half,options);ready(d);check(d.frame().state->error.empty()&&d.frame().state->message.find("2048 missed")!=std::string::npos,"Partial Document bake publishes normal status with visible miss counts");auto after=d.copy_scene().encode();
 check(d.undo()&&d.copy_scene().encode()==baseline&&!d.frame().state->can_undo&&d.redo()&&d.copy_scene().encode()==after,"Accepted partial bake is exactly one native Undo/Redo transaction");ready(d);
 Document xr;xr.replace(native.scene);ready(xr);auto untouched_scene=xr.copy_scene().encode();Driver hand(xr);hand.open();check(xr.frame().state->controls.parameter==VrParameter::BakeProjection&&xr.frame().state->controls.components==2,"Controller baking page exposes independent partial and backface policy rows");
 hand.point(520,240);hand.point(520,320);hand.confirm(false);check(!xr.frame().state->bake_settings.allow_partial&&!xr.frame().state->bake_settings.allow_backfaces&&xr.copy_scene().encode()==untouched_scene,"XR Cancel discards both projection choices without changing the document");
 hand.sector(11);hand.point(520,240);hand.point(520,320);hand.confirm(true);check(xr.frame().state->bake_settings.allow_partial&&xr.frame().state->bake_settings.allow_backfaces&&!xr.frame().state->dirty&&!xr.frame().state->can_undo,"XR confirmation changes only explicit bake configuration");
 hand.sector(11);hand.point(100,240);hand.f.focused=false;hand.send();check(xr.frame().state->controls.parameter==VrParameter::None&&xr.frame().state->bake_settings.allow_partial&&xr.copy_scene().encode()==untouched_scene,"OpenXR-style focus loss discards unconfirmed policy while retaining prior choices and source");
 std::cout<<checks<<" native bake projection policy checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
