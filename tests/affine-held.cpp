// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
#include <atomic>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
#include <unistd.h>
using namespace dome;
namespace dome {struct DocumentTransactionTestAccess {static std::thread::id input(Document&d){return d.input_thread_.get_id();}};}
namespace {
thread_local bool armed=false,hit=false;thread_local size_t calls=0,fail_at=SIZE_MAX;
std::atomic_bool input_armed=false;std::atomic<size_t> input_calls=0;size_t input_fail=1;std::thread::id input_thread;
size_t checks=0;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;}
void near(const AffineTransform&a,const AffineTransform&b,const char*m){for(size_t i=0;i<16;++i)if(std::abs(a.matrix()[i]-b.matrix()[i])>3e-6*std::max(1.,std::abs(b.matrix()[i])))throw std::runtime_error(m);++checks;}
Mesh quad(){Mesh m;m.positions={{-.3f,-.3f,0},{.3f,-.3f,0},{.3f,.3f,0},{-.3f,.3f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
Scene scene(){Scene s;auto group=s.add_group("Nonuniform reflected source");auto a=AffineTransform{}.matrix();a[0]=-2;a[4]=.3;a[10]=.7;s.group(group).affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(a));s.active().group=group;s.active().painted=std::make_shared<PaintedMesh>(quad(),64);s.active().mesh_active=true;auto e=AffineTransform{}.matrix();e[8]=.2;s.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(e));return s;}
AffineTransform translation(double x,double y=0,double z=0){auto m=AffineTransform{}.matrix();m[12]=x;m[13]=y;m[14]=z;return AffineTransform(m);}
AffineTransform current(Document&d){const auto state=d.frame().state;return *state->objects[0].affine_world;}
void ready(Document&d){check(d.wait_mesh(std::chrono::seconds(12))&&d.frame().state->error.empty(),"held fixture publishes current immutable source");}
struct Driver {
 Document&d;SpatialFrame f;
 explicit Driver(Document&doc):d(doc){f.focused=f.head_valid=true;f.head.position={0,1.6f,0};for(auto&h:f.hands)h.tracked.valid=true;f.hands[0].tracked.grip.position={0,1.3f,-.4f};f.hands[1].tracked.grip.position={.5f,1.3f,-.4f};send();}
 void send(bool check_error=true){++f.sequence;f.time+=.05;d.submit(f);check(d.wait_input(),"actual controller frame drains");if(check_error)check(d.frame().state->error.empty(),"actual affine XR input reports no Canvas approximation error");}
};
}
void*operator new(size_t n){if(armed&&++calls>=fail_at){hit=true;throw std::bad_alloc();}if(input_armed.load()&&std::this_thread::get_id()==input_thread&&++input_calls>=input_fail)throw std::bad_alloc();if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main(int argc,char**argv)try {
 const std::string mode=argc>1?argv[1]:"";
 if(argc==3&&mode=="--ui-fixture") {auto s=scene();s.active().material.color={.8f,.1f,.05f,1};s.active().material.unlit=true;auto other=s.add("Other mesh",Volume{});s.object(other).painted=std::make_shared<PaintedMesh>(quad(),64);s.object(other).mesh_active=true;s.object(other).transform.position.x=.7f;s.object(other).material.color={.05f,.3f,.9f,1};s.object(other).material.unlit=true;s.select(s.objects()[0].id);s.save(argv[2]);return 0;}
 if(argc==3&&mode=="--inspect") {auto s=Scene::load(argv[2]);auto out=nlohmann::json::array();for(const auto&o:s.objects()){auto world=s.world_matrix(o.id);out.push_back({{"id",o.id},{"x",world.point({0,0,0})[0]},{"world",world.matrix()},{"visible",o.visible},{"locked",o.locked},{"color",o.material.color},{"rest",o.painted?o.painted->encode():nlohmann::json(nullptr)}});}std::cout<<out.dump()<<'\n';return 0;}
 auto home=std::filesystem::temp_directory_path()/("dome-held-affine-"+std::to_string(getpid()));std::filesystem::create_directories(home);setenv("HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);
 struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{home};
 auto source=scene();const auto baseline=source.encode();const auto world=source.world_matrix(source.active().id);
 Document d;d.replace(source);d.configure_object_grab(true,true);ready(d);auto original_mesh=d.frame().surface;
 if(mode=="--input-oom") {
   Driver c(d);c.f.hands[0].tracked.squeeze=1;c.send();const auto previous=d.frame().state;input_thread=DocumentTransactionTestAccess::input(d);input_calls=0;input_armed=true;c.f.hands[0].tracked.grip.position.x+=.1f;c.send(false);input_armed=false;
   check(input_calls>0&&d.copy_scene().encode()==baseline,"persistent controller allocation denial retains exact authoritative source");d.report_error({});c.send();near(current(d),translation(.1)*world,"controller retries from the unchanged pre-failure anchor");d.end_object_grab(false);std::cout<<checks<<" input OOM lifecycle checks passed\n";return 0;
 }
 check(d.begin_object_grab(source.active().id),"native affine holding starts");
 CanvasTransform delta{{.2f,-.1f,.15f},{0,0,std::sin(.2f),std::cos(.2f)},1.3f};
 d.move_held_object_world(delta);near(current(d),affine_similarity(delta)*world,"world gesture composes before original source world frame");
 check(d.copy_scene().encode()==baseline&&d.frame().surface==original_mesh,"held preview retains authoritative Scene and packed mesh owner");
 if(mode=="--faults") {
   d.move_held_object_world({});calls=0;fail_at=SIZE_MAX;armed=true;d.move_held_object_world(delta);armed=false;const auto allocations=calls;d.move_held_object_world({});
   for(size_t at=1;at<=allocations;++at){const auto before=d.frame().state;calls=0;fail_at=at;hit=false;armed=true;bool caught=false;try{d.move_held_object_world(delta);}catch(const std::bad_alloc&){caught=true;}armed=false;check(hit&&caught&&d.frame().state==before,"every held preview allocation failure restores exact publication/preview owner");check(d.copy_scene().encode()==baseline,"denied held preview preserves source");near(current(d),world,"denied held preview preserves full world frame");d.move_held_object_world(delta);near(current(d),affine_similarity(delta)*world,"held retry remains exact");d.move_held_object_world({});}
   d.end_object_grab(false);check(d.copy_scene().encode()==baseline,"all failed preview retries cancel without source/history changes");std::cout<<checks<<" held allocation assertions at"<<allocations<<" persistent boundaries passed\n";return 0;
 }
 d.end_object_grab(false);check(d.copy_scene().encode()==baseline&&!d.frame().state->can_undo,"native cancel discards full affine preview without history");
 check(d.begin_object_grab(source.active().id),"second native affine holding starts");d.move_held_object_world(delta);d.end_object_grab();ready(d);
 near(d.copy_scene().world_matrix(source.active().id),affine_similarity(delta)*world,"held release commits full world frame without shear loss");check(d.copy_scene().groups()[0].affine_edit==source.groups()[0].affine_edit,"held release leaves immutable original parent authority untouched");
 check(d.undo()&&d.copy_scene().encode()==baseline,"held transform has one exact source-preserving Undo");ready(d);
 Driver c(d);c.f.hands[0].tracked.squeeze=1;c.send();check(d.frame().state->object_grabbing,"actual physical grip begins affine hold");
 c.f.hands[0].tracked.grip.position.x+=.15f;c.send();near(current(d),translation(.15)*world,"actual one-hand controller translates full affine source");
 auto moved=current(d);c.f.hands[1].tracked.squeeze=1;c.send();near(current(d),moved,"one-to-two hand rebase has no jump");
 for(auto &h:c.f.hands)h.tracked.grip.position.y+=.1f;c.send();near(current(d),translation(.15,.1)*world,"two-hand translation uses current full preview pivot");
 const auto canvas=d.frame().state->canvas;const auto old_mid=(c.f.hands[0].tracked.grip.position+c.f.hands[1].tracked.grip.position)*.5f;
 c.f.hands[1].tracked.grip.position.x+=.175f;const auto new_mid=(c.f.hands[0].tracked.grip.position+c.f.hands[1].tracked.grip.position)*.5f;const float scale=ob::length(c.f.hands[1].tracked.grip.position-c.f.hands[0].tracked.grip.position)/.35f;
 // Start separation is .35 after the first hand translated .15 from its grip.
 const auto a=world_to_model(canvas,{old_mid.x,old_mid.y,old_mid.z}),b=world_to_model(canvas,{new_mid.x,new_mid.y,new_mid.z});auto sm=AffineTransform{}.matrix();sm[0]=sm[5]=sm[10]=scale;sm[12]=b.x-scale*a.x;sm[13]=b.y-scale*a.y;sm[14]=b.z-scale*a.z;
 c.send();const auto expected=AffineTransform(sm)*translation(.15,.1)*world;near(current(d),expected,"actual two-hand scaling applies world metric around tracked midpoint");
 moved=current(d);c.f.hands[1].tracked.squeeze=0;c.send();near(current(d),moved,"two-to-one hand rebase has no jump");c.f.hands[0].tracked.squeeze=0;c.send();ready(d);near(d.copy_scene().world_matrix(source.active().id),expected,"physical release retains complete preview world frame");
 check(d.undo()&&d.copy_scene().encode()==baseline,"physical multi-hand gesture has one exact Undo");ready(d);
 c.f.hands[0].tracked.squeeze=1;c.send();c.f.hands[0].tracked.grip.position.x+=.1f;c.send();c.f.focused=false;c.send();check(d.copy_scene().encode()==baseline&&!d.frame().state->object_grabbing,"XR focus loss cancels affine preview without source mutation");
 std::cout<<checks<<" actual affine held/controller checks passed\n";return 0;
}catch(const std::exception&e){armed=false;input_armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
