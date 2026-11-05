// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-fixture.hpp"
#include "dome/source-placement.hpp"
#include <filesystem>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool b,const char *message){if(!b)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<std::endl;}
template<class F> bool refuses(F f){try{f();return false;}catch(const std::exception &){return true;}}
void ready(Document &d,bool polygon){check(polygon?d.wait_polygon():d.wait_retopology(),"Actual workspace worker reaches endpoint");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
void idle(Document &d){check(d.wait_asset()&&d.wait_mesh(),"Ordered native publication and geometry finish");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
Vec3 point(const AffineTransform &w,Vec3 p){auto q=w.point({p.x,p.y,p.z});return {float(q[0]),float(q[1]),float(q[2])};}
Vec3 vector(const AffineTransform &w,Vec3 p){auto q=w.direction({p.x,p.y,p.z});return {float(q[0]),float(q[1]),float(q[2])};}
Scene source(bool reflected){
 auto s=polygon_test::fixture();auto m=AffineTransform{}.matrix();m[0]=reflected?-1.3:1.3;m[1]=.1;m[4]=.35;m[5]=.8;m[9]=.15;m[10]=1.2;m[12]=.15;m[13]=-.1;m[14]=.2;
 auto group=s.add_group("Combined skutečný affine parent");s.group(group).affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(m));s.active().group=group;
 const auto &mesh=s.active().painted->mesh();DeformationState d(mesh);d.skeleton=Skeleton{{{"root",-1,{},.2f}}};d.skin=SkinWeights{};d.skin->vertices.assign(mesh.positions.size(),{{0,1}});d.morphs=MorphSet(mesh);auto target=mesh.positions;for(auto &p:target)p.z+=.04f;d.morphs->set_target(mesh,"Retained crease",target);d.morph_weights={.35f};d.pose.resize(1);d.pose[0].translation={.01f,-.02f,.06f};d.pose[0].scale={1.1f,.85f,1.2f};s.set_deformation(s.active().id,std::move(d));s.validate();return s;
}
Vec3 posed(Vec3 p){return {p.x*1.1f+.01f,p.y*.85f-.02f,(p.z+.35f*.04f)*1.2f+.06f};}
void saved(Document &d,const std::filesystem::path &path){auto f=d.save_async(path);f.get();idle(d);check(Scene::load(path).encode()==d.copy_scene().encode(),"Immediate ordered Save/reopen retains complete current native authority");}
nlohmann::json polygon_cycle(Document &d,const std::filesystem::path &path,bool held_cancel){
 const auto before=d.copy_scene().encode();const auto object=d.copy_scene().active().id;
 d.begin_polygon(true);ready(d,true);const auto draft=d.frame().state->polygon;
 check(!d.frame().state->can_retopologize&&refuses([&]{d.begin_retopology();})&&d.frame().state->polygon==draft&&!d.frame().state->retopology&&d.copy_scene().encode()==before,"Polygon workspace excludes real API/menu retopology entry without losing source or draft");
 d.end_polygon(false);check(d.copy_scene().encode()==before,"Explicit polygon conversion Cancel preserves full affine pose and sparse paint");
 if(held_cancel){
  d.configure_object_grab(true,true);d.hold_object(object);d.begin_polygon(true);ready(d,true);check(d.begin_object_grab(object),"Real affine held-object gesture enters during polygon workspace");CanvasTransform move;move.position={.12f,-.04f,.06f};d.move_held_object_world(move);check(d.frame().state->object_grabbing,"Held affine motion remains live before Cancel");d.end_object_grab(false);d.end_polygon(false);d.release_held_object(false);d.configure_object_grab(false,false);check(d.copy_scene().encode()==before,"Cancelling held affine motion and polygon conversion restores exact source/history endpoint");
 }
 d.begin_polygon(true);ready(d,true);const auto current=d.frame().state->polygon;const auto &mesh=current->surface->mesh;const auto i=mesh.indices.front();const auto local=mesh.positions[i]*.25f+mesh.positions[mesh.indices[1]]*.35f+mesh.positions[mesh.indices[2]]*.4f;
 const auto world=d.copy_scene().world_matrix(object);const auto raw=world.normal({mesh.normals[i].x,mesh.normals[i].y,mesh.normals[i].z});const auto nsize=std::hypot(raw[0],raw[1],raw[2]);const Vec3 n{float(raw[0]/nsize),float(raw[1]/nsize),float(raw[2]/nsize)};
 d.polygon_pointer(point(world,local)+n*2,n*-1,PolygonAction::Pick);ready(d,true);check(d.frame().state->polygon->selection&&!d.frame().state->polygon->selection->ids.empty(),"True affine ray selects a genuine converted current face");PolygonControls cfg;cfg.distance_mm=10;d.configure_polygon(cfg);d.edit_polygon(PolygonAction::Extrude);ready(d,true);d.end_polygon(true);saved(d,path);const auto accepted=d.copy_scene().encode();check(accepted!=before&&d.copy_scene().active().polygon&&d.copy_scene().active().polygon->origin,"Actual Apply owns changed polygon topology and complete original source");
 check(d.undo()&&d.copy_scene().encode()==before&&d.redo()&&d.copy_scene().encode()==accepted,"Combined constructor supports exact polygon global Undo/Redo");d.restore_polygon_source();idle(d);check(d.copy_scene().encode()==before,"Source Restore retains complete prior affine/pose/paint/retopology graph");check(d.undo()&&d.copy_scene().encode()==accepted&&d.redo()&&d.copy_scene().encode()==before,"Actual polygon Restore has exact native Undo/Redo");return accepted;
}
void sample(Document &d,Vec3 p,bool finish=true){check(d.begin_desktop(),"Actual desktop owns retopology input");Brush b;b.radius=.01f;b.from=b.to=p;d.stamp_desktop(b);if(finish)d.end_desktop();ready(d,false);}
void retopo_cycle(Document &d,const std::filesystem::path &path,bool convert_after){
 const auto original=d.copy_scene();const auto before=original.encode();const auto id=original.active().id;const auto world=original.world_matrix(id);d.begin_retopology();ready(d,false);auto draft=d.frame().state->retopology;
 check(!d.frame().state->can_polygon&&refuses([&]{d.begin_polygon(true);})&&d.frame().state->retopology==draft&&!d.frame().state->polygon&&d.copy_scene().encode()==before,"Retopology workspace excludes real polygon entry without source or draft mutation");
 const Vec3 p[]{point(world,posed({-.2f,-.2f,.5f})),point(world,posed({.2f,-.2f,.5f})),point(world,posed({.2f,.2f,.5f})),point(world,posed({-.2f,.2f,.5f}))};
 const auto zero=d.frame().state->retopology->topology->encode();sample(d,p[0],false);d.cancel_desktop();ready(d,false);check(d.frame().state->retopology->topology->encode()==zero&&d.copy_scene().encode()==before,"Retained held-point Cancel still restores exact retopology/source state");
 d.end_retopology(false);check(d.copy_scene().encode()==before,"Retopology workspace Cancel preserves complete source");d.begin_retopology();ready(d,false);for(auto x:p)sample(d,x);d.edit_retopology(RetopoAction::Face);ready(d,false);d.end_retopology(true);saved(d,path);const auto accepted=d.copy_scene().encode();check(d.copy_scene().active().retopology&&!d.copy_scene().object(id).visible,"Actual accepted patch retains hidden original and editable retopology");check(d.copy_scene().object(id).painted->encode()==original.active().painted->encode()&&d.copy_scene().object(id).deformation->encode(original.active().painted->mesh())==original.active().deformation->encode(original.active().painted->mesh()),"Affine retopology publication retains exact original sparse paint, skin, morph and pose");
 check(d.undo()&&d.copy_scene().encode()==before&&d.redo()&&d.copy_scene().encode()==accepted,"Combined constructor supports exact retopology global Undo/Redo");
 if(convert_after){const auto converted=polygon_cycle(d,path.string()+"-polygon.dsculpt",false);check(d.copy_scene().encode()==accepted&&d.copy_scene().active().retopology,"Retopology to polygon edit and Restore recovers exact editable patch plus original source");check(d.undo()&&d.copy_scene().encode()==converted&&d.undo()&&d.copy_scene().encode()==accepted,"Separate source-Restore and conversion history remain genuine before undoing retopology");}
 check(d.undo()&&d.copy_scene().encode()==before,"Final retopology Undo returns to original before subsequent workspace entry");
}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 for(bool reflected:{false,true})for(bool polygon_first:{false,true}){
  const auto dir=out/(std::string(reflected?"reflected":"sheared")+(polygon_first?"-polygon-first":"-retopo-first"));std::filesystem::create_directories(dir);auto s=source(reflected);Document d;d.replace(s.snapshot());idle(d);const auto exact=s.encode();check(d.frame().state->can_polygon&&d.frame().state->can_retopologize,"Merged native capability exposes both genuine affine authoring paths");
  if(polygon_first){polygon_cycle(d,dir/"polygon.dsculpt",true);retopo_cycle(d,dir/"retopo.dsculpt",true);}else{retopo_cycle(d,dir/"retopo.dsculpt",true);polygon_cycle(d,dir/"polygon.dsculpt",true);}
  check(d.copy_scene().encode()==exact&&!d.frame().state->polygon&&!d.frame().state->retopology,"Alternating real workspaces preserves exact source and retires both queues");saved(d,dir/"restored.dsculpt");
 }
 std::cout<<"PASS "<<checks<<" combined affine polygon/retopo checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
