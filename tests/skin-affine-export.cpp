// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_skin_affine_test
#include "skin-affine.cpp"
#undef main
#include "cgltf.h"
namespace {
Vec3 parsed_point(const float *m,Vec3 p){return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};}
}
int main(int argc,char **argv)try{
 if(argc!=2)return 2;std::filesystem::create_directories(argv[1]);float maximum=0;
 for(int bind:{0,1,2})for(bool reflected:{false,true}){
  auto scene=fixture(bind,reflected);author_skin(scene,2,options());scene.object(1).visible=false;
  auto path=std::filesystem::path(argv[1])/(std::to_string(bind)+(reflected?"-reflected.glb":"-preserving.glb"));
  auto expected=scene.active().deformation->preview(scene.active().painted->mesh());const auto world=scene.world_matrix(2);
  Document d;d.replace(scene);d.export_asset(path).get();
  cgltf_options opts{};cgltf_data *raw=nullptr;check(cgltf_parse_file(&opts,path.c_str(),&raw)==cgltf_result_success,"Independent cgltf reads actual affine skin export");std::unique_ptr<cgltf_data,decltype(&cgltf_free)> model(raw,cgltf_free);
  check(cgltf_load_buffers(&opts,raw,path.c_str())==cgltf_result_success&&cgltf_validate(raw)==cgltf_result_success,"Independent skin/accessor/hierarchy validation passes");size_t vertices=0;float wrong_error=0;
  for(size_t n=0;n<raw->nodes_count;++n){auto &node=raw->nodes[n];if(!node.mesh)continue;check(node.skin&&node.skin->inverse_bind_matrices,"Export retains real skin and explicit inverse binds");
   for(size_t part=0;part<node.mesh->primitives_count;++part){const auto &p=node.mesh->primitives[part];auto positions=cgltf_find_accessor(&p,cgltf_attribute_type_position,0);auto joints=cgltf_find_accessor(&p,cgltf_attribute_type_joints,0);auto weights=cgltf_find_accessor(&p,cgltf_attribute_type_weights,0);check(positions&&joints&&weights&&positions->count==3,"Actual POSITION/JOINTS/WEIGHTS attributes retain three original vertices");
    for(size_t v=0;v<positions->count;++v){float pos[3]{};check(cgltf_accessor_read_float(positions,v,pos,3),"Independent POSITION accessor reads");Vec3 point{pos[0],pos[1],pos[2]};
     for(size_t t=0;t<p.targets_count;++t){float w=node.weights_count>t?node.weights[t]:node.mesh->weights_count>t?node.mesh->weights[t]:0;for(size_t a=0;a<p.targets[t].attributes_count;++a)if(p.targets[t].attributes[a].type==cgltf_attribute_type_position){float delta[3]{};check(cgltf_accessor_read_float(p.targets[t].attributes[a].data,v,delta,3),"Independent active morph accessor reads");point+=Vec3{delta[0],delta[1],delta[2]}*w;}}
     unsigned joint[4]{};float weight[4]{};check(cgltf_accessor_read_uint(joints,v,joint,4)&&cgltf_accessor_read_float(weights,v,weight,4),"Independent joint indices and normalized weights read");Vec3 actual{},wrong{};
     for(size_t i=0;i<4;++i)if(weight[i]>0){check(joint[i]<node.skin->joints_count,"Joint index belongs to actual emitted skin");float inverse[16]{},global[16]{};check(cgltf_accessor_read_float(node.skin->inverse_bind_matrices,joint[i],inverse,16),"Independent authored inverse-bind accessor reads");cgltf_node_transform_world(node.skin->joints[joint[i]],global);actual+=parsed_point(global,parsed_point(inverse,point))*weight[i];wrong+=parsed_point(global,point)*weight[i];}
     auto reference=::point(world,expected.positions[v]);float error=length(actual-reference);maximum=std::max(maximum,error);wrong_error=std::max(wrong_error,length(wrong-reference));check(error<3e-5f,"Parsed glTF bind/pose/morph/world products equal native authored geometry");++vertices;
    }
   }
  }
  check(vertices==3&&wrong_error>.001f,"Wrong missing-IBM consumer is independently sensitive to this fixture");
 }
 std::cout<<"PASS "<<checks<<" independent affine skin export checks; max_position_error="<<maximum<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
