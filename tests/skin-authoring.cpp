// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/skin-authoring.hpp"
#include "dome/document.hpp"
#include "dome/creation.hpp"
#include <fstream>
#include <numbers>
#include <nlohmann/json.hpp>
#include <iostream>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <new>
using namespace dome;
namespace {
std::atomic_bool block_allocation=false,allocation_entered=false;
std::atomic_size_t blocked_size=0;
std::thread::id caller_thread;
std::mutex allocation_mutex;std::condition_variable allocation_changed;
bool released=false;
void observe(size_t size) {
  if(!block_allocation || size!=blocked_size || std::this_thread::get_id()==caller_thread || allocation_entered.exchange(true))return;
  std::unique_lock lock(allocation_mutex);allocation_changed.notify_all();allocation_changed.wait(lock,[]{return released;});
}
struct WorkerBarrier {
  explicit WorkerBarrier(size_t bytes){std::lock_guard lock(allocation_mutex);released=false;allocation_entered=false;blocked_size=bytes;caller_thread=std::this_thread::get_id();block_allocation=true;}
  void wait(){std::unique_lock lock(allocation_mutex);if(!allocation_changed.wait_for(lock,std::chrono::seconds(30),[]{return allocation_entered.load();}))throw std::runtime_error("Skin worker did not reach geometry copy");}
  void release(){std::lock_guard lock(allocation_mutex);block_allocation=false;released=true;allocation_changed.notify_all();}
  ~WorkerBarrier(){release();}
};
}
[[gnu::noinline]] void *operator new(size_t n){observe(n);if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,size_t)noexcept{std::free(p);}
namespace {
void check(bool condition,const char *message) {
  if(!condition) throw std::runtime_error(message); std::cout<<"PASS "<<message<<'\n';
}
template<class F> bool rejects(F f) {try{f();return false;}catch(...){return true;}}
Vec3 point(const CanvasTransform &t,Vec3 p) {
  auto q=ob::transform(t,{p.x,p.y,p.z});return {q.x,q.y,q.z};
}
Scene fixture() {
  Mesh m;m.positions={{-.1f,0,0},{.1f,0,0},{0,.5f,0}};
  m.normals.assign(3,{0,0,1});m.indices={0,1,2};m.uv={{{0,0}},{{1,0}},{{.5f,1}}};
  Scene s;s.active().painted=std::make_shared<PaintedMesh>(m,64);s.active().mesh_active=true;
  DeformationState d(m);d.skeleton=Skeleton{{{"root",-1,{},.3f},{"tip",0,{{0,.3f,0}},.2f}}};
  d.skin=SkinWeights{{{{0,.8f},{1,.2f}},{{0,.4f},{1,.6f}},{{1,1}}}};
  d.pose={d.skeleton->bones[0].rest,d.skeleton->bones[1].rest};d.pose[0].translation.z=.2f;
  d.pose[1].rotation={.92387953f,0,0,.38268343f};s.set_deformation(1,d);
  s.duplicate(1);s.object(2).deformation.reset();s.select(2);
  return s;
}
}
int main(int argc,char **argv) try {
  if(argc==3 && std::string_view(argv[1])=="--ui-fixture") {fixture().save(argv[2]);return 0;}
  if(argc==3 && std::string_view(argv[1])=="--ui-unbound-fixture") {
    auto scene=fixture();scene.select(1);auto d=*scene.active().deformation;d.skin.reset();
    const auto &rest=scene.active().painted->mesh();d.morphs=MorphSet(rest);auto positions=rest.positions;positions[0].x+=.01f;
    d.morphs->set_target(rest,"Fit detail",positions);d.morph_weights={.25f};scene.set_deformation(1,d);scene.save(argv[2]);return 0;
  }
  if(argc==3 && std::string_view(argv[1])=="--inspect") {
    auto scene=Scene::load(argv[2]);const auto &d=scene.active().deformation;
    std::cout<<nlohmann::json{{"skinned",bool(d&&d->skin)}, {"scene",scene.encode()}}.dump();return 0;
  }
  auto scene=fixture();scene.object(1).transform={{1,2,3},{0,.70710678f,0,.70710678f},2};
  scene.object(2).transform={{-.5f,1,2},{0,0,.38268343f,.92387953f},.5f};
  auto source_world=scene.world_transform(1),target_world=scene.world_transform(2);
  auto transform=ob::compose(ob::inverse(target_world),source_world);
  auto target_mesh=scene.object(2).painted->mesh();
  for(auto &p:target_mesh.positions)p=point(transform,p);
  scene.object(2).painted=std::make_shared<PaintedMesh>(target_mesh,64);
  DeformationState morphs(target_mesh);morphs.morphs=MorphSet(target_mesh);
  auto changed=target_mesh.positions;changed[0].z+=.02f;
  morphs.morphs->set_target(target_mesh,"Test",changed);morphs.morph_weights={.25f};
  scene.set_deformation(2,morphs);
  auto paint=scene.object(2).painted;auto original=scene.encode();
  SkinAuthoringOptions options;options.reference=1;options.mode=SkinAuthoringMode::Transfer;
  options.transfer_distance=.01f;author_skin(scene,2,options);
  check(scene.object(2).painted==paint && scene.object(2).deformation->morphs->encode()==morphs.morphs->encode() &&
        scene.object(2).deformation->morph_weights==morphs.morph_weights,
        "Cross-object weight transfer preserves target paint, morph deltas and active morph values");
  check(scene.encode()["objects"][0]==original["objects"][0],
        "A rotated scaled transfer leaves the complete source rig and object immutable");
  auto transferred=scene.object(2).deformation;
  const auto &source=*scene.object(1).deformation,&target=*transferred;
  for(size_t vertex=0;vertex<3;++vertex) for(auto influence:source.skin->vertices[vertex]) {
    float actual=0;for(auto i:target.skin->vertices[vertex])if(i.joint==influence.joint)actual=i.weight;
    if(std::abs(actual-influence.weight)>1e-5f)throw std::runtime_error("World-space barycentric transfer changed an exact source vertex weight");
  }
  check(true,"Independent barycentric expectations survive separate rotated and scaled object frames");
  auto source_mesh=scene.object(1).painted->mesh();
  auto source_posed=deform_mesh(source_mesh,*source.skin,*source.skeleton,source.pose);
  auto target_posed=deform_mesh(target_mesh,*target.skin,*target.skeleton,target.pose);
  for(size_t v=0;v<3;++v)if(length(point(source_world,source_posed.positions[v])-point(target_world,target_posed.positions[v]))>1e-4f)
    throw std::runtime_error("Transferred root/rest/pose frame does not commute with world placement");
  check(true,"Transferred rest rig and current pose deform into the same independently evaluated world positions");
  auto before=scene.encode();options.reference=2;options.mode=SkinAuthoringMode::Automatic;
  options.seed_distance=2;options.locks={1,0};author_skin(scene,2,options);
  for(size_t v=0;v<3;++v) {
    float old=0,now=0;for(auto i:target.skin->vertices[v])if(i.joint==0)old=i.weight;
    for(auto i:scene.object(2).deformation->skin->vertices[v])if(i.joint==0)now=i.weight;
    if(old!=now)throw std::runtime_error("Rebinding modified a hard-locked influence");
  }
  check(true,"Automatic rebinding preserves exact hard-locked influence values");
  auto intact=scene.encode();check(rejects([&]{author_skin(scene,2,options,[]{return true;});}) && scene.encode()==intact,
        "Cancelled binding publishes neither a partial rig nor partial skin weights");
  options.mode=SkinAuthoringMode::Transfer;
  check(rejects([&]{author_skin(scene,2,options);}) && scene.encode()==intact,
        "Self transfer is an explicit transactional error");
  options.reference=1;
  auto invalid=scene.snapshot();auto other=*invalid.object(2).deformation;
  other.skeleton->bones[0].name="Different identity";invalid.set_deformation(2,other);auto prior=invalid.encode();
  check(rejects([&]{author_skin(invalid,2,options);}) && invalid.encode()==prior,
        "Different existing joint identities cannot be silently replaced or guessed by name");
  auto distant=fixture();distant.object(2).transform.position.x=10;auto saved=distant.encode();
  options.locks.clear();options.transfer_distance=.01f;
  check(rejects([&]{author_skin(distant,2,options);}) && distant.encode()==saved,
        "Unreachable transfer vertices fail without changing the target's prior authoring data");
  {
    auto hidden=fixture();hidden.object(1).visible=false;hidden.object(1).locked=true;
    auto old=hidden.encode()["objects"][0];SkinAuthoringOptions o;o.reference=1;o.mode=SkinAuthoringMode::Transfer;
    check(author_skin(hidden,2,o)&&hidden.object(2).deformation->skin&&hidden.encode()["objects"][0]==old,
          "Hidden locked reference rigs can supply weights without being modified");
    for(float distance:{0.f,.0001f,.01f,.3f,10.f,10000.f})
      if(std::abs(skin_distance_metres(skin_distance_slider(distance))-distance)>std::max(1e-7f,distance*2e-6f))
        throw std::runtime_error("World distance slider failed its independent unit round trip");
    check(skin_distance_metres(0)==0&&skin_distance_metres(1)==10000&&rejects([]{skin_distance_metres(-.1f);}),
          "World distance controls preserve exact zero, finite units and bounded endpoint values");
  }
  {
    auto same=fixture();auto d=*same.object(1).deformation;d.skin->vertices[0]={{0,1}};same.set_deformation(2,d);
    same.object(1).transform.orientation={0,.70710678f,0,.70710678f};
    same.object(2).transform.orientation={0,-.70710678f,0,-.70710678f};
    auto before=same.object(2).deformation->skeleton->encode();SkinAuthoringOptions o;o.reference=1;o.mode=SkinAuthoringMode::Transfer;
    check(author_skin(same,2,o)&&same.object(2).deformation->skeleton->encode()==before,
          "Equivalent opposite quaternion signs preserve exact native rest rigs during transfer");
  }
  {
    auto fitted=fixture();fitted.select(1);auto rig=*fitted.active().deformation;rig.skin.reset();
    const auto &rest=fitted.active().painted->mesh();rig.morphs=MorphSet(rest);auto edited=rest.positions;edited[0].x+=.01f;
    rig.morphs->set_target(rest,"Fit detail",edited);rig.morph_weights={.25f};
    fitted.set_deformation(1,rig);auto prior=fitted.encode();auto bones=rig.skeleton->encode();
    Document doc;doc.replace(fitted);SkinAuthoringOptions o;o.reference=1;o.mode=SkinAuthoringMode::Automatic;
    check(doc.frame().state->can_bind_skin&&!doc.frame().state->can_paint_weights,
          "A fitted skeleton without prior weights exposes automatic binding");
    doc.bind_skin(o);check(doc.wait_asset()&&doc.frame().state->error.empty()&&doc.copy_scene().active().deformation->skin&&
      doc.copy_scene().active().deformation->skeleton->encode()==bones&&
      doc.copy_scene().active().deformation->morphs->encode()==rig.morphs->encode()&&
      doc.copy_scene().active().deformation->pose[0].translation.z==rig.pose[0].translation.z,
      "The actual worker binds a fitted skeleton while retaining its exact rest hierarchy");
    check(doc.undo()&&doc.copy_scene().encode()==prior,"Undo restores the exact fitted but unbound authoring state");
  }
  {
    Scene avatar;append_humanoid(avatar,humanoid_baseline_defaults("vroid-beta-female"),"CC0",{},64);
    auto data=avatar.active().deformation;auto appearance=avatar.active().appearance;auto paint=avatar.active().painted;
    SkinAuthoringOptions o;o.reference=avatar.active().id;o.locks.assign(data->skeleton->bones.size(),1);o.limits.max_influences=32;
    check(!author_skin(avatar,o.reference,o)&&avatar.active().deformation==data&&avatar.active().appearance==appearance&&avatar.active().painted==paint,
          "Closed hard locks retain every exact original CC0 avatar rig, morph, paint and source identity without recomputation");
  }
  {
    auto saturated=fixture();auto d=*saturated.object(1).deformation;d.skin->vertices.assign(3,{{0,1}});saturated.set_deformation(2,d);
    auto old=saturated.active().deformation;SkinAuthoringOptions o;o.reference=2;o.locks={1,0};o.maximum_work=1;
    check(!author_skin(saturated,2,o)&&saturated.active().deformation==old,
          "A saturated locked influence leaves no editable mass and skips unnecessary binding work exactly");
  }
  {
    auto native=fixture();auto original_native=native.encode();Document document;document.replace(native);
    check(document.frame().state->can_configure_skin&&document.frame().state->can_bind_skin,
          "An unskinned prepared target exposes the native bind workflow");
    SkinAuthoringOptions operation;operation.reference=1;operation.mode=SkinAuthoringMode::Transfer;
    document.bind_skin(operation,document.draft_scene().source);
    check(document.wait_asset()&&document.frame().state->error.empty()&&document.copy_scene().active().deformation->skin,
          "Explicit native bind request completes on the guarded asset worker");
    auto bound=document.copy_scene();auto bound_bytes=bound.encode();
    check(document.undo()&&document.copy_scene().encode()==original_native&&document.redo()&&document.copy_scene().encode()==bound_bytes,
          "The complete skin authoring operation has one exact global Undo and Redo");
    Document unchanged;unchanged.replace(bound);operation.mode=SkinAuthoringMode::Automatic;operation.reference=2;operation.locks={1,1};
    unchanged.bind_skin(operation);check(unchanged.wait_asset()&&unchanged.frame().state->error.empty()&&
      !unchanged.frame().state->dirty&&!unchanged.frame().state->can_undo&&unchanged.copy_scene().encode()==bound_bytes,
      "Fully locked exact rebinding creates neither dirty state nor a phantom history step");
    auto lease=document.draft_scene().source;auto active=document.copy_scene().active();
    document.object_properties(active.id,"New name",active.visible,active.locked,active.transform,active.material);
    auto current=document.copy_scene().encode();
    check(rejects([&]{document.bind_skin(operation,lease);})&&document.copy_scene().encode()==current&&!document.frame().state->asset_busy,
          "A stale native dialog cannot enqueue work against a changed scene");
    auto large=native.snapshot();Mesh grid;
    for(uint32_t y=0;y<=100;++y)for(uint32_t x=0;x<=100;++x){grid.positions.push_back({-.01f+.02f*x/100,.1f+.1f*y/100,0});grid.normals.push_back({0,0,1});grid.uv.push_back({.1f+.8f*x/100,.1f+.8f*y/100});}
    for(uint32_t y=0;y<100;++y)for(uint32_t x=0;x<100;++x){uint32_t a=y*101+x;grid.indices.insert(grid.indices.end(),{a,a+1,a+102,a,a+102,a+101});}
    large.active().painted=std::make_shared<PaintedMesh>(grid,64);auto intact=large.encode();
    Document cancel;cancel.replace(large);check(cancel.wait_mesh(),"Cancellation fixture finishes its prior immutable preview before binding");
    operation.mode=SkinAuthoringMode::Transfer;operation.reference=1;operation.locks.clear();
    WorkerBarrier gate(grid.positions.size()*sizeof(Vec3));cancel.bind_skin(operation);gate.wait();
    check(cancel.asset_status().cancellable,"A deterministically blocked skin worker exposes native cancellation");
    cancel.cancel_asset_work();gate.release();
    check(cancel.wait_asset()&&cancel.frame().state->error.empty()&&!cancel.frame().state->dirty&&cancel.copy_scene().encode()==intact,
          "Cancelling the executing skin worker retains the complete original unskinned project");
  }
  {
    auto stress=fixture();auto m=stress.object(1).painted->mesh();
    for(int i=0;i<1024;++i)m.indices.insert(m.indices.end(),{0,1,2});
    SkinWeights w=*stress.object(1).deformation->skin;
    auto prior=stress.encode();SkinAuthoringOptions o;o.reference=1;o.mode=SkinAuthoringMode::Transfer;o.maximum_work=8;
    // Direct bounded kernel sees thousands of coincident triangles, exercising
    // BVH construction/query accounting without editing any source buffers.
    check(rejects([&]{transfer_weights(m,w,stress.active().painted->mesh(),2,.1f,{}, {},8);}),
          "Coincident transfer geometry has a finite explicitly enforced work budget");
    size_t baseline_callbacks=0;
    transfer_weights(m,w,stress.active().painted->mesh(),2,.1f,{},[&]{++baseline_callbacks;return false;},50000000);
    auto many=stress.active().painted->mesh();for(int i=0;i<64;++i){many.positions.push_back(many.positions[0]);many.normals.push_back(many.normals[0]);many.uv.push_back(many.uv[0]);}
    size_t callbacks=0;check(rejects([&]{transfer_weights(m,w,many,2,.1f,{},[&]{return ++callbacks>baseline_callbacks+5;},50000000);})&&callbacks>baseline_callbacks,
          "Cancellation is sampled inside stressed BVH queries after construction completes");
    check(rejects([&]{author_skin(stress,2,o);})&&stress.encode()==prior,
          "Exhausted helper transfer budget publishes no partial native rig or skin");
  }
  struct Driver {
    Document &doc;SpatialFrame frame;
    explicit Driver(Document &d):doc(d){frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};frame.hands[1].tracked.valid=true;send();}
    void send(float x=0,float y=0,bool click=false,float force=0){++frame.sequence;frame.time+=.03;auto &h=frame.hands[1];h.x=x;h.y=y;h.joystick=click;h.tracked.trigger=force;doc.submit(frame);if(!doc.wait_input())throw std::runtime_error("Skin controller input timeout");}
    void sector(int n){float a=n*std::numbers::pi_v<float>/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
    void open(){send();send(0,0,true);send();sector(8);sector(10);sector(5);sector(10);sector(11);}
    void value(float t,int component=0){auto state=doc.frame().state;auto &h=frame.hands[1];h.tracked.aimValid=true;
      float y=state->controls.components==1?300:240+component*80;
      h.tracked.aim.position=ob::transform(state->panel,{((100+420*t)/640.f-.5f)*.6f,(.5f-y/640.f)*.6f,.25f});
      h.tracked.aim.orientation=state->panel.orientation;send(0,0,false,.8f);send();}
    void confirm(){send(0,0,true);send();}
  };
  {
    auto scene=fixture();Document xr;xr.replace(scene);Driver driver(xr);driver.open();
    check(xr.frame().state->menu_page==14,"Joystick reaches bind and transfer controls for an unskinned target");
    auto before=xr.copy_scene().encode();driver.sector(1);driver.value(1);driver.confirm();
    check(xr.frame().state->skin_options.mode==SkinAuthoringMode::Transfer&&xr.copy_scene().encode()==before&&!xr.frame().state->dirty,
          "Controller method selection is staged configuration with no native data or history change");
    driver.sector(2);driver.value(0,1);driver.confirm();
    check(xr.frame().state->skin_options.seed_margin==0,"Controller logarithmic distance panel supports an exact zero margin");
    driver.sector(4);driver.value(1.f/31);driver.confirm();
    check(xr.frame().state->skin_options.limits.max_influences==2,"Controller influence slider commits an integer cap");
    driver.sector(7);check(xr.wait_asset()&&xr.frame().state->error.empty()&&xr.copy_scene().active().deformation->skin,
          "Actual joystick Bind command performs persistent worker skin transfer");
    auto bound=xr.copy_scene().encode();
    check(xr.undo()&&xr.copy_scene().encode()==before&&xr.redo()&&xr.copy_scene().encode()==bound,
          "Controller-authored skin transfer shares exact complete native history");
    auto &focus=driver;focus.open();focus.sector(2);check(xr.frame().state->controls.parameter==VrParameter::SkinDistances,"Focus cancellation fixture reaches the actual skin distance panel");auto confirmed=xr.frame().state->skin_options.seed_distance;focus.value(.9f);
    focus.frame.focused=false;focus.send();
    check(xr.frame().state->controls.parameter==VrParameter::None&&xr.frame().state->skin_options.seed_distance==confirmed&&xr.copy_scene().encode()==bound,
          "Focus loss cancels staged skin distances without changing accepted skin or configuration");
  }
  return 0;
} catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
