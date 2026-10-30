// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh-processing.hpp"
#include <iostream>
using namespace dome;
namespace {
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
Mesh triangle(Vec3 origin,float scale) {
 Mesh mesh;mesh.positions={origin,origin+Vec3{scale,0,0},origin+Vec3{0,scale,0}};
 mesh.normals.assign(3,{0,0,1});mesh.indices={0,1,2};return mesh;
}
}
int main()try {
 for(auto mesh:{triangle({},1e-7f),triangle({80,-70,60},.00001f),triangle({-40,-30,20},30)}) {
  mesh.validate();auto report=inspect_mesh(mesh);
  check(report.degenerate_triangles==0 && report.boundary_edges==3,"Tiny, translated and large noncollinear triangles retain geometric validity");
  auto clean=cleanup_mesh(mesh);check(clean.removed_degenerate==0 && clean.mesh.positions.size()==3 && clean.mesh.indices==mesh.indices,"Cleanup never discards valid faces solely because of world area");
 }
 auto invalid=triangle({},1e-7f);invalid.positions[2]={2e-7f,0,0};
 check(inspect_mesh(invalid).degenerate_triangles==1,"Actual collinearity remains degenerate at tiny scales");
 invalid.positions[2]=invalid.positions[1];check(inspect_mesh(invalid).degenerate_triangles==1,"Coincident positions remain degenerate");
 bool refused=false;try{invalid.validate();}catch(const std::exception &){refused=true;}
 check(refused,"Topology inspector and canonical mesh validator agree on true degeneracy");
 return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
