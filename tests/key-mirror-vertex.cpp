// SPDX-License-Identifier: GPL-3.0-or-later
#include "key-mirror-correspondence.hpp"
#include <algorithm>
#include <iostream>
#include <numeric>
#include <limits>
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;}
template<class F>bool rejects(F action){try{action();return false;}catch(const std::exception&){return true;}}
Mesh square(){Mesh m;m.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
bool equal(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
}
int main()try{
 const std::array<Vec3,4> values{{{.1f,.2f,.3f},{.4f,.5f,.6f},{.7f,.8f,.9f},{1,1.1f,1.2f}}};
 const std::array<size_t,4> partner{1,0,3,2};
 for(float scale:{.0001f,1.f,10000.f})for(float plane:{0.f,.125f})for(bool diagonal:{false,true}){
  std::array<size_t,4> order{0,1,2,3};
  do{
   auto input=square();const auto original=input;std::array<uint32_t,4> inverse;std::vector<Vec3> delta;
   for(size_t v=0;v<4;++v){inverse[order[v]]=uint32_t(v);const auto p=original.positions[order[v]];input.positions[v]={p.x*scale+plane,p.y*scale,p.z*scale};input.uv[v]=original.uv[order[v]];delta.push_back(values[order[v]]);}
   input.indices=diagonal?std::vector<uint32_t>{0,1,3,1,2,3}:original.indices;for(auto &v:input.indices)v=inverse[v];
   const auto result=detail::reflected_key_vertex_deltas(input,delta,{plane},scale<1?1e-7f:.0001f);
   check(result.size()==4,"Complete native vertex displacement field is retained");
   for(size_t v=0;v<4;++v){const auto wanted=values[partner[order[v]]];check(equal(result[v],{-wanted.x,wanted.y,wanted.z}),"Independent index oracle survives opposite diagonals, permutations, scale and off-center planes");}
  }while(std::next_permutation(order.begin(),order.end()));
 }
 auto mesh=square();std::vector<Vec3> delta(values.begin(),values.end());
 check(rejects([&]{detail::reflected_key_correspondence(mesh,.0001f);}),"Opposite reflected diagonals retain the exact authored-corner refusal");
 mesh.positions.push_back(mesh.positions[0]);mesh.normals.push_back(mesh.normals[0]);mesh.uv.push_back(mesh.uv[0]);delta.push_back(delta[0]);
 auto result=detail::reflected_key_vertex_deltas(mesh,delta,{},.0001f);check(result.size()==5&&equal(result[0],result[4]),"Coincident seam vertices with identical source displacement remain separate and consistent");
 delta.back().z+=.001f;check(rejects([&]{detail::reflected_key_vertex_deltas(mesh,delta,{},.0001f);}),"Conflicting coincident seam displacement refuses instead of choosing or averaging a vertex");delta.back()=delta[0];
 delta.back().z=std::numeric_limits<float>::infinity();check(rejects([&]{detail::reflected_key_vertex_deltas(mesh,delta,{},.0001f);}),"Nonfinite input displacement refuses before spatial publication");delta.back()=delta[0];
 for(int choice=0;choice<3;++choice){detail::KeyMirrorLimits limits;if(choice==0)limits.max_bytes=1;if(choice==1)limits.max_work=1;if(choice==2)limits.cancelled=[]{return true;};check(rejects([&]{detail::reflected_key_vertex_deltas(mesh,delta,{},.0001f,limits);}),"Actual scratch, work and cancellation controls refuse immutable input");}
 mesh.positions[1].x+=.01f;check(rejects([&]{detail::reflected_key_vertex_deltas(mesh,delta,{},.0001f);}),"Missing reflected geometry refuses within the explicit local tolerance");
 std::cout<<"PASS "<<checks<<" vertex-position Mirror oracle checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
