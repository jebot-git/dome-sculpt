// SPDX-License-Identifier: GPL-3.0-or-later
#define main base_visibility_consumer
#include "automatic-visibility-audit.cpp"
#undef main
#include "skin-affine.hpp"
#include "visibility-geodesic-bind.hpp"
using namespace dome::detail;
namespace {size_t checks=0;void check(bool v,const char*m){if(!v)throw std::runtime_error(m);++checks;std::cout<<"PASS "<<m<<std::endl;}}
namespace {
Mesh connected_u(){
 const std::array<float,4>x{-.001f,.001f,.002f,.004f};const std::array<float,3>y{0,.01f,.2f};const std::array<float,2>z{-.001f,.001f};
 Mesh m;std::map<std::array<int,3>,uint32_t>vertices;
 auto filled=[](int a,int b,int c){return a>=0&&a<3&&b>=0&&b<2&&c==0&&(b==0||a!=1);};
 auto vertex=[&](std::array<int,3>p){auto[it,created]=vertices.emplace(p,uint32_t(m.positions.size()));if(created){m.positions.push_back({x[p[0]],y[p[1]],z[p[2]]});m.normals.push_back({0,0,1});}return it->second;};
 const std::array<std::array<std::array<int,3>,4>,6>faces{{{{{0,0,0},{0,0,1},{0,1,1},{0,1,0}}},{{{1,0,0},{1,1,0},{1,1,1},{1,0,1}}},{{{0,0,0},{1,0,0},{1,0,1},{0,0,1}}},{{{0,1,0},{0,1,1},{1,1,1},{1,1,0}}},{{{0,0,0},{0,1,0},{1,1,0},{1,0,0}}},{{{0,0,1},{1,0,1},{1,1,1},{0,1,1}}}}};
 const std::array<std::array<int,3>,6>directions{{{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}}};
 for(int a=0;a<3;++a)for(int b=0;b<2;++b)if(filled(a,b,0))for(size_t f=0;f<6;++f)if(!filled(a+directions[f][0],b+directions[f][1],directions[f][2])){std::array<uint32_t,4>v;for(size_t k=0;k<4;++k)v[k]=vertex({a+faces[f][k][0],b+faces[f][k][1],faces[f][k][2]});m.indices.insert(m.indices.end(),{v[0],v[1],v[2],v[0],v[2],v[3]});}
 m.validate();return m;
}
}
int main(int argc,char**argv)try{
 if(argc!=2)return 2;std::filesystem::create_directories(argv[1]);const auto policy=VisibilitySeedPolicy::ExactNearestSurfaceRows;
 auto u=connected_u();Skeleton rig;for(float x:{0.f,.003f}){Bone b;b.name=rig.bones.empty()?"Left actual arm":"Right actual arm";b.rest.translation={x,0,0};b.length=.2f;rig.bones.push_back(b);}auto field=visibility_geodesic_bind(u,rig,{}, {},256u*1024u*1024u,policy);
 auto extended=rig;Bone irrelevant;irrelevant.name="Unreferenced far control";irrelevant.rest.translation={100,0,0};irrelevant.length=100;extended.bones.push_back(irrelevant);auto added=visibility_geodesic_bind(u,extended,{}, {},256u*1024u*1024u,policy);
 double delta=0,foreign=0;for(size_t v=0;v<u.positions.size();++v){for(uint32_t j=0;j<2;++j)delta=std::max(delta,std::abs(influence(field.weights.vertices[v],j)-influence(added.weights.vertices[v],j)));foreign=std::max(foreign,influence(added.weights.vertices[v],2));}
 std::ofstream(std::filesystem::path(argv[1])/"unused-control.json")<<nlohmann::json{{"vertices",u.positions.size()},{"maximum_field_delta",delta},{"unused_control_weight",foreign},{"base",field.weights.encode()},{"extended",added.weights.encode()}}.dump(2);check(foreign==0,"Unused far control has no accepted source field");check(delta==0,"Unseeded control cannot change regularization of the actual selected source field");
 Mesh thin;box(thin,0);box(thin,.003f);auto thin_field=visibility_geodesic_bind(thin,rig,{}, {},256u*1024u*1024u,policy);
 const std::array<double,16>a{-2,0,0,0,.5,1.5,0,0,0,.25,.75,0,16,-8,4,1};const AffineTransform affine(a);auto changed=thin;for(auto&p:changed.positions){auto q=affine.point({p.x,p.y,p.z});p={float(q[0]),float(q[1]),float(q[2])};}for(auto&n:changed.normals){auto q=affine.normal({n.x,n.y,n.z});n={float(q[0]),float(q[1]),float(q[2])};}auto placed=rig;placed.space_matrix=affine.float_matrix();auto posed=visibility_geodesic_bind(changed,placed,{}, {},256u*1024u*1024u,policy);check(posed.weights.encode()==thin_field.weights.encode(),"Full affine reflection/shear/nonuniform metric retains independent thin-limb fields");
 std::cout<<"PASS visibility_stress_checks="<<checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
