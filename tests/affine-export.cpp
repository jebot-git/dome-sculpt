// SPDX-License-Identifier: GPL-3.0-or-later
#include "cgltf.h"
#include "dome/source-placement.hpp"
#include "dome/import-image.hpp"
#include "dome/gltf-export.hpp"
#include "affine-matrix.hpp"
#include "import-authoring-fixtures.hpp"
#include <bit>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;}
Vec3 point(const float*m,Vec3 p){return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};}
void near(Vec3 a,AffineVector b,const char*m){check(std::hypot(a.x-b[0],a.y-b[1],a.z-b[2])<3e-6,m);}
std::vector<uint8_t> read(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}
int main()try {
  import_fixture::Fixture fixture;fixture.j["nodes"][0]["scale"]={2,1,-3};
  fixture.j["nodes"][1]["rotation"]={0,0,std::sin(.37f),std::cos(.37f)};
  auto model=std::make_shared<CreatorSourceModel>();model->original_bytes=std::make_shared<const std::vector<uint8_t>>(fixture.bytes());
  model->asset=std::make_shared<const ImportedAsset>(import_glb(*model->original_bytes));
  auto images=std::make_shared<std::vector<std::shared_ptr<const Image>>>();for(const auto&i:model->asset->images)images->push_back(std::make_shared<const Image>(decode_import_image(i)));model->decoded_images=images;
  Scene scene;const auto first=scene.add_group("Reflected parent"),second=scene.add_group("Rotated child",{},first);
  auto node0=std::make_shared<SourceNodePlacement>();node0->model=model;node0->source_node=0;
  auto node1=std::make_shared<SourceNodePlacement>(*node0);node1->source_node=1;
  scene.group(first).source_placement=node0;scene.group(second).source_placement=node1;
  auto raw=AffineTransform{}.matrix();raw[4]=.2;auto edit=std::make_shared<const NativeAffineEdit>(AffineTransform(raw));
  scene.group(first).affine_edit=edit;scene.active().affine_edit=edit;scene.active().group=second;
  const auto&mesh=model->asset->primitives[0].mesh;scene.active().painted=std::make_shared<PaintedMesh>(mesh,64);scene.active().mesh_active=true;
  const auto before=scene.encode();const auto path=std::filesystem::temp_directory_path()/("dome-affine-export-"+std::to_string(getpid())+".glb");
  struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove(p,ec);}}cleanup{path};
  auto verify=[&](bool skinned){
    const auto objects=scene.surfaces();export_glb(objects,path);
    cgltf_options options{};cgltf_data*raw_data=nullptr;
    check(cgltf_parse_file(&options,path.c_str(),&raw_data)==cgltf_result_success,"independent parser accepts affine chain GLB");
    std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw_data,cgltf_free);
    check(cgltf_load_buffers(&options,data.get(),path.c_str())==cgltf_result_success&&cgltf_validate(data.get())==cgltf_result_success,"independent accessor and hierarchy validation succeeds");
    size_t originals=0;cgltf_node*surface=nullptr;
    for(size_t i=0;i<data->nodes_count;++i){auto&node=data->nodes[i];
      if(node.has_matrix){RigMatrix local;std::copy(node.matrix,node.matrix+16,local.begin());check(detail::gltf_trs_matrix(local),"every emitted matrix is independently TRS-decomposable");for(size_t n=0;n<2;++n)if(std::equal(local.begin(),local.end(),model->asset->nodes[n].local.begin()))++originals;}
      if(node.mesh)surface=&node;
    }
    check(originals==2,"each original source local matrix remains an exact separate node");
    check(surface&&surface->mesh,"actual mesh node is reachable");
    const auto&primitive=surface->mesh->primitives[0];auto positions=cgltf_find_accessor(&primitive,cgltf_attribute_type_position,0);
    const auto expected=AffineTransform(raw)*AffineTransform(model->asset->nodes[0].local)*AffineTransform(model->asset->nodes[1].local)*AffineTransform(raw);
    auto evaluated=skinned?scene.active().deformation->preview(mesh):mesh;
    for(size_t v=0;v<mesh.positions.size();++v){float p[3];check(cgltf_accessor_read_float(positions,v,p,3),"independent POSITION reads");Vec3 world;
      if(skinned){check(surface->skin&&surface->parent==nullptr,"skinned surface is detached from placement as glTF requires");world={};for(size_t j=0;j<surface->skin->joints_count;++j){float ibm[16],joint[16];check(cgltf_accessor_read_float(surface->skin->inverse_bind_matrices,j,ibm,16),"authored bind accessor reads");cgltf_node_transform_world(surface->skin->joints[j],joint);world+=point(joint,point(ibm,{p[0],p[1],p[2]}));}}
      else{float w[16];cgltf_node_transform_world(surface,w);world=point(w,{p[0],p[1],p[2]});}
      near(world,expected.point({evaluated.positions[v].x,evaluated.positions[v].y,evaluated.positions[v].z}),"independent world evaluation preserves affine placement and pose");
    }
  };
  verify(false);check(scene.encode()==before,"export never mutates original graph or raw source");
  DeformationState deformation(mesh);deformation.skeleton.emplace();auto&s=*deformation.skeleton;
  Bone bone;bone.name="Authored joint";bone.rest.translation={.03f,-.02f,.01f};bone.rest.scale={1.1f,.7f,1.3f};s.bones.push_back(bone);
  auto inverse=AffineTransform{}.float_matrix();inverse[0]=.9f;inverse[5]=1.2f;inverse[12]=-.02f;s.inverse_bind_override=std::vector<RigMatrix>{inverse};
  deformation.skin=SkinWeights{};deformation.skin->vertices.assign(mesh.positions.size(),{{0,1}});
  deformation.pose={bone.rest};deformation.pose[0].translation.x+=.04f;deformation.pose[0].rotation={std::cos(.1f),0,std::sin(.1f),0};
  scene.active().deformation=std::make_shared<const DeformationState>(deformation);verify(true);
  const auto exported=read(path);size_t observations=0;GltfExportOptions options;options.cancelled=[&]{++observations;return false;};export_glb(scene.surfaces(),path,options);
  for(size_t at=1;at<=observations;++at){size_t count=0;options.cancelled=[&]{return ++count>=at;};bool refused=false;try{export_glb(scene.surfaces(),path,options);}catch(const std::exception&){refused=true;}check(refused&&read(path)==exported,"every affine export cancellation preserves prior complete file");}
  std::cout<<checks<<" affine hierarchy/export checks +"<<observations<<" cancellation boundaries passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
