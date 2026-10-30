// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
#include "dome/gltf-export.hpp"
#include "cgltf.h"
#include "import-skin-fixtures.hpp"
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;}
void near(Vec3 a,Vec3 b,const char*m){check(length(a-b)<8e-6f,m);}
Vec3 point(const float*m,Vec3 p){return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};}
struct Model{
 std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data{nullptr,cgltf_free};
 explicit Model(const std::filesystem::path&p){cgltf_options options{};cgltf_data*raw=nullptr;check(cgltf_parse_file(&options,p.c_str(),&raw)==cgltf_result_success,"Independent parser accepts source/export");data.reset(raw);check(cgltf_load_buffers(&options,raw,p.c_str())==cgltf_result_success&&cgltf_validate(raw)==cgltf_result_success,"Independent hierarchy/accessor validation succeeds");}
};
Vec3 evaluated(cgltf_node &node,size_t v){
 auto &p=node.mesh->primitives[0];auto position=cgltf_find_accessor(&p,cgltf_attribute_type_position,0);float a[3];check(cgltf_accessor_read_float(position,v,a,3),"Independent base POSITION accessor reads");Vec3 value{a[0],a[1],a[2]};
 for(size_t t=0;t<p.targets_count;++t){float w=t<node.weights_count?node.weights[t]:t<node.mesh->weights_count?node.mesh->weights[t]:0;for(size_t i=0;i<p.targets[t].attributes_count;++i)if(p.targets[t].attributes[i].type==cgltf_attribute_type_position){check(cgltf_accessor_read_float(p.targets[t].attributes[i].data,v,a,3),"Independent target POSITION accessor reads");value+=Vec3{a[0],a[1],a[2]}*w;}}
 if(!node.skin){float world[16];cgltf_node_transform_world(&node,world);return point(world,value);}
 auto joints=cgltf_find_accessor(&p,cgltf_attribute_type_joints,0),weights=cgltf_find_accessor(&p,cgltf_attribute_type_weights,0);float j[4],w[4];check(cgltf_accessor_read_float(joints,v,j,4)&&cgltf_accessor_read_float(weights,v,w,4),"Independent skin attributes read");Vec3 result{};
 for(size_t k=0;k<4;++k)if(w[k]!=0){auto slot=size_t(j[k]);check(slot<node.skin->joints_count,"Independent joint slot is bounded");float world[16],ibm[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};if(node.skin->inverse_bind_matrices)check(cgltf_accessor_read_float(node.skin->inverse_bind_matrices,slot,ibm,16),"Independent authored inverse bind reads");cgltf_node_transform_world(node.skin->joints[slot],world);result+=point(world,point(ibm,value))*w[k];}
 return result;
}
void apply_pose(Model &model,const SculptObject &o){
 if(!o.deformation||o.deformation->pose.empty())return;
 for(size_t b=0;b<o.deformation->pose.size();++b){auto &node=model.data->nodes[o.appearance->source_joint_nodes[b]];const auto &pose=o.deformation->pose[b];node.has_matrix=false;node.has_translation=node.has_rotation=node.has_scale=true;node.translation[0]=pose.translation.x;node.translation[1]=pose.translation.y;node.translation[2]=pose.translation.z;node.rotation[0]=pose.rotation.x;node.rotation[1]=pose.rotation.y;node.rotation[2]=pose.rotation.z;node.rotation[3]=pose.rotation.w;node.scale[0]=pose.scale.x;node.scale[1]=pose.scale.y;node.scale[2]=pose.scale.z;}
}
}
int main()try{
 auto home=std::filesystem::temp_directory_path()/("dome-source-chain-"+std::to_string(getpid()));std::filesystem::create_directories(home);setenv("HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);
 struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{home};
 for(int mode=0;mode<3;++mode){
  auto f=mode?import_fixture::skinned(mode==2):import_fixture::Fixture{};f.j["nodes"][0]["scale"]={2,1,-.7f};f.j["nodes"][1]["rotation"]={0,0,std::sin(.31f),std::cos(.31f)};
  if(mode){f.j["nodes"][3]["scale"]={1.2f,.9f,1.1f};f.j["nodes"][4]["scale"]={.7f,1.3f,.8f};f.j["nodes"][4]["rotation"]={0,std::sin(.19f),0,std::cos(.19f)};}
  auto path=home/("source"+std::to_string(mode)+".glb");f.save(path);Scene before;auto unchanged=before.encode();ImportAuthoringOptions options;options.paint_resolution=64;auto prepared=prepare_authoring_import(before,path,options);
  check(before.encode()==unchanged&&prepared.objects.size()==2,"Actual affine preparation remains detached and preserves both instances");
  std::shared_ptr<const CreatorSourceModel> model;
  for(auto &g:prepared.scene.groups())if(g.source_placement){check(g.transform.scale==1&&g.transform.position.x==0&&g.transform.position.y==0&&g.transform.position.z==0&&!g.affine_edit,"Original node local is separate from identity native edit delta");if(!model)model=g.source_placement->model;check(model==g.source_placement->model&&*model->original_bytes==f.bytes(),"Whole affine graph shares one exact immutable model authority");check(g.source_placement->local().matrix()==AffineTransform(model->asset->nodes[g.source_placement->source_node].local).matrix(),"Original local matrix survives without quaternion/scale approximation");}
  check(bool(model)&&prepared.scene.encode()["version"]==13,"Affine original-chain authority selects Scene13");
  for(bool posed:{false,true}){
   if(posed&&!mode)continue;Model reference(path);
   for(auto id:prepared.objects){auto &o=prepared.scene.object(id);o.name="Instance "+std::to_string(o.appearance->source_node);
    if(posed){auto deformation=*o.deformation;deformation.pose.clear();for(const auto &b:deformation.skeleton->bones)deformation.pose.push_back(b.rest);for(size_t b=0;b<o.appearance->source_joint_nodes.size();++b)if(o.appearance->source_joint_nodes[b]==3){deformation.pose[b].rotation={std::cos(.23f),0,0,std::sin(.23f)};deformation.pose[b].scale={.8f,1.4f,1.1f};}prepared.scene.set_deformation(id,deformation);}
    apply_pose(reference,o);auto local=o.deformation?o.deformation->preview(o.painted->mesh()):o.painted->mesh();auto world=prepared.scene.world_matrix(id);
    for(size_t v=0;v<local.positions.size();++v){auto p=local.positions[v];auto q=world.point({p.x,p.y,p.z});near({float(q[0]),float(q[1]),float(q[2])},evaluated(reference.data->nodes[o.appearance->source_node],o.appearance->source_vertices[v]),"Native original/current pose matches independent source world+IBM+weights without double placement");}
   }
   auto encoded=prepared.scene.encode();check(Scene::decode(encoded).encode()==encoded,"Full source/skin/current pose restarts exactly with affine model aliases");
   auto target=home/("export"+std::to_string(mode)+(posed?"posed":"rest")+".glb");export_glb(prepared.scene.surfaces(),target);Model exported(target);size_t surfaces=0;
   for(size_t n=0;n<exported.data->nodes_count;++n){auto &node=exported.data->nodes[n];if(!node.mesh)continue;auto found=std::find_if(prepared.objects.begin(),prepared.objects.end(),[&](auto id){const auto &o=prepared.scene.object(id);return node.mesh->name&&o.name==node.mesh->name;});check(found!=prepared.objects.end(),"Export keeps distinguishable original instances");const auto &o=prepared.scene.object(*found);for(size_t v=0;v<o.painted->mesh().positions.size();++v)near(evaluated(node,v),evaluated(reference.data->nodes[o.appearance->source_node],o.appearance->source_vertices[v]),"Exported full affine/bind/current-pose world matches independent original consumer");++surfaces;}
   check(surfaces==2&&prepared.scene.encode()==encoded,"Actual affine export preserves both instances and leaves source authority untouched");
  }
 }
 std::cout<<checks<<" original-chain affine import/native/export/pose checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
