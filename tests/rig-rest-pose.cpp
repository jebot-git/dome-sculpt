// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig-rest.hpp"
#include "dome/rig-edit.hpp"
#include "polygon-conversion.hpp"
#include "scene-feature-version.hpp"
#include "current-color-target-fixture.hpp"
#include "dome/color-preparation.hpp"
#include "polygon-paint.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;std::cout<<"PASS "<<m<<'\n';}
void reject(auto f,const char *m){bool failed=false;try{f();}catch(...){failed=true;}check(failed,m);}
Skeleton rig(){Skeleton s;s.bones={{"root",-1,{{.1f,.2f,0},{},{1.2f,.8f,1}},.3f},{"child",0,{{.15f,.3f,0},{},{.8f,1.1f,1}},.2f},{"sibling",0,{{-.2f,.2f,.05f},{},{1,.9f,1.2f}},.2f}};s.bones[1].rest_matrix=RigMatrix{1,0,0,0,.25f,1,0,0,0,.1f,1,0,.15f,.3f,0,1};s.space_matrix=RigMatrix{-1,0,0,0,.2f,1,0,0,0,.1f,1.1f,0,.2f,-.1f,0,1};return s;}
Mesh mesh(){Mesh m;m.positions={{-.2f,0,0},{.2f,0,0},{0,.5f,0}};m.normals.assign(3,{0,0,1});m.indices={0,1,2};m.uv={{{0,0}},{{1,0}},{{.5f,1}}};return m;}
void near(const RigMatrix &a,const RigMatrix &b,double tolerance){for(size_t i=0;i<16;++i)if(std::abs(double(a[i])-b[i])>tolerance)throw std::runtime_error("Independent matrix mismatch");}
}
int main()try{
 auto s=rig();std::vector<JointTransform> pose;for(const auto &b:s.bones)pose.push_back(b.rest);pose[0].rotation=arcball({0,1,0},normalized({.2f,1,.1f}));
 const RigMatrix exact{.9f,.1f,0,0,.35f,1.2f,0,0,.1f,.2f,1,0,.3f,.4f,.05f,1};pose[1].full_local_matrix=exact;
 check(s.local_matrices(pose)[1]==exact,"Exact pose-local authority bypasses TRS/rest-reference recomposition bit-exactly");
 auto worlds=s.world(pose);near(worlds[1],(AffineTransform(worlds[0])*AffineTransform(exact)).float_matrix(),1e-6);check(true,"Independent parent-affine composition matches exact posed world matrix");
 auto bad=s;bad.bones[0].rest.full_local_matrix=exact;reject([&]{bad.validate();},"Bone.rest refuses pose-only affine authority");
 auto adjusted=adjust_joint_pose(pose[1],arcball({0,1,0},normalized({.1f,1,0})),{.03f,0,0});
 auto q=arcball({0,1,0},normalized({.1f,1,0}));auto t=AffineTransform(std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,.03f,0,0,1});near(*adjusted.full_local_matrix,(t*AffineTransform(exact)*AffineTransform(q.matrix())).float_matrix(),1e-7);check(true,"Exact pose delta has independent T(parent)*M*R(local) semantics without TRS guessing");
 DeformationState d(mesh());d.skeleton=s;d.skin=SkinWeights{{{{0,1}},{{1,1}},{{1,.5f},{2,.5f}}}};d.pose=pose;d.validate(mesh());const auto encoded=d.encode(mesh());check(encoded["version"]==2&&DeformationState::decode(encoded,mesh()).encode(mesh())==encoded,"Conditional deformation2 persists exact pose matrices and original editable references");
 auto disguised=encoded;disguised["version"]=1;reject([&]{DeformationState::decode(disguised,mesh());},"Exact pose marker cannot masquerade as legacy deformation1");auto empty=d;for(auto &j:empty.pose)j.full_local_matrix.reset();auto legacy=empty.encode(mesh());check(legacy["version"]==1,"Ordinary TRS poses retain exact legacy deformation1 format");legacy["version"]=2;reject([&]{DeformationState::decode(legacy,mesh());},"Version2 without actual affine pose authority is refused");
 auto singular=d;singular.pose[1].full_local_matrix->at(0)=0;singular.pose[1].full_local_matrix->at(1)=0;reject([&]{singular.validate(mesh());},"Singular exact pose is explicitly refused");
 const auto before=s.skin_matrices(pose);std::array<double,16> w{-1.1,0,0,0,.4,.9,0,0,.1,.2,1.3,0,1,2,-.5,1};AffineTransform world(w);
 auto old_world=world*AffineTransform(s.world()[1]);auto p=old_world.point({0,0,0});RigRestJointEdit move;move.joint=1;move.world_position=Vec3{float(p[0]+.07),float(p[1]-.03),float(p[2]+.02)};move.parent=2;
 auto next=transport_rig_rest(s,pose,world,std::span<const RigRestJointEdit>(&move,1));check(next.changed&&next.skeleton.bones[1].parent==2&&next.pose[1].full_local_matrix,"Actual reparent/head fitting produces an explicit exact local pose rather than a reset");
 auto after=next.skeleton.skin_matrices(next.pose);for(size_t j=0;j<before.size();++j)near((world*AffineTransform(before[j])).float_matrix(),(world*AffineTransform(after[j])).float_matrix(),2e-5);check(true,"Independent world joint skin products preserve authored binds and current pose through reparent");
 auto joint_world=world*AffineTransform(next.skeleton.world()[1]);auto position=joint_world.point({0,0,0});check(std::abs(position[0]-move.world_position->x)<2e-5&&std::abs(position[1]-move.world_position->y)<2e-5&&std::abs(position[2]-move.world_position->z)<2e-5,"New REST head reaches independent requested world coordinates under reflection and shear");
 RigRestJointEdit cycle;cycle.joint=0;cycle.parent=1;reject([&]{transport_rig_rest(s,pose,world,std::span<const RigRestJointEdit>(&cycle,1));},"Cyclic proposed rest graph refuses without touching source");reject([&]{transport_rig_rest(s,pose,world,std::span<const RigRestJointEdit>(&move,1),{},[]{return true;});},"Actual rest transport honors cancellation before publication");
 reject([&]{std::vector<JointTransform> invalid(s.bones.size()+1);transport_rig_rest(s,invalid,world,{});},"Empty edits still reject invalid pose dimensions before copying unbounded caller data");
 auto noop=transport_rig_rest(s,pose,world,{});check(!noop.changed&&noop.skeleton.encode()==s.encode(),"Empty rest edits retain exact original skeleton without phantom change");
 Scene scene;scene.active().painted=std::make_shared<PaintedMesh>(mesh(),64);scene.active().mesh_active=true;scene.set_deformation(1,d);auto json=scene.encode();check(json["version"]==25&&json["polygon_sources"]["version"]==1,"Pose-only Scene25 does not fabricate Scene20 color preparation or rig-source pool");check(Scene::decode(json).encode()==json,"Pose-only Scene25 with null source fields restarts exactly");
 auto concealed=json;concealed["version"]=18;for(auto &object:concealed["objects"])object.erase("rig_source");bool exact_diagnostic=false;try{Scene::decode(concealed);}catch(const std::exception &e){std::cerr<<"DIAG concealed Scene18 refusal: "<<e.what()<<"\n";exact_diagnostic=std::string_view(e.what())=="Exact pose-local matrices require Scene25";}check(exact_diagnostic,"Legacy Scene18 shape without new nullable fields specifically refuses hidden exact pose authority before allocation");
 auto downgraded=json;downgraded["version"]=20;reject([&]{Scene::decode(downgraded);},"Legacy Scene cannot admit a hidden exact posed-local marker");
 auto original=scene;original.active().deformation=std::make_shared<const DeformationState>(empty);scene.active().rig_source=capture_polygon_origin(original,1);scene.active().shape_source=scene.active().rig_source;auto archive=scene.encode();check(archive["polygon_sources"]["version"]==6&&!archive["polygon_sources"]["shared_colors"].get<bool>()&&archive["polygon_sources"]["origins"].size()==1,"Rig source archive6 owns direct history and deduplicates shape/rig aliases without false shared colors");auto restored=Scene::decode(archive);check(restored.active().rig_source==restored.active().shape_source&&restored.encode()==archive,"Scene25 restores exact shared first-origin identities and source contexts");
 auto corrupt=archive;corrupt["polygon_sources"]["origins"][0]["rig_source"]=0;reject([&]{Scene::decode(corrupt);},"Serialized rig-source cycles refuse before allocating authoring objects");
 {
  auto colored=current_color_bake_fixture::fixture(true,false);const auto id=colored.objects().front().id;colored.select(id);auto &target=colored.object(id);target.visible=true;target.locked=false;
  auto descriptor=std::make_shared<PolygonColorSurface>();descriptor->topology=std::make_shared<const PolygonMesh>(target.polygon->topology);descriptor->field=target.polygon->colors;
  auto strict=polygon_paint_geometry(*target.painted,target.painted->mesh(),{});ColorPreparationOptions options;options.resolve_identity=true;
  auto prepared=prepare_source_colors({strict,target.appearance,target.material_bindings,target.deformation,target.material,descriptor},options);target.painted=prepared.paint;target.material_bindings=prepared.material_bindings;colored.validate();
  const auto legacy_color=colored.encode();check(legacy_color["version"]==20&&legacy_color["polygon_sources"]["version"]==5,"Actual existing shared color recipe retains its unchanged Scene20 context");
  colored.object(id).rig_source=capture_polygon_origin(colored,id);const auto combined=colored.encode();check(combined["version"]==25&&combined["polygon_sources"]["version"]==6&&combined["polygon_sources"]["shared_colors"].get<bool>(),"Rig history6 explicitly composes actual shared current-corner color context");
  auto decoded=Scene::decode(combined);check(decoded.encode()==combined&&decoded.object(id).rig_source->material_bindings->color_preparation->polygon_colors->field==decoded.object(id).polygon->colors,"Scene25 restores exact current and first-source shared field/recipe owner identities");
 }
 {
  Scene grouped;auto &first=grouped.active();first.painted=std::make_shared<PaintedMesh>(mesh(),64);first.mesh_active=true;first.deformation=std::make_shared<const DeformationState>(d);
  const auto group=grouped.add_group("Linked authored rig");first.group=group;
  const auto other=grouped.duplicate(1);auto second=d;second.pose[0]=adjust_joint_pose(second.pose[0],arcball({0,1,0},normalized({-.1f,1,.2f})),{.02f,0,0});grouped.set_deformation(other,second);grouped.object(other).visible=false;
  auto outside=grouped.duplicate(1);grouped.object(outside).deformation=std::make_shared<const DeformationState>(*grouped.object(1).deformation);const auto outside_owner=grouped.object(outside).deformation;grouped.select(1);
  const auto automatic=rig_rest_linked_objects(grouped,1);check(std::find(automatic.begin(),automatic.end(),outside)==automatic.end(),"Unrelated equal-valued skeletons cannot enroll by group or skeleton equality alone");
  const auto before=grouped.encode();const auto first_paint=grouped.object(1).painted,second_paint=grouped.object(other).painted;
  auto head=grouped.world_matrix(1)*AffineTransform(s.world()[1]);auto point=head.point({0,0,0});RigRestJointEdit edit;edit.joint=1;edit.world_position=Vec3{float(point[0]+.05),float(point[1]),float(point[2])};edit.parent=2;
  RigRestOptions linked_options;linked_options.linked_objects={1,other};
  check(author_rig_rest(grouped,1,{&edit,1},linked_options),"Actual scene operation fits and reparents linked authored rig parts including hidden parts");
  const auto &one=*grouped.object(1).deformation,&two=*grouped.object(other).deformation;
  for(size_t j=0;j<s.bones.size();++j){near(d.skeleton->skin_matrices(d.pose)[j],one.skeleton->skin_matrices(one.pose)[j],2e-5);near(second.skeleton->skin_matrices(second.pose)[j],two.skeleton->skin_matrices(two.pose)[j],2e-5);}
  check(one.pose[0].rotation.x!=two.pose[0].rotation.x&&one.skeleton->encode()==two.skeleton->encode(),"Each linked part retains its own authored current pose with one exact shared new REST authority");
  check(grouped.object(1).painted==first_paint&&grouped.object(other).painted==second_paint&&grouped.object(1).rig_source->deformation->encode(mesh())==d.encode(mesh())&&grouped.object(other).rig_source->deformation->encode(mesh())==second.encode(mesh()),"Rest authoring preserves current paint identity and exact distinct first-source bind/pose states");
  const auto source=grouped.object(1).rig_source;auto repeat=edit;repeat.world_position->x+=.02f;author_rig_rest(grouped,1,{&repeat,1},linked_options);check(grouped.object(1).rig_source==source,"Repeated rest edits retain the immutable first source instead of growing provenance chains");
  check(grouped.object(outside).deformation==outside_owner&&!grouped.object(outside).rig_source,"Explicit linked target lease preserves arbitrary outside object source exactly");
  const auto current=grouped.encode();grouped.object(other).locked=true;const auto locked=grouped.encode();reject([&]{author_rig_rest(grouped,1,{&edit,1},linked_options);},"A locked linked part refuses the whole rest edit");check(grouped.encode()==locked,"Locked linked refusal is scene-exact and preserves all first sources");grouped.object(other).locked=false;
  reject([&]{author_rig_rest(grouped,1,{&edit,1},{},[]{return true;});},"Scene-level rest edit honors actual cancellation");check(grouped.encode()==current,"Cancelled rest publication preserves exact scene/history source state");
  auto restarted=Scene::decode(current);check(restarted.encode()==current,"Actual linked rest-fitting operation and complete first sources survive Scene25 restart");restore_rig_source(restarted,1,linked_options);check(restarted.encode()==before,"Linked Restore recovers exact original paint, rigs, distinct poses and all source metadata");
 }
 {
  Scene unbound;unbound.active().painted=std::make_shared<PaintedMesh>(mesh(),64);unbound.active().mesh_active=true;auto state=d;state.skin.reset();unbound.set_deformation(1,state);const auto original=unbound.encode();RigRestJointEdit edit;edit.joint=1;edit.parent=2;check(author_rig_rest(unbound,1,{&edit,1})&&!unbound.active().deformation->skin,"Unbound authored skeletons support rest/reparent controls without silently binding skin");restore_rig_source(unbound,1);check(unbound.encode()==original,"Unbound rig Restore retains exact original skeletal and painting data");
 }
 std::cout<<checks<<" exact pose/rest transport/Scene25 checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
