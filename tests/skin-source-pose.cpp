// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_affine_skin_test
#include "skin-affine.cpp"
#undef main
#include "dome/rig-rest.hpp"
#include "polygon-conversion.hpp"
namespace {
Scene exact_pose_fixture(bool child){
 auto s=fixture(0,false);s.group(1).affine_edit.reset();s.object(2).affine_edit.reset();
 s.object(1).transform={{.25f,.1f,-.2f},{.98078528f,0,0,.19509032f},1.25f};
 s.object(2).transform={{-.1f,.2f,.15f},{.96592583f,0,.25881905f,0},.75f};
 auto source=*s.object(1).deformation;source.pose[child?1:0].full_local_matrix=matrix(false,.9f).float_matrix();s.set_deformation(1,source);
 auto mesh=s.object(1).painted->mesh();const auto change=s.world_matrix(2).inverse()*s.world_matrix(1);
 for(size_t v=0;v<mesh.positions.size();++v){mesh.positions[v]=point(change,mesh.positions[v]);mesh.normals[v]=normal(change,mesh.normals[v]);}
 s.object(2).painted=std::make_shared<PaintedMesh>(mesh,64);DeformationState target(mesh);target.morphs=MorphSet(mesh);auto moved=mesh.positions;moved[0].z+=.02f;target.morphs->set_target(mesh,"Artist key",moved);target.morph_weights={.25f};s.set_deformation(2,target);s.validate();return s;
}
}
int main()try{
 for(bool child:{false,true}){
  auto s=exact_pose_fixture(child);const auto before=s.encode();const auto paint=s.object(2).painted;
  check(!s.has_affine_placement(1)&&!s.has_affine_placement(2)&&!s.object(1).deformation->skeleton->space_matrix&&!s.object(1).deformation->skeleton->inverse_bind_override,"Exact-pose-only fixture has ordinary object/rest/bind frames");
  check(author_skin(s,2,options()),"Actual skin transfer accepts exact local pose authority under distinct ordinary placements");compare_pose(s);
  check(s.object(2).deformation->pose[child?1:0].full_local_matrix==s.object(1).deformation->pose[child?1:0].full_local_matrix,"Copied full local pose is byte-exact instead of modifying ignored editable TRS references");
  check(s.object(2).painted==paint&&s.encode()["objects"][0]==before["objects"][0],"Full-pose binding preserves source and target paint authority");
  auto encoded=s.encode();check(encoded["version"]==25&&Scene::decode(encoded).encode()==encoded,"Bound exact pose and morph data restart exactly through conditional Scene25");
 }
 {
  auto s=exact_pose_fixture(false);s.select(1);const auto original=s.encode();RigRestJointEdit edit;edit.joint=0;edit.world_length=.41f;
  check(author_rig_rest(s,1,{&edit,1})&&s.object(1).rig_source,"Genuine rest authoring creates immutable first-rig history");auto first=s.object(1).rig_source;const auto target=s.encode()["objects"][1];
  SkinAuthoringOptions automatic;automatic.reference=1;automatic.seed_distance=2;check(author_skin(s,1,automatic),"Exact-pose existing rig can recompute automatic weights");
  check(s.object(1).rig_source==first&&s.encode()["objects"][1]==target,"Rebinding preserves first-rig source ownership and unrelated target exactly");
  const auto encoded=s.encode();auto restarted=Scene::decode(encoded);check(restarted.encode()==encoded&&restarted.object(1).rig_source,"Real retained rig-source graph and rebound exact pose restart together");
  restore_rig_source(restarted,1);check(restarted.encode()==original,"Restore after new binding returns original rig pose weights and source data exactly");
 }
 Document doc;doc.replace(exact_pose_fixture(true));check(doc.wait_mesh(),"Exact-pose document has coherent native render snapshot");const auto before=doc.copy_scene().encode();doc.bind_skin(options(),doc.draft_scene().source);
 const auto file=std::filesystem::temp_directory_path()/("dome-source-pose-"+std::to_string(getpid())+".dsculpt");doc.save_async(file).get();check(doc.wait_asset()&&doc.frame().state->error.empty(),"Exact-pose transfer and immediate Save complete on actual ordered worker");const auto after=doc.copy_scene().encode();
 check(Scene::load(file).encode()==after,"Exact-pose dependent Save reopens to complete accepted endpoint");check(doc.undo()&&doc.copy_scene().encode()==before&&doc.redo()&&doc.copy_scene().encode()==after,"Exact-pose bind owns one global Undo/Redo transaction");std::filesystem::remove(file);
 std::cout<<"PASS "<<checks<<" merged exact-pose skin checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
