// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_affine_skin_test
#include "skin-affine.cpp"
#undef main
#include "dome/rig-rest.hpp"
#include "polygon-conversion.hpp"
#include "cgltf.h"
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
Vec3 parsed_point(const float *m,Vec3 p){return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};}
void verify_export(Scene scene,const std::filesystem::path &path){
 scene.object(1).visible=false;const auto expected=scene.active().deformation->preview(scene.active().painted->mesh());const auto world=scene.world_matrix(2);Document doc;doc.replace(scene);doc.export_asset(path).get();
 cgltf_options options{};cgltf_data *raw=nullptr;check(cgltf_parse_file(&options,path.c_str(),&raw)==cgltf_result_success,"Independent parser reads actual bound exact-pose GLB");std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
 check(cgltf_load_buffers(&options,raw,path.c_str())==cgltf_result_success&&cgltf_validate(raw)==cgltf_result_success,"Bound exact-pose GLB hierarchy skin and accessors independently validate");size_t vertices=0;float wrong_error=0;
 for(size_t n=0;n<raw->nodes_count;++n){const auto &node=raw->nodes[n];if(!node.mesh)continue;check(node.skin&&node.skin->inverse_bind_matrices,"Bound exact-pose export retains actual joint and IBM authority");
  for(size_t p=0;p<node.mesh->primitives_count;++p){const auto &part=node.mesh->primitives[p];auto positions=cgltf_find_accessor(&part,cgltf_attribute_type_position,0),joints=cgltf_find_accessor(&part,cgltf_attribute_type_joints,0),weights=cgltf_find_accessor(&part,cgltf_attribute_type_weights,0);check(positions&&joints&&weights&&positions->count==3,"Bound exact-pose export retains original POSITION JOINTS WEIGHTS");
   for(size_t v=0;v<positions->count;++v){float position[3]{};check(cgltf_accessor_read_float(positions,v,position,3),"Independent bound exact-pose position accessor reads");Vec3 local{position[0],position[1],position[2]};
    for(size_t t=0;t<part.targets_count;++t){float value=node.weights_count>t?node.weights[t]:node.mesh->weights_count>t?node.mesh->weights[t]:0;for(size_t a=0;a<part.targets[t].attributes_count;++a)if(part.targets[t].attributes[a].type==cgltf_attribute_type_position){float delta[3]{};check(cgltf_accessor_read_float(part.targets[t].attributes[a].data,v,delta,3),"Independent bound exact-pose morph accessor reads");local+=Vec3{delta[0],delta[1],delta[2]}*value;}}
    unsigned indices[4]{};float values[4]{};check(cgltf_accessor_read_uint(joints,v,indices,4)&&cgltf_accessor_read_float(weights,v,values,4),"Independent bound exact-pose joint/weight accessors read");Vec3 actual{},wrong{};
    for(size_t k=0;k<4;++k)if(values[k]>0){check(indices[k]<node.skin->joints_count,"Bound joint belongs to actual emitted skin");float inverse[16]{},global[16]{};check(cgltf_accessor_read_float(node.skin->inverse_bind_matrices,indices[k],inverse,16),"Bound exact-pose inverse-bind accessor reads");cgltf_node_transform_world(node.skin->joints[indices[k]],global);actual+=parsed_point(global,parsed_point(inverse,local))*values[k];wrong+=parsed_point(global,local)*values[k];}
    const auto reference=point(world,expected.positions[v]);check(length(actual-reference)<3e-5f,"Parsed bound exact-pose skin/morph products equal native world geometry");wrong_error=std::max(wrong_error,length(wrong-reference));++vertices;
   }
  }
 }
 check(vertices==3&&wrong_error>.001f,"Independent bound-pose consumer is sensitive to incorrect omitted inverse binds");
}
}
int main(int argc,char **argv)try{
 const auto output=argc==2?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/("dome-skin-source-pose-"+std::to_string(getpid()));std::filesystem::create_directories(output);
 for(bool child:{false,true}){
  auto s=exact_pose_fixture(child);const auto before=s.encode();const auto paint=s.object(2).painted;
  check(!s.has_affine_placement(1)&&!s.has_affine_placement(2)&&!s.object(1).deformation->skeleton->space_matrix&&!s.object(1).deformation->skeleton->inverse_bind_override,"Exact-pose-only fixture has ordinary object/rest/bind frames");
  check(author_skin(s,2,options()),"Actual skin transfer accepts exact local pose authority under distinct ordinary placements");compare_pose(s);
  check(s.object(2).deformation->pose[child?1:0].full_local_matrix==s.object(1).deformation->pose[child?1:0].full_local_matrix,"Copied full local pose is byte-exact instead of modifying ignored editable TRS references");
  check(s.object(2).painted==paint&&s.encode()["objects"][0]==before["objects"][0],"Full-pose binding preserves source and target paint authority");
  auto encoded=s.encode();check(encoded["version"]==25&&Scene::decode(encoded).encode()==encoded,"Bound exact pose and morph data restart exactly through conditional Scene25");
  verify_export(s,output/(child?"child-pose.glb":"root-pose.glb"));
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
