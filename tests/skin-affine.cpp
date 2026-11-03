// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char *text){++checks;if(!value)throw std::runtime_error(text);}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
Vec3 point(const AffineTransform &m,Vec3 p){auto q=m.point({p.x,p.y,p.z});return {float(q[0]),float(q[1]),float(q[2])};}
Vec3 normal(const AffineTransform &m,Vec3 p){auto q=m.normal({p.x,p.y,p.z});return normalized(Vec3{float(q[0]),float(q[1]),float(q[2])});}
AffineTransform matrix(bool reflected,float scale=1){return AffineTransform(std::array<double,16>{reflected?-1.3*scale:1.3*scale,.1*scale,0,0,.35*scale,.8*scale,.05*scale,0,.1*scale,.15*scale,1.2*scale,0,.2,.1,-.2,1});}
Scene fixture(int bind=0,bool reflected=true,bool dyadic=false){
 Mesh m;m.positions={{dyadic?-.125f:-.1f,0,0},{dyadic?.125f:.1f,0,0},{0,.5f,0}};m.normals.assign(3,{0,0,1});m.indices={0,1,2};m.uv={{{0,0}},{{1,0}},{{.5f,1}}};
 Scene s;s.active().name="Source rig";s.active().painted=std::make_shared<PaintedMesh>(m,64);s.active().mesh_active=true;
 auto parent=s.add_group("Source parent");s.group(parent).affine_edit=std::make_shared<const NativeAffineEdit>(matrix(false));s.active().group=parent;
 if(dyadic){auto a=AffineTransform{}.matrix();a[4]=.5;s.group(parent).affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(a));}
 DeformationState d(m);d.skeleton=Skeleton{{{"root",-1,{},.3f},{"tip",0,{{0,.3f,0}},.2f}}};
 if(bind){d.skeleton->space_matrix=matrix(false,.9f).float_matrix();auto a=AffineTransform{}.matrix();a[4]=.17;a[12]=.03;d.skeleton->bones[0].rest_matrix=AffineTransform(a).float_matrix();d.skeleton->inverse_bind_override=bind==1?std::vector<RigMatrix>(2,AffineTransform{}.float_matrix()):d.skeleton->inverse_bind();}
 d.skin=SkinWeights{{{{0,.8f},{1,.2f}},{{0,.4f},{1,.6f}},{{1,1}}}};
 d.pose={d.skeleton->bones[0].rest,d.skeleton->bones[1].rest};d.pose[0].translation.z=.2f;d.pose[1].rotation={.92387953f,0,0,.38268343f};
 d.landmarks=HumanLandmarks{};d.landmarks->points={{"hips",{0,0,0}},{"head",{0,.5f,0}}};s.set_deformation(1,d);
 auto target=s.add("Affine target",Volume{});s.object(target).mesh_active=true;s.object(target).affine_edit=std::make_shared<const NativeAffineEdit>(matrix(reflected,.7f));s.select(target);
 if(dyadic){auto a=AffineTransform{}.matrix();a[0]=-2;a[4]=.5;a[5]=.5;s.object(target).affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(a));}
 auto relative=s.world_matrix(target).inverse()*s.world_matrix(1);auto t=m;for(size_t i=0;i<t.positions.size();++i){t.positions[i]=point(relative,m.positions[i]);t.normals[i]=normal(relative,m.normals[i]);}
 // The persistent target has its own authored topology/frame, not a world-space copy.
 if(relative.orientation()==AffineOrientation::Reversing)std::swap(t.indices[1],t.indices[2]);
 s.object(target).painted=std::make_shared<PaintedMesh>(t,64);DeformationState shapes(t);shapes.morphs=MorphSet(t);auto moved=t.positions;moved[0].z+=.02f;shapes.morphs->set_target(t,"Artist key",moved);shapes.morph_weights={.25f};s.set_deformation(target,shapes);s.validate();return s;
}
void compare_pose(const Scene &s){
 const auto &a=s.object(1),&b=s.object(2);auto source=a.deformation->preview(a.painted->mesh());
 // Disable the independently authored target key only for this correspondence oracle.
 auto target_state=*b.deformation;target_state.morph_weights={0};auto target=target_state.preview(b.painted->mesh());
 for(size_t i=0;i<source.positions.size();++i){check(length(point(s.world_matrix(1),source.positions[i])-point(s.world_matrix(2),target.positions[i]))<3e-5f,"Copied exact rest/pose/IBM skin commutes with both full world placements");
  check(dot(normal(s.world_matrix(1),source.normals[i]),normal(s.world_matrix(2),target.normals[i]))>.9999f,"Copied posed normals preserve independent inverse-transpose world direction");}
}
SkinAuthoringOptions options(){SkinAuthoringOptions o;o.reference=1;o.mode=SkinAuthoringMode::Transfer;o.transfer_distance=.001f;return o;}
}
int main(int argc,char **argv)try{
 if(argc==3&&std::string(argv[1])=="--ui-fixture"){auto s=fixture(2,true,true);s.object(1).visible=false;s.object(1).locked=true;s.save(argv[2]);return 0;}
 if(argc==3&&std::string(argv[1])=="--inspect"){auto s=Scene::load(argv[2]);std::cout<<nlohmann::json{{"skinned",bool(s.active().deformation&&s.active().deformation->skin)},{"scene",s.encode()}}.dump()<<'\n';return 0;}
 for(int bind:{0,1,2})for(bool reflection:{false,true}){
  auto s=fixture(bind,reflection);auto before=s.encode();auto paint=s.active().painted;auto morph=s.active().deformation->morphs->encode();auto old_axes=s.object(1).deformation->landmarks;
  check(author_skin(s,2,options()),"Affine transfer creates actual skin weights");
  check(s.active().painted==paint&&s.active().deformation->morphs->encode()==morph&&s.active().deformation->morph_weights==std::vector<float>{.25f},"Affine binding preserves exact paint, rest geometry and authored morph channels/values");
  check(s.encode()["objects"][0]==before["objects"][0],"Complete source rig, pose, appearance and placement remain immutable");
  for(size_t v=0;v<3;++v)for(auto expected:s.object(1).deformation->skin->vertices[v]){float actual=0;for(auto i:s.active().deformation->skin->vertices[v])if(i.joint==expected.joint)actual=i.weight;check(std::abs(actual-expected.weight)<2e-5f,"World barycentric transfer retains independently authored endpoint weights");}
  compare_pose(s);auto c=s.world_matrix(2).inverse()*s.world_matrix(1);
  for(const auto &[name,p]:old_axes->points)check(length(s.active().deformation->landmarks->points.at(name)-point(c,p))<1e-6f,"Copied landmark positions retain full affine transport");
  auto encoded=s.encode();check(Scene::decode(encoded).encode()==encoded,"Affine binding exact archive roundtrip preserves local rig and source data");
  auto closed=s.active().deformation;auto lock=options();lock.reference=2;lock.mode=SkinAuthoringMode::Automatic;lock.locks={1,1};lock.maximum_work=1;
  check(!author_skin(s,2,lock)&&s.active().deformation==closed,"Fully locked affine rebind is exact no-op despite tiny solve budget");
 }
 // Independent world segments, constructed from endpoints rather than the
 // production space/IBM transport helper, define the automatic-bind oracle.
 for(bool reflected:{false,true}){
  auto scene=fixture(0,reflected);auto original=scene.encode();auto world=scene.world_matrix(1);auto mesh=scene.object(1).painted->mesh();
  for(size_t v=0;v<mesh.positions.size();++v){mesh.positions[v]=point(world,mesh.positions[v]);mesh.normals[v]=normal(world,mesh.normals[v]);}
  auto a=point(world,{}),b=point(world,{0,.3f,0}),c=point(world,{0,.5f,0});auto rotation=arcball({0,1,0},normalized(b-a));
  Skeleton rig{{{"root",-1,{a,rotation},length(b-a)},{"tip",0,{{0,length(b-a),0}},length(c-b)}}};
  BindOptions bind;auto expected=automatic_bind(mesh,rig,bind);SkinAuthoringOptions operation;operation.reference=1;
  check(author_skin(scene,2,operation),"Automatic binding supports a real reflected/sheared target and source");
  for(size_t v=0;v<mesh.positions.size();++v)for(auto i:expected.vertices[v]){float actual=0;for(auto w:scene.active().deformation->skin->vertices[v])if(w.joint==i.joint)actual=w.weight;check(std::abs(actual-i.weight)<2e-5f,"Automatic weights equal independent physical-world segment/edge metric");}
  check(scene.encode()["objects"][0]==original["objects"][0],"Automatic solve retains complete immutable source rig");
 }
 // A common translation must change neither metric nor serialized skin data.
 for(auto mode:{SkinAuthoringMode::Automatic,SkinAuthoringMode::Transfer}){
  auto near=fixture(2);auto distant=near.snapshot();auto common=distant.add_group("Distant common frame");auto m=AffineTransform{}.matrix();m[12]=20000;m[13]=30000;m[14]=-20000;
  distant.group(common).affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(m));distant.group(1).parent=common;distant.object(2).group=common;distant.validate();
  auto operation=options();operation.mode=mode;operation.seed_distance=1;check(author_skin(near,2,operation)&&author_skin(distant,2,operation),"Valid distant scene binds without absolute-coordinate rejection");
  check(near.active().deformation->encode(near.active().painted->mesh())==distant.active().deformation->encode(distant.active().painted->mesh()),"Common distant placement preserves exact weights/rig/pose/landmarks and morph authoring data");
 }
 auto source=fixture(2);auto exact=source.encode();auto o=options();o.maximum_work=1;
 check(refuses([&]{author_skin(source,2,o);})&&source.encode()==exact,"World packing/rig transport budget refusal preserves entire scene");
 o=options();size_t calls=0;check(refuses([&]{author_skin(source,2,o,[&]{return ++calls>8;});})&&source.encode()==exact,"Cancellation during actual affine preparation preserves all original owners/data");
 // A 5mm normal displacement must be measured in world metres, despite source stretch.
 auto distant=fixture();auto rest=distant.active().painted->mesh();auto world=distant.world_matrix(2);auto inverse=world.inverse();auto n=normal(distant.world_matrix(1),{0,0,1});
 for(auto &p:rest.positions)p=point(inverse,point(world,p)+n*.005f);distant.active().painted=std::make_shared<PaintedMesh>(rest,64);distant.active().deformation.reset();auto distant_before=distant.encode();
 check(refuses([&]{author_skin(distant,2,options());})&&distant.encode()==distant_before,"True world distance rejects outside transfer radius without guessed uniform scale");
 Document d;d.replace(fixture(2));check(d.wait_mesh(),"Actual affine mesh snapshot becomes ready");check(d.frame().state->can_configure_skin&&d.frame().state->can_bind_skin,"Native affine target and reference expose existing bind controls");
 auto before=d.copy_scene().encode();d.bind_skin(options(),d.draft_scene().source);auto path=std::filesystem::temp_directory_path()/("dome-affine-skin-"+std::to_string(getpid())+".dsculpt");auto save=d.save_async(path);save.get();check(d.wait_asset()&&d.frame().state->error.empty()&&d.copy_scene().active().deformation->skin,"Actual ordered worker performs affine binding and dependent Save");
 auto after=d.copy_scene().encode();check(Scene::load(path).encode()==after,"Queued native Save/reopen retains exact affine rig and paint endpoint");
 check(d.undo()&&d.copy_scene().encode()==before&&d.redo()&&d.copy_scene().encode()==after,"One global Undo/Redo restores exact complete binding endpoints");std::filesystem::remove(path);
 std::cout<<"PASS "<<checks<<" affine skin checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
