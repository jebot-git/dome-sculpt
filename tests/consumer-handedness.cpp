// SPDX-License-Identifier: GPL-3.0-or-later
#include "consumer-handedness.hpp"
#include <iostream>
using M=std::array<float,16>;using V=std::array<double,3>;
V cross(V a,V b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
double dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
V scale(V a,double s){return {a[0]*s,a[1]*s,a[2]*s};}
V unit(V a){return scale(a,1/std::sqrt(dot(a,a)));}
V sub(V a,V b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
V vector(const M&m,V p){return {m[0]*p[0]+m[4]*p[1]+m[8]*p[2],m[1]*p[0]+m[5]*p[1]+m[9]*p[2],m[2]*p[0]+m[6]*p[1]+m[10]*p[2]};}
size_t checks=0;void check(bool ok,const char*m){if(!ok)throw std::runtime_error(m);++checks;}
int main()try{
 for(float x:{-2.f,2.f})for(float shear:{0.f,.4f})for(float w:{-1.f,1.f}){
  M m{x,0,0,0,shear,3,.2f,0,0,0,.7f,0,0,0,0,1};
  const auto t=unit(vector(m,{1,0,0}));
  // Independently derive the transformed plane normal from its two parametric
  // directions and orient it by the known sign of x (rather than the helper).
  auto n=unit(scale(cross(vector(m,{1,0,0}),vector(m,{0,1,0})),x<0?-1:1));
  auto b=vector(m,{0,w,0});b=unit(sub(b,scale(t,dot(b,t))));
  const auto actual=scale(unit(cross(n,t)),w*consumer_frame::handedness(m));
  check(dot(actual,b)>1-1e-12,"Transformed bitangent orientation must match the independent parametric plane");
  if(x<0)check(dot(scale(unit(cross(n,t)),w),b)<-1+1e-12,"Keeping raw tangent W under world reflection reverses bitangent");
 }
 M singular{};bool rejected=false;try{consumer_frame::handedness(singular);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Singular consumer frames refuse");
 std::cout<<checks<<" independent world-reflection tangent-frame checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
