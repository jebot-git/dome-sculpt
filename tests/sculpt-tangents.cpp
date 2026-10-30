// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/tangents.hpp"
#include "dome/mesh-authoring.hpp"
#include "mikktspace.h"
#include <cstring>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
template<class F> bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
bool exact(const Mesh&a,const Mesh&b){return a.indices==b.indices&&a.uv==b.uv&&a.uv_pins==b.uv_pins&&a.uv_seams==b.uv_seams&&a.positions.size()==b.positions.size()&&a.normals.size()==b.normals.size()&&!std::memcmp(a.positions.data(),b.positions.data(),a.positions.size()*sizeof(Vec3))&&!std::memcmp(a.normals.data(),b.normals.data(),a.normals.size()*sizeof(Vec3));}
std::vector<Tangent> original_mikk(const Mesh &mesh){
  struct Data{const Mesh&m;std::vector<Tangent> result;}d{mesh,std::vector<Tangent>(mesh.indices.size())};SMikkTSpaceInterface a{};
  a.m_getNumFaces=[](auto*c){return int(static_cast<Data*>(c->m_pUserData)->m.indices.size()/3);};a.m_getNumVerticesOfFace=[](auto*,int){return 3;};
  a.m_getPosition=[](auto*c,float*out,int f,int v){auto &m=static_cast<Data*>(c->m_pUserData)->m;auto p=m.positions[m.indices[f*3+v]];out[0]=p.x;out[1]=p.y;out[2]=p.z;};
  a.m_getNormal=[](auto*c,float*out,int f,int v){auto &m=static_cast<Data*>(c->m_pUserData)->m;auto p=normalized(m.normals[m.indices[f*3+v]]);out[0]=p.x;out[1]=p.y;out[2]=p.z;};
  a.m_getTexCoord=[](auto*c,float*out,int f,int v){auto &m=static_cast<Data*>(c->m_pUserData)->m;auto p=m.uv[m.indices[f*3+v]];out[0]=p[0];out[1]=p[1];};
  a.m_setTSpaceBasic=[](auto*c,const float*t,float w,int f,int v){static_cast<Data*>(c->m_pUserData)->result[f*3+v]={t[0],t[1],t[2],w};};SMikkTSpaceContext context{&a,&d};if(!genTangSpaceDefault(&context))throw std::runtime_error("Original upstream fixture failed");return d.result;
}
Mesh triangle(float height=1,float scale=1,bool mirrored=false,Quat rotation={}){Mesh m;m.positions={rotation.rotate({0,0,0}),rotation.rotate({scale,0,0}),rotation.rotate({0,scale*height,0})};m.normals.assign(3,rotation.rotate({0,0,1}));m.indices={0,1,2};m.uv={{{0,0}},{{mirrored?-1.f:1.f,0}},{{0,1}}};return m;}
}
int main()try {
  size_t zero_original=0,compared=0;
  for(bool mirror:{false,true})for(auto rotation:{Quat{},Quat{.9238795f,.2209424f,.2209424f,.2209424f}.unit()}){
    auto ordinary=triangle(.4f,1,mirror,rotation);auto old=original_mikk(ordinary),now=tangent_corners(ordinary);
    check(old==now,"Valid original Mikk frames remain bit-identical including mirrored and rotated charts");
    for(float scale:{.0001f,1.f,10000.f})for(float height:{.00001f,.000001f,.0000001f}) {
      auto m=triangle(height,scale,mirror,rotation);auto before=m;auto raw=original_mikk(m);auto recovered=tangent_corners(m);
      for(auto t:raw)zero_original+=length({t[0],t[1],t[2]})<.5f;
      auto expected=rotation.rotate({mirror?-1.f:1.f,0,0});
      for(auto t:recovered){if(std::abs(length({t[0],t[1],t[2]})-1)>1e-5f||dot(Vec3{t[0],t[1],t[2]},expected)<.99999f||t[3]!=(mirror?-1.f:1.f))throw std::runtime_error("Recovered frame differs from independent planar UV derivative");++compared;}
      if(!exact(m,before))throw std::runtime_error("Tangent recovery changed source geometry, normals or UVs");
    }
  }
  check(zero_original>0&&compared==108,"Analytic acute corners across scales, rotations and mirrors recover finite signed UV frames where upstream returned zeros");
  auto impossible=triangle();impossible.normals.assign(3,{1,0,0});auto immutable=impossible;
  check(refuses([&]{tangent_corners(impossible);})&&exact(impossible,immutable),"Intrinsically singular authored normal/UV frames still refuse without fabricating source directions");
  auto generated=triangle();generated.uv.clear();generated.normals[2]={1,0,0};auto source=generated;
  check(condition_sculpt_normals(generated)==1&&generated.positions.size()==6,"New sculpt preparation splits an inconsistent generated-normal face into geometric flat corners");
  bool retained=true;for(size_t i=0;i<source.indices.size();++i){auto p=source.positions[source.indices[i]],q=generated.positions[generated.indices[i]];retained&=p.x==q.x&&p.y==q.y&&p.z==q.z&&generated.normals[generated.indices[i]].z==1;}
  check(retained&&generated.indices.size()==source.indices.size(),"Conditioning retains every exact position, triangle and winding without a geometric area cutoff");
  auto already=triangle();already.uv.clear();auto original=already;auto pointer=already.positions.data();
  check(condition_sculpt_normals(already)==0&&exact(already,original)&&already.positions.data()==pointer,"Consistent sculpt geometry is an exact allocation-free no-op");
  auto authored=triangle();auto saved=authored;check(refuses([&]{condition_sculpt_normals(authored);})&&exact(authored,saved),"Sculpt normal conditioning refuses already authored UV/paint identities");
  size_t callbacks=0;auto probe=source;condition_sculpt_normals(probe,[&]{++callbacks;return false;});
  for(size_t boundary=0;boundary<callbacks;++boundary){auto candidate=source;size_t at=0;if(!refuses([&]{condition_sculpt_normals(candidate,[&]{return at++==boundary;});})||!exact(candidate,source))throw std::runtime_error("Conditioning cancellation published a partial mesh");}
  check(true,"Every observed normal-preparation cancellation boundary retains complete source geometry");
  std::cout<<checks<<" sculpt tangent checks passed; analytic corners="<<compared<<", original zero corners="<<zero_original<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
