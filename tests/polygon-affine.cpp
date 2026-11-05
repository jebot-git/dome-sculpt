// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-fixture.hpp"
#include "polygon-conversion.hpp"
#include "polygon-view-state.hpp"
#include "desktop-framing.hpp"
#include "import-controller-driver.hpp"
#include <map>
#include "dome/source-placement.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <unistd.h>
using namespace dome;
namespace {
thread_local bool armed=false;thread_local size_t calls=0,deny=SIZE_MAX;
unsigned checks=0;
void check(bool b,const char *m){if(!b)throw std::runtime_error(m);++checks;std::cout<<"PASS "<<m<<'\n';}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
Vec3 point(const AffineTransform &w,Vec3 p){auto q=w.point({p.x,p.y,p.z});return {float(q[0]),float(q[1]),float(q[2])};}
Vec3 direction(const AffineTransform &w,Vec3 p){auto q=w.direction({p.x,p.y,p.z});return {float(q[0]),float(q[1]),float(q[2])};}
AffineTransform frame(bool reflect){auto m=AffineTransform{}.matrix();m[0]=reflect?-1.7:1.7;m[1]=.2;m[4]=.6;m[5]=.9;m[8]=.35;m[9]=.1;m[10]=.7;m[12]=.3;m[13]=-.2;m[14]=.4;return AffineTransform(m);}
Scene fixture(bool reflect){auto s=polygon_test::fixture();auto g=s.add_group("Actual affine parent");s.group(g).affine_edit=std::make_shared<const NativeAffineEdit>(frame(reflect));s.active().group=g;s.active().transform.position={.05f,-.03f,.02f};s.validate();return s;}
void ready(Document &d){check(d.wait_polygon(),"Ordered affine polygon worker completes");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
void near_world(AffineVector a,AffineVector b,double tolerance,const char *m){check(std::hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2])<=tolerance,m);}
void extrusion(const PolygonMesh &before,const PolygonMesh &after,const AffineTransform &world,float amount){
 std::map<PolygonId,Vec3> old;for(auto &v:before.vertices())old.emplace(v.id,v.position);
 auto n=world.normal({0,0,1});const auto length=std::hypot(n[0],n[1],n[2]);for(auto &x:n)x*=amount/length;
 size_t created=0;
 for(const auto &v:after.vertices())if(v.id>=before.next_id()){
  check(v.provenance.size()==1&&v.provenance[0].second==1,"Extrusion keeps genuine stable source vertex provenance");
  auto source=world.point({old.at(v.provenance[0].first).x,old.at(v.provenance[0].first).y,old.at(v.provenance[0].first).z});
  auto current=world.point({v.position.x,v.position.y,v.position.z});for(size_t i=0;i<3;++i)source[i]+=n[i];
  near_world(current,source,2e-6,"Extrusion endpoint matches independent inverse-transpose world normal and physical metres");++created;
 }
 check(created==4,"One selected quad creates four genuine new extruded vertices");
}
void kernel(bool reflect){
 const auto world=frame(reflect);auto cube=PolygonMesh::cube();PolygonWorkspace w(cube);
 check(w.select_ray_affine(world,point(world,{0,0,3}),direction(world,{0,0,-1}),20,.015f),"Affine ray selects an actual local quad without a similarity fit");
 check(w.view().selection->ids==std::vector<PolygonId>{cube.faces()[1].id},"Ray selection retains exact original face identity");
 w.apply_affine(world,PolygonOperation::Extrude,.02f);extrusion(cube,*w.view().topology,world,.02f);
 auto endpoint=w.view().topology;check(w.undo()&&w.view().topology->encode()==cube.encode(),"Affine operation has one exact local Undo");check(w.redo()&&w.view().topology==endpoint,"Affine Redo reuses exact immutable endpoint");
 std::vector<PolygonCornerRgba> samples;for(const auto &f:cube.faces())for(const auto &c:f.corners)samples.push_back({c.id,{float(c.id%5)/5,.2f,.7f,1}});
 auto colors=std::make_shared<const PolygonColorField>(PolygonColorField::from_corners(cube,samples));PolygonWorkspace colored(cube,colors,{});colored.select(cube.faces()[1].id);
 colored.apply_affine(world,PolygonOperation::Extrude,.02f);colored.view().colors->validate(*colored.view().topology);
 check(colored.view().colors->source_owner()==colors->source_owner(),"World-metric operation preserves immutable source CORNER color authority");
 auto before=colored.view().topology;PolygonLimits cancel;cancel.cancelled=[] {return true;};
 check(refuses([&]{colored.apply_affine(world,PolygonOperation::Inset,.01f,1,cancel);})&&colored.view().topology==before,"Affine operation Cancel preserves exact current topology/history owner");
 PolygonLimits quota;quota.maximum_vertices=before->vertices().size();
 check(refuses([&]{colored.apply_affine(world,PolygonOperation::Extrude,.01f,1,quota);})&&colored.view().topology==before,"Actual affine expansion quota refuses before workspace publication");
 auto diagonal=AffineTransform{}.matrix();diagonal[0]=4;diagonal[5]=1;diagonal[10]=.5;AffineTransform stretch(diagonal);PolygonWorkspace picker(cube);picker.mode(PolygonSelectionMode::Vertex);
 check(!picker.select_ray_affine(stretch,point(stretch,{.488f,.499f,3}),{0,0,-1},20,.015f),"World radius rejects a locally-near vertex beyond15mm after nonuniform stretch");
 check(picker.select_ray_affine(stretch,point(stretch,{.499f,.488f,3}),{0,0,-1},20,.015f),"Same15mm physical radius accepts the independently close world vertex");
}
void inset(bool reflect){
 auto grid=PolygonMesh::grid();const auto world=frame(reflect);PolygonWorkspace w(grid);w.select_all();w.apply_affine(world,PolygonOperation::Inset,.02f);
 std::vector<Vec3> boundary;for(const auto &c:grid.faces()[0].corners){const auto &v=*std::find_if(grid.vertices().begin(),grid.vertices().end(),[&](const auto &v){return v.id==c.vertex;});boundary.push_back(point(world,v.position));}
 size_t inner=0;
 for(const auto &v:w.view().topology->vertices())if(v.id>=grid.next_id()){
  auto p=point(world,v.position);std::vector<double> distances;
  for(size_t i=0;i<4;++i){auto a=boundary[i],b=boundary[(i+1)%4];auto e=b-a;const auto len=length(e);auto area=cross(p-a,e);distances.push_back(length(area)/len);}
  std::sort(distances.begin(),distances.end());check(std::abs(distances[0]-.02)<2e-6&&std::abs(distances[1]-.02)<2e-6,"Inset uses20mm perpendicular world distances on both incident sheared/reflected sides");++inner;
 }
 check(inner==4,"Affine inset creates exactly four inner vertices with original outer boundary retained");
 for(const auto &v:grid.vertices()){auto now=std::find_if(w.view().topology->vertices().begin(),w.view().topology->vertices().end(),[&](const auto &x){return x.id==v.id;});check(now!=w.view().topology->vertices().end()&&now->position.x==v.position.x&&now->position.y==v.position.y&&now->position.z==v.position.z,"Untouched original vertex positions remain bit exact after world-metric inset");}
}
void native(bool reflect,const std::filesystem::path &out){
 auto source=fixture(reflect);const auto original=source.encode();const auto world=source.world_matrix(source.active().id);
 Document d;d.replace(source);check(d.wait_mesh(),"Actual native source publishes affine geometry");d.begin_polygon();ready(d);
 d.edit_polygon(PolygonAction::SelectAll);ready(d);check(d.frame().state->polygon->selection->ids.size()==6,"Native SelectAll works on genuine affine source without world_transform fallback");
 d.edit_polygon(PolygonAction::Clear);ready(d);
 auto preview=detail::polygon_preview(d.frame());check(preview.view&&preview.affine_world&&preview.affine_world->matrix()==world.matrix(),"Actual preview retains full affine placement instead of hiding workspace");
 check(bool(detail::desktop_framing(d.frame(),{},1,true)),"Desktop framing includes affine current polygon workspace");
 d.polygon_pointer(point(world,{0,0,3}),direction(world,{0,0,-1}),PolygonAction::Pick);ready(d);
 PolygonControls cfg;cfg.distance_mm=20;d.configure_polygon(cfg);d.edit_polygon(PolygonAction::Extrude);ready(d);
 extrusion(source.active().polygon->topology,*d.frame().state->polygon->topology,world,.02f);
 d.end_polygon(false);check(d.copy_scene().encode()==original&&!d.frame().state->can_undo,"Cancel after real affine topology work preserves complete original Scene/history");
 d.begin_polygon();ready(d);auto n=world.normal({0,0,1});const auto nl=std::hypot(n[0],n[1],n[2]);Vec3 normal{float(n[0]/nl),float(n[1]/nl),float(n[2]/nl)};
 const auto center=point(world,{0,0,.5f});auto along=normalized(direction(world,{1,0,0}))*.03f;
 d.polygon_pointer(center+normal*2,normal*-1,PolygonAction::BeginDrag);ready(d);const auto immutable=d.frame().state->polygon->surface;
 d.polygon_pointer(center+normal*2+along,normal*-1,PolygonAction::Drag);ready(d);
 check(d.frame().state->polygon->moving&&d.frame().state->polygon->surface==immutable,"Native affine drag changes only GPU displacement, retaining immutable geometry");
 auto delta=d.frame().state->polygon->move_delta;auto actual=world.direction({delta.x,delta.y,delta.z});near_world(actual,{along.x,along.y,along.z},2e-6,"Native drag delta maps through exact inverse affine frame in physical world metres");
 d.polygon_pointer(center+normal*2+along,normal*-1,PolygonAction::EndDrag);ready(d);d.end_polygon(true);
 auto path=out/(reflect?"reflected.dsculpt":"sheared.dsculpt");d.save_async(path).get();ready(d);const auto accepted=d.copy_scene().encode();
 check(Scene::load(path).encode()==accepted,"Apply plus immediate queued Save restarts exact affine polygon/native owners");
 check(d.copy_scene().active().affine_edit==source.active().affine_edit&&d.copy_scene().active().group==source.active().group,"Adoption retains actual object/group affine authority");
 check(d.undo()&&d.copy_scene().encode()==original,"One global Undo restores full paint/sculpt/source/placement after affine drag Apply");check(d.redo()&&d.copy_scene().encode()==accepted,"Global Redo restores exact accepted affine endpoint");
 d.restore_polygon_source();check(d.wait_asset()&&d.frame().state->error.empty()&&d.copy_scene().encode()==original,"Original Scene placement capsule restores exact source without TRS approximation");
 // Capturing a simultaneous held world gesture must preserve its real native
 // affine edit together with topology, not just a nominal Canvas pose.
 d.configure_object_grab(true,true);d.hold_object(source.active().id);d.begin_polygon();ready(d);
 check(d.begin_object_grab(source.active().id),"Affine polygon source supports real native held placement");
 CanvasTransform motion;motion.position={.12f,-.04f,.06f};d.move_held_object_world(motion);
 const auto held=*d.frame().state->objects[0].affine_world;
 d.polygon_pointer(point(held,{0,0,3}),direction(held,{0,0,-1}),PolygonAction::Pick);ready(d);d.edit_polygon(PolygonAction::Extrude);ready(d);d.end_polygon(true);ready(d);
 const auto committed=d.copy_scene().world_matrix(source.active().id);for(size_t i=0;i<16;++i)check(std::abs(committed.matrix()[i]-held.matrix()[i])<2e-6,"Accepted held topology retains every actual world affine coefficient");
 check(!d.frame().state->object_grabbing&&d.undo()&&d.copy_scene().encode()==original,"Held affine placement and geometry form one exactly undoable publication");
}
void controller(bool reflect){
 auto source=fixture(reflect);const auto original=source.encode();Document d;d.replace(source);check(d.wait_mesh(),"Affine controller source publishes real packed geometry");ImportDriver hand(d);
 hand.open();hand.sector(8);hand.sector(9);hand.sector(2);hand.sector(0);ready(d);
 check(d.frame().state->polygon&&!d.frame().state->menu,"Actual joystick opens editable affine workspace without disabling tools");
 auto aim=[&](Vec3 offset,float pressure){
   const auto state=d.frame().state;const auto world=affine_similarity(state->canvas)*source.world_matrix(source.active().id);
   auto n=world.normal({0,0,1});const auto size=std::hypot(n[0],n[1],n[2]);Vec3 normal{float(n[0]/size),float(n[1]/size),float(n[2]/size)};
   auto p=point(world,{0,0,.5f})+normal*2+offset;auto q=arcball({0,0,-1},normal*-1);
   auto &h=hand.frame.hands[1].tracked;h.aimValid=true;h.aim.position={p.x,p.y,p.z};h.aim.orientation={q.x,q.y,q.z,q.w};hand.send(0,0,false,pressure);
 };
 aim({},.8f);aim({},0);ready(d);check(d.frame().state->polygon->selection->ids.size()==1&&d.copy_scene().encode()==original,"Actual XR pressure/ray selects genuine face through canvas plus full affine world frame");
 hand.open();hand.sector(4);check(d.frame().state->polygon_controls.move_mode&&!d.frame().state->menu,"Actual XR move control remains usable on affine topology");
 const auto tracking=affine_similarity(d.frame().state->canvas)*source.world_matrix(source.active().id);const auto shift=normalized(direction(tracking,{1,0,0}))*.03f;
 aim({},.8f);aim(shift,.8f);ready(d);check(d.frame().state->polygon->moving&&length(d.frame().state->polygon->move_delta)>0,"Actual tracked affine drag publishes nonzero immutable-mesh GPU motion");
 hand.frame.focused=false;hand.send();ready(d);check(!d.frame().state->polygon->moving&&d.copy_scene().encode()==original,"Actual XR focus loss cancels affine component gesture without source/history change");
 hand.frame.focused=true;hand.send();hand.open();hand.sector(7);ready(d);check(d.frame().state->polygon->topology->faces().size()==10,"Actual XR Extrude retains true affine metric operation path");hand.sector(10);ready(d);
 check(!d.frame().state->polygon&&d.copy_scene().active().polygon->topology.faces().size()==10,"Actual joystick Apply publishes affine topology on normal worker/transaction");
 check(d.undo()&&d.copy_scene().encode()==original,"XR affine operation Undo restores complete original source");
}
void faults(){
 auto cube=PolygonMesh::cube();auto world=frame(true);PolygonWorkspace base(cube);base.select(cube.faces()[1].id);auto expected=base.view().topology;const auto exact=cube.encode();
 auto count=base;calls=0;deny=SIZE_MAX;armed=true;count.apply_affine(world,PolygonOperation::Extrude,.02f);armed=false;const auto baseline=calls;
 for(size_t at=1;at<=baseline;++at){auto candidate=base;calls=0;deny=at;armed=true;bool caught=false;try{candidate.apply_affine(world,PolygonOperation::Extrude,.02f);}catch(const std::bad_alloc&){caught=true;}armed=false;
  check(caught&&candidate.view().topology==expected&&base.view().topology->encode()==exact&&!candidate.view().can_undo,"Every measured affine kernel allocation denial preserves exact topology/history owner");
  candidate.apply_affine(world,PolygonOperation::Extrude,.02f);check(candidate.view().changed,"Same authentic affine operation retries after allocation recovery");
 }
 size_t callbacks=0;auto probe=base;PolygonLimits limits;limits.cancelled=[&]{++callbacks;return false;};probe.apply_affine(world,PolygonOperation::Extrude,.02f,1,limits);
 for(size_t at=1;at<=callbacks;++at){auto candidate=base;size_t n=0;PolygonLimits c;c.cancelled=[&]{return ++n>=at;};check(refuses([&]{candidate.apply_affine(world,PolygonOperation::Extrude,.02f,1,c);})&&candidate.view().topology==expected,"Every actual affine kernel Cancel callback preserves source/history owner");}
 std::cout<<"allocation_prefixes="<<baseline<<" cancel_sites="<<callbacks<<'\n';
}
}
void *operator new(size_t n){if(armed&&++calls>=deny)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}void operator delete(void *p)noexcept{std::free(p);}void operator delete[](void *p)noexcept{std::free(p);}void operator delete(void *p,size_t)noexcept{std::free(p);}void operator delete[](void *p,size_t)noexcept{std::free(p);}
int main(int argc,char **argv)try{
 const bool keep=argc==3&&std::string_view(argv[1])=="--out";
 auto out=keep?std::filesystem::path(argv[2]):std::filesystem::temp_directory_path()/("dome-polygon-affine-"+std::to_string(getpid()));
 std::filesystem::create_directories(out);struct Cleanup{std::filesystem::path p;bool keep;~Cleanup(){if(!keep){std::error_code e;std::filesystem::remove_all(p,e);}}}cleanup{out,keep};
 setenv("XDG_STATE_HOME",out.c_str(),1);setenv("XDG_CONFIG_HOME",out.c_str(),1);
 if(argc==2&&std::string_view(argv[1])=="--controller"){for(bool r:{false,true})controller(r);std::cout<<checks<<" affine controller checks\n";return 0;}
 if(argc==2&&std::string_view(argv[1])=="--faults"){faults();std::cout<<checks<<" affine polygon fault checks\n";return 0;}
 for(bool reflect:{false,true}){kernel(reflect);inset(reflect);native(reflect,out);}
 std::cout<<checks<<" affine polygon checks\n";return 0;
}catch(const std::exception &e){armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
