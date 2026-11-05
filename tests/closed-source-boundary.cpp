// SPDX-License-Identifier: GPL-3.0-or-later
#include "visibility-heat-bind.hpp"
#include <iostream>
using namespace dome;using namespace dome::detail;
int main()try{
 Mesh m;for(float y:{0.f,.2f})for(auto p:{Vec3{-1,0,-1},Vec3{1,0,-1},Vec3{1,0,1},Vec3{-1,0,1}})m.positions.push_back({p.x*.001f,y,p.z*.001f});m.normals.assign(8,{0,0,1});m.indices={0,2,1,0,3,2,4,5,6,4,6,7};for(uint32_t a=0;a<4;++a){auto b=(a+1)%4;m.indices.insert(m.indices.end(),{a,b,a+4,b,b+4,a+4});}
 Skeleton rig;Bone bone;bone.name="Actual crossing finite handle";bone.rest.translation={0,-.1f,0};bone.length=.4f;rig.bones.push_back(bone);BindOptions options;options.maximum_work=500'000'000;
 auto result=visibility_heat_bind(m,rig,options,{},256u*1024u*1024u,{},HeatDomainPolicy::ClosedMaterial);bool full=result.weights.vertices.size()==8;for(auto&row:result.weights.vertices)full&=row.size()==1&&row[0].joint==0&&row[0].weight==1;if(!full)throw std::runtime_error("Required coarse closed facet-interior handle field differs");std::cout<<"PASS actual8-vertex closed facet-interior support complete"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
