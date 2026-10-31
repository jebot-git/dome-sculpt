// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/material-visibility.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstdlib>
#include <new>
thread_local int64_t allocation_budget=-1;
void *operator new(std::size_t size){if(allocation_budget==0)throw std::bad_alloc();if(allocation_budget>0)--allocation_budget;if(auto p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void *operator new[](std::size_t n){return ::operator new(n);}
void *operator new(std::size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void *operator new[](std::size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
void operator delete(void*p,const std::nothrow_t&)noexcept{std::free(p);}void operator delete[](void*p,const std::nothrow_t&)noexcept{std::free(p);}
using namespace dome;
namespace {
int checks=0;
void check(bool ok,const char*label){if(!ok)throw std::runtime_error(label);++checks;std::cout<<"PASS "<<label<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
Mesh face(){Mesh m;m.positions={{-.075f,-.08f,0},{.075f,-.08f,0},{.075f,.08f,0},{-.075f,.08f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
Scene fixture(){
 Scene s;auto a=s.active().id;auto top=s.add_group("Avatar");auto child=s.add_group("Parts",{},top);
 auto make=[&](uint64_t id,float x,std::array<float,4> color){auto &o=s.object(id);o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(face(),64);o.transform.position.x=x;o.material.color=color;o.material.unlit=true;};
 make(a,-.16f,{1,.1f,.1f,1});s.active().group=top;s.active().name="Body";
 auto b=s.add("Hair",Volume{});make(b,.16f,{.1f,1,.1f,1});s.object(b).group=child;
 auto c=s.add("Reference",Volume{});make(c,.52f,{.1f,.1f,1,1});s.object(c).locked=true;
 auto hidden=s.add("Hidden source",Volume{});s.object(hidden).group=child;s.object(hidden).visible=false;
 for(int i=0;i<12;++i)s.add("Extra "+std::to_string(i),Volume{});
 s.select(a);return s;
}
const ObjectInfo &info(const Document &d,uint64_t id){static ObjectInfo copy;auto state=d.frame().state;copy=*std::find_if(state->objects.begin(),state->objects.end(),[&](const auto&o){return o.id==id;});return copy;}
struct Controller {
 Document &d;SpatialFrame f;
 Controller(Document &doc):d(doc){f.focused=f.head_valid=true;f.head.position={0,1.6f,0};for(auto &h:f.hands){h.tracked.valid=h.tracked.aimValid=true;h.tracked.grip.position={-.8f,1.4f,-.4f};h.tracked.aim=h.tracked.grip;}send();}
 void send(){++f.sequence;f.time+=.04;d.submit(f);if(!d.wait_input())throw std::runtime_error("Controller input timeout");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
 void point(size_t hand,Vec3 local,bool near){auto canvas=d.frame().state->canvas;auto p=ob::transform(canvas,{local.x,local.y,local.z});auto &h=f.hands[hand].tracked;h.grip={p+ob::rotate(canvas.orientation,{0,0,near?.02f:.6f}),canvas.orientation,1};h.aim=h.grip;}
 void click(size_t hand,bool additive=false){f.hands[hand].tracked.squeeze=additive?1:0;f.hands[hand].tracked.trigger=0;send();f.hands[hand].tracked.trigger=1;send();f.hands[hand].tracked.trigger=0;send();f.hands[hand].tracked.squeeze=0;}
 void panel(float u,float v,size_t hand=1){auto p=d.frame().state->hierarchy_panel;f.hands[hand].tracked.grip={ob::transform(p,{(u-.5f)*.6f,(.5f-v)*.6f,.4f}),p.orientation,1};f.hands[hand].tracked.aim=f.hands[hand].tracked.grip;}
};
}
int main(int argc,char **argv)try{
 if(argc==3&&std::string(argv[1])=="--fixture"){fixture().save(argv[2]);return 0;}
 if(argc==3&&std::string(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 {Scene deep;uint64_t parent=0;for(int i=0;i<32;++i)parent=deep.add_group("Nested "+std::to_string(i),{},parent);
 deep.active().group=parent;std::vector<uint64_t> ids{deep.active().id};for(int i=1;i<128;++i){auto id=deep.add("Part "+std::to_string(i),Volume{});deep.object(id).group=parent;ids.push_back(id);}
 auto rows=object_hierarchy(deep,ids,{});check(rows.size()==160&&rows.front().selected&&rows.back().depth==32,"Deep hierarchy computes all collection memberships with bounded ancestry traversal");
 uint64_t root=deep.groups().front().id;rows=object_hierarchy(deep,ids,std::span<const uint64_t>(&root,1));check(rows.size()==1&&rows.front().selected,"Collapsed deep hierarchy retains full descendant selection");}
 Document d;d.replace(fixture());check(d.wait_mesh(),"Complete initial immutable scene prepared");
 auto original=d.copy_scene();auto a=original.objects()[0].id,b=original.objects()[1].id,c=original.objects()[2].id,hidden=original.objects()[3].id;
 check(!d.frame().state->selection_mode&&!d.frame().surface->picking,"Ordinary editing allocates no selection BVH");
 d.configure_selection(true);check(d.wait_mesh()&&d.frame().surface->picking,"First-page selection toggle prepares worker BVH");auto geometry=d.frame().surface;
 allocation_budget=0;auto empty_touch=geometry->picking->nearby({99,99,99},.04f);allocation_budget=-1;
 check(!empty_touch,"Empty-space proximity selection completes without any heap allocation or exception");
 check(d.frame().state->selected_objects==std::vector<uint64_t>{a}&&info(d,a).active&&info(d,b).ghost,"Entering selection clearly marks active object and ghosts every unselected visible object");
 Controller input(d);d.show_object_hierarchy(false);auto canvas=d.frame().state->canvas;
 input.point(0,{-.16f,0,0},true);input.click(0);
 check(d.frame().state->active_object==a&&d.frame().state->selection_hover[0]==a,"Left controller directly touches and selects a surface");
 input.point(1,{.16f,0,0},false);input.click(1,true);
 check(d.frame().state->selected_objects==std::vector<uint64_t>({a,b})&&info(d,b).active&&info(d,a).selected&&!info(d,a).ghost&&!info(d,b).ghost&&info(d,c).ghost,"Grip+trigger selects a ghost by ray, extends membership and makes the last added object active");
 check(d.frame().state->canvas.position.x==canvas.position.x&&d.frame().surface==geometry&&!d.frame().state->dirty,"Selection and grip modifier move no canvas, mesh, material, paint or history");
 input.f.hands[1].tracked.trigger=1;input.f.hands[1].tracked.squeeze=1;input.send();auto held=d.frame().state->selected_objects;input.send();input.send();
 check(d.frame().state->selected_objects==held,"Held trigger toggles once rather than repeatedly changing membership");input.f.hands[1].tracked.trigger=0;input.send();
 input.click(1,true);check(d.frame().state->selected_objects==std::vector<uint64_t>({a,b}),"A new trigger edge can add a previously removed member");
 input.point(1,{.52f,0,0},false);input.click(1);
 check(d.frame().state->active_object==c&&info(d,c).selected&&!info(d,c).editable,"Locked objects remain inspectable and selected without gaining editing permission");
 input.point(1,{2,0,0},false);input.click(1);
 check(d.frame().state->selected_objects.empty()&&info(d,a).ghost&&!d.object_editable(c),"Trigger on empty space clears membership and grants no accidental active-object edits");
 auto exact_objects=original.encode()["objects"];
 check(d.copy_scene().encode()["objects"]==exact_objects&&!d.frame().state->dirty,"All controller selection changes preserve every original object byte");
 d.show_object_hierarchy(true);input.f.hands[1].tracked.trigger=0;input.send();auto rows=d.frame().state->hierarchy;
 check(rows.size()==18&&rows[0].group&&rows[1].group&&rows[1].depth==1,"XR hierarchy retains actual nested group structure and all object rows");
 input.panel(.4f,object_outliner::first+.5f*object_outliner::row_height);input.click(1);
 check(d.frame().state->selected_objects.size()==3&&info(d,a).selected&&info(d,b).selected&&info(d,hidden).selected,"Controller selects an entire collection including collapsed/hidden descendants");
 auto selected=d.frame().state->selected_objects;input.panel(.07f,object_outliner::first+.5f*object_outliner::row_height);input.click(1);
 check(d.frame().state->hierarchy.size()==14&&d.frame().state->selected_objects==selected&&d.frame().surface==geometry,"Disclosure arrow collapses descendants without changing selection or geometry");
 input.panel(.75f,.88f);input.click(1);check(d.frame().state->hierarchy_offset==9,"Controller hierarchy paging reaches rows beyond the first nine");
 input.panel(.25f,.88f);input.click(1);check(d.frame().state->hierarchy_offset==0,"Controller hierarchy paging returns to the first page");
 input.panel(.07f,object_outliner::first+.5f*object_outliner::row_height);input.click(1);
 rows=d.frame().state->hierarchy;auto row=std::find_if(rows.begin(),rows.end(),[&](auto&r){return !r.group&&r.id==b;})-rows.begin();
 input.panel(.94f,object_outliner::first+(float(row)+.5f)*object_outliner::row_height);input.click(1);
 check(!d.copy_scene().object(b).visible&&d.frame().state->can_undo,"Outliner visibility is a real undoable scene edit");check(d.undo()&&d.copy_scene().object(b).visible,"Visibility undo restores the actual object");d.wait_mesh();
 d.show_object_hierarchy(false);input.point(0,{-.16f,0,0},true);input.click(0);
 auto before_focus=d.frame().state->selected_objects;
 input.f.focused=false;input.send();input.f.focused=true;input.f.hands[1].tracked.trigger=1;input.point(1,{.16f,0,0},true);input.send();input.send();
 check(d.frame().state->selected_objects==before_focus,"Regaining focus with a held trigger cannot select until release");input.f.hands[1].tracked.trigger=0;input.send();input.click(1);
 check(d.frame().state->active_object==b,"Released trigger rearms normal direct selection after focus recovery");
 auto source=d.draft_scene().source;uint64_t target=a;d.select_objects(std::span<const uint64_t>(&target,1));
 check(rejects([&]{d.select_objects(std::span<const uint64_t>(&target,1),ObjectSelection::Replace,source);}),"Stale selection authority cannot act through an old hierarchy lease");
 input.f.hands[0].tracked.trigger=1;input.send();d.configure_selection(false);input.send();input.send();
 check(!d.frame().state->stroking&&!d.frame().state->native_pending,"Switching from selection to editing with held trigger cannot start a brush");input.f.hands[0].tracked.trigger=0;input.send();
 check(info(d,b).ghost&&info(d,a).selected,"Leaving selection mode retains visible focused-selection cues");d.focus_selection(false);
 check(!info(d,b).ghost&&d.object_editable(b),"All-solid toggle restores ordinary object editing authority");
 input.send(); // Rearm after the explicit All-solid authority transition.
 // Actual joystick pipeline: the first menu page directly exposes Selection.
 input.f.hands[0].joystick=true;input.send();input.f.hands[0].joystick=false;input.f.hands[0].x=.8660254f;input.f.hands[0].y=-.5f;input.send();input.send();
 check(d.frame().state->menu&&d.frame().state->menu_page==0&&d.frame().state->menu_index==11,"Selection toggle is on the first controller menu page");
 input.f.hands[0].joystick=true;input.send();input.f.hands[0].joystick=false;input.send();
 check(d.frame().state->selection_mode&&!d.frame().state->menu,"First-page joystick confirmation enters selection without submenu navigation");
 d.show_object_hierarchy(false);input.send();
 input.f.hands[0].joystick=true;input.send();input.f.hands[0].joystick=false;input.send();
 check(d.frame().state->menu&&d.frame().state->menu_page==0,"Controller can reopen the first menu page while selection consumes trigger input");
 d.cancel_spatial_menu();input.send();
 d.show_object_hierarchy(true);input.send();input.panel(.2f,.06f);input.click(1);
 check(!d.frame().state->selection_mode,"Direct outliner header toggles selection off without navigating a menu");
 auto paint_before=d.copy_scene().encode()["objects"];
 rows=d.frame().state->hierarchy;row=std::find_if(rows.begin(),rows.end(),[&](auto&r){return !r.group&&r.id==b;})-rows.begin();
 input.panel(.4f,object_outliner::first+(float(row)+.5f)*object_outliner::row_height);
 input.f.hands[1].tracked.trigger=1;input.send();input.point(1,{.16f,0,0},true);input.send();input.send();
 check(!d.frame().state->stroking&&!d.frame().state->native_pending&&d.copy_scene().encode()["objects"]==paint_before,"An outliner trigger remains captured until release even after the controller leaves the panel");
 input.f.hands[1].tracked.trigger=0;input.send();
 input.panel(.2f,.06f);input.click(1);check(d.frame().state->selection_mode,"Direct outliner header toggles selection back on");
 d.show_object_hierarchy(false);input.send();
 d.select_object(a);input.send();
 auto sector=[&](int n){float angle=n*3.14159265358979323846f/6;input.f.hands[0].joystick=false;input.f.hands[0].x=std::cos(angle);input.f.hands[0].y=std::sin(angle);input.send();input.f.hands[0].joystick=true;input.send();input.f.hands[0].joystick=false;input.send();};
 input.f.hands[0].joystick=true;input.send();input.f.hands[0].joystick=false;input.send();sector(8);sector(3);
 check(d.frame().state->active_object==b&&d.frame().state->selected_objects==std::vector<uint64_t>{b}&&info(d,b).editable,"Existing Next-object controller command stays synchronized with focused selection");
 d.configure_selection(false);d.configure_object_grab(true,true);d.hold_object(a);d.show_object_hierarchy(true);input.send();
 rows=d.frame().state->hierarchy;row=std::find_if(rows.begin(),rows.end(),[&](auto&r){return !r.group&&r.id==a;})-rows.begin();
 input.panel(.94f,object_outliner::first+(float(row)+.5f)*object_outliner::row_height);input.click(1);
 check(!d.frame().state->held_object&&!d.copy_scene().object(a).visible,"Hiding a held object through the hierarchy releases its exclusive authority");
 check(d.undo()&&d.copy_scene().object(a).visible,"Held-object visibility still restores through normal undo");d.wait_mesh();d.configure_object_grab(false,false);d.configure_selection(true);d.show_object_hierarchy(false);input.send();
 size_t failed=0;
 size_t successes=0;
 for(int limit=0;limit<2048&&successes<8;++limit){
  auto before=d.frame().state;auto exact=d.copy_scene().encode();uint64_t next=before->active_object==a?b:a;bool threw=false;allocation_budget=limit;
  try{d.select_objects(std::span<const uint64_t>(&next,1));}catch(const std::bad_alloc&){threw=true;}allocation_budget=-1;
  if(!threw)++successes;else successes=0;
  if(threw){++failed;check(d.frame().state==before&&d.copy_scene().encode()==exact,"Allocation denial restores exact selection publication, authority and committed scene");}
 }
 check(failed>10&&successes==8,"Allocation test crosses all single-failure staging/publication prefixes and reaches successful selection");
 std::cout<<"Selection allocation failure prefixes "<<failed<<'\n';
 // A transparent nearer triangle cannot intercept touch selection of the
 // visible surface underneath, even from slightly inside that surface.
 {auto layered=fixture();layered.object(b).transform=layered.object(a).transform;layered.object(b).transform.position.z=.015f;layered.object(b).material.alpha=Material::AlphaMode::Mask;layered.object(b).material.color[3]=0;
 Document touch;touch.replace(layered);touch.configure_selection(true);touch.show_object_hierarchy(false);check(touch.wait_mesh(),"Layered transparency selection BVH prepared");Controller hand(touch);hand.point(0,{-.16f,0,0},true);hand.f.hands[0].tracked.aimValid=false;hand.click(0);
 check(touch.frame().state->selection_hover[0]==a,"Direct touch ignores transparent closer fragments and selects visible underlying geometry");
 hand.f.hands[0].tracked.grip.position.z-=.04f;hand.click(0);check(touch.frame().state->selection_hover[0]==a,"A controller slightly inside a one-sided surface can still select it by touch");}
 d.replace(fixture());check(!d.frame().state->selection_mode&&!d.frame().state->selection_focus&&d.frame().state->selected_objects.empty(),"Replacement clears stale selection identities and presentation overrides");
 std::cout<<"Object selection checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){allocation_budget=-1;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
