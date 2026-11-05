// SPDX-License-Identifier: GPL-3.0-or-later
// Diagnostic before changing production binding. No artist values enter bind.
#include "dome/skin-authoring.hpp"
#include "dome/humanoid-creator.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
void box(Mesh &m,float center) {
 const auto offset=uint32_t(m.positions.size());
 for(float y:{0.f,.02f,.18f,.2f})for(auto q:{Vec3{-1,0,-1},Vec3{1,0,-1},Vec3{1,0,1},Vec3{-1,0,1}})m.positions.push_back({center+q.x*.001f,y,q.z*.001f});
 const std::array<uint32_t,6>bottom{0,2,1,0,3,2},top{12,13,14,12,14,15};
 for(auto t:bottom)m.indices.push_back(offset+t);for(auto t:top)m.indices.push_back(offset+t);
 for(uint32_t row=0;row<3;++row)for(uint32_t side=0;side<4;++side){auto a=offset+row*4+side,b=offset+row*4+(side+1)%4,c=a+4,d=b+4;m.indices.insert(m.indices.end(),{a,b,c,b,d,c});}
 m.normals.assign(m.positions.size(),{0,0,1});
}
double influence(const VertexWeights &row,uint32_t joint){for(auto x:row)if(x.joint==joint)return x.weight;return 0;}
}
int main(int argc,char**argv)try{
 if(argc!=2&&argc!=4)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 Mesh mesh;box(mesh,0);box(mesh,.003f);Skeleton rig;for(float x:{0.f,.003f}){Bone b;b.name=rig.bones.empty()?"Authored left thin limb":"Authored right thin limb";b.rest.translation={x,.02f,0};b.length=.16f;rig.bones.push_back(b);}auto options=BindOptions{};auto weights=automatic_bind(mesh,rig,options);std::vector<JointTransform>pose;for(const auto&b:rig.bones)pose.push_back(b.rest);pose[0].translation.x+=.1f;pose[1].translation.x-=.1f;auto actual=deform_mesh(mesh,weights,rig,pose);double maximum=0,mean=0,wrong=0;
 for(size_t v=0;v<mesh.positions.size();++v){auto expected=mesh.positions[v];expected.x+=v<16?.1f:-.1f;double e=length(actual.positions[v]-expected);maximum=std::max(maximum,e);mean+=e;wrong=std::max(wrong,influence(weights.vertices[v],v<16?1:0));}mean/=mesh.positions.size();nlohmann::json report{{"fixture","two disconnected closed2mm limbs with1mm gap, genuine independent authored bone segments"},{"vertices",mesh.positions.size()},{"maximum_foreign_weight",wrong},{"max_pose_error_m",maximum},{"mean_pose_error_m",mean},{"height_m",.2},{"max_fraction",maximum/.2},{"mean_fraction",mean/.2},{"max_bound",.03},{"mean_bound",.01},{"pass",maximum<=.006&&mean<=.002},{"weights",weights.encode()}};std::ofstream(out/"thin-native.json")<<report.dump(2);std::cout<<report.dump(2)<<std::endl;
 if(argc==4){auto scene=Scene::load(argv[2]);uint64_t reference=0;for(const auto&o:scene.objects())if(o.deformation&&o.deformation->skeleton){if(reference)throw std::runtime_error("Unique actual fitted reference needed");reference=o.id;}if(!reference)throw std::runtime_error("Actual fitted reference needed");const auto appearance=scene.object(reference).appearance;if(!appearance||!appearance->asset)throw std::runtime_error("Genuine retained source asset required");std::vector<uint64_t>ids;for(const auto&o:scene.objects())if(o.mesh_active&&o.painted&&o.appearance&&o.appearance->asset.get()==appearance->asset.get()&&!o.appearance->asset.owner_before(appearance->asset)&&!appearance->asset.owner_before(o.appearance->asset))ids.push_back(o.id);if(ids.size()!=19)throw std::runtime_error("Diagnostic fixture must retain all nineteen actual repository source parts");SkinAuthoringOptions bind;bind.reference=reference;bind.mode=SkinAuthoringMode::Automatic;for(auto id:ids){std::cout<<"LEGACY_AUTO object="<<id<<std::endl;author_skin(scene,id,bind);}scene.save(argv[3]);std::cout<<"PASS actual unskinned CC0 ordinary Automatic outputs="<<ids.size()<<std::endl;}
 return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
