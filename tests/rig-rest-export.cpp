// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_affine_chain_main
#include "import-source-chain.cpp"
#undef main
#include "dome/rig-rest.hpp"
int main(int argc,char **argv)try{
 const auto directory=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/("dome-rig-rest-export-"+std::to_string(getpid()));std::filesystem::create_directories(directory);
 Mesh rest;rest.positions={{-.2f,0,0},{.2f,0,0},{0,.5f,0}};rest.normals.assign(3,{0,0,1});rest.uv={{{0,0}},{{1,0}},{{.5f,1}}};rest.indices={0,1,2};
 Skeleton rig;rig.bones={{"root",-1,{{.1f,.2f,0},{},{1.2f,.8f,1}},.3f},{"child",0,{{.15f,.3f,0},{},{.8f,1.1f,1}},.2f},{"sibling",0,{{-.2f,.2f,.05f},{},{1,.9f,1.2f}},.2f}};rig.bones[1].rest_matrix=RigMatrix{1,0,0,0,.25f,1,0,0,0,.1f,1,0,.15f,.3f,0,1};rig.space_matrix=RigMatrix{-1,0,0,0,.2f,1,0,0,0,.1f,1.1f,0,.2f,-.1f,0,1};
 std::vector<JointTransform> pose;for(const auto &b:rig.bones)pose.push_back(b.rest);pose[1].full_local_matrix=RigMatrix{.9f,.1f,0,0,.35f,1.2f,0,0,.1f,.2f,1,0,.3f,.4f,.05f,1};
 AffineTransform world(std::array<double,16>{-1.1,0,0,0,.4,.9,0,0,.1,.2,1.3,0,1,2,-.5,1});auto old_head=(world*AffineTransform(rig.world()[1])).point({0,0,0});RigRestJointEdit edit;edit.joint=1;edit.parent=2;edit.world_position=Vec3{float(old_head[0]+.07),float(old_head[1]-.03),float(old_head[2]+.02)};auto transported=transport_rig_rest(rig,pose,world,std::span<const RigRestJointEdit>(&edit,1));
 Scene scene;auto &object=scene.active();object.mesh_active=true;object.painted=std::make_shared<PaintedMesh>(rest,64);object.affine_edit=std::make_shared<const NativeAffineEdit>(world);DeformationState d(rest);d.skeleton=transported.skeleton;d.pose=transported.pose;d.skin=SkinWeights{{{{0,1}},{{1,1}},{{1,.5f},{2,.5f}}}};scene.set_deformation(1,d);const auto encoded=scene.encode();
 {std::ofstream output(directory/"deformation2.json");output<<d.encode(rest);if(!output)throw std::runtime_error("Unable to retain actual Deformation2 writer output");auto legacy=d;for(auto &joint:legacy.pose)joint.full_local_matrix.reset();std::ofstream old(directory/"deformation1.json");old<<legacy.encode(rest);if(!old)throw std::runtime_error("Unable to retain compatible Deformation1 writer output");}
 const auto file=directory/"rest-reparent-full-pose.glb";export_glb(scene.surfaces(),file);Model consumer(file);cgltf_node *mesh_node=nullptr;for(size_t i=0;i<consumer.data->nodes_count;++i)if(consumer.data->nodes[i].mesh){if(mesh_node)throw std::runtime_error("Unexpected extra model");mesh_node=&consumer.data->nodes[i];}
 check(mesh_node&&mesh_node->skin&&mesh_node->skin->joints_count==3,"Exact affine helpers never invent skin joints");
 const auto preview=d.preview(rest);for(size_t v=0;v<rest.positions.size();++v){auto w=world.point({preview.positions[v].x,preview.positions[v].y,preview.positions[v].z});check(length(evaluated(*mesh_node,v)-Vec3{float(w[0]),float(w[1]),float(w[2])})<2e-5f,"Independent current-pose IBM/corner world oracle");}
 size_t matrices=0;for(size_t n=0;n<consumer.data->nodes_count;++n){const auto &node=consumer.data->nodes[n];if(!node.has_matrix)continue;++matrices;for(int a=0;a<3;++a)for(int b=a+1;b<3;++b){double product=0,aa=0,bb=0;for(int row=0;row<3;++row){double x=node.matrix[a*4+row],y=node.matrix[b*4+row];product+=x*y;aa+=x*x;bb+=y*y;}check(std::abs(product)<=1e-5*std::sqrt(aa*bb),"Every emitted affine factor independently has orthogonal glTF node columns");}}
 check(matrices>5&&scene.encode()==encoded,"Real affine pose/rest export uses verified helper chains and never mutates native authority");std::cout<<checks<<" independent exact-rest GLB consumer checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
