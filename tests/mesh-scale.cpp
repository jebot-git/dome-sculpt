// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh.hpp"
#include <iostream>
#include <map>
using namespace dome;
namespace {
void check(bool condition,const char *message) {
  if(!condition)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';
}
template<class F>bool rejects(F f){try{f();return false;}catch(...){return true;}}
bool closed(const Mesh &m) {
  std::map<std::pair<uint32_t,uint32_t>,int> edges;
  for(size_t i=0;i<m.indices.size();i+=3)for(int k=0;k<3;++k) {
    auto a=m.indices[i+k],b=m.indices[i+(k+1)%3];if(a>b)std::swap(a,b);++edges[{a,b}];
  }
  return !edges.empty() && std::all_of(edges.begin(),edges.end(),[](auto e){return e.second==2;});
}
Volume perturbed_sphere(float voxel) {
  Volume base(voxel);Volume::Voxels samples;
  for(int z=-10;z<=10;++z)for(int y=-10;y<=10;++y)for(int x=-10;x<=10;++x) {
    double a=double(x)*voxel,b=double(y)*voxel,c=double(z)*voxel;
    float d=float(std::sqrt(a*a+b*b+c*c)-7*double(voxel));
    // Model real single-precision backend rounding around lattice vertices,
    // with both signs and exact zeros in one deterministic surface.
    if(std::abs(d)<voxel*1e-5f)d=((std::abs(x)+2*std::abs(y)+3*std::abs(z))%3-1)*voxel*6e-7f;
    samples[{x,y,z}]=std::clamp(d,-base.band(),base.band());
  }
  return Volume::from_samples(voxel,std::move(samples));
}
}
int main()try {
  for(float scale:{1.f,.001f,.00001f,1e-8f,1e-15f}) {
    Mesh mesh;mesh.positions={{0,0,0},{scale,0,0},{0,scale,0}};
    mesh.normals.assign(3,{0,0,1});mesh.indices={0,1,2};mesh.validate();
    mesh.positions[2]={2*scale,0,0};
    check(rejects([&]{mesh.validate();}),"Scale-independent validation retains true triangles and rejects exact collinearity");
    mesh.positions[2]=mesh.positions[0];
    check(rejects([&]{mesh.validate();}),"Coincident triangle vertices remain invalid at every scale");
  }
  for(float voxel:{.05f,.00001f}) {
    auto volume=perturbed_sphere(voxel);auto before=volume.voxels();auto mesh=surface_mesh(volume);
    mesh.validate();
    check(closed(mesh),"Near-zero backend rounding retains every closing face, including10-micrometre voxels");
    double normalized_volume=0;
    for(size_t i=0;i<mesh.indices.size();i+=3) {
      auto a=mesh.positions[mesh.indices[i]],b=mesh.positions[mesh.indices[i+1]],c=mesh.positions[mesh.indices[i+2]];
      normalized_volume+=(double(a.x)*(double(b.y)*c.z-double(b.z)*c.y)+
         double(a.y)*(double(b.z)*c.x-double(b.x)*c.z)+double(a.z)*(double(b.x)*c.y-double(b.y)*c.x))/6;
    }
    normalized_volume/=double(voxel)*voxel*voxel;
    check(normalized_volume>1300 && normalized_volume<1500,"Small closing faces retain outward winding and the expected sphere volume");
    check(volume.voxels()==before,"Meshing preserves every original signed field value exactly");
    check(rejects([&]{surface_mesh(volume,[]{return true;});}),"Cancelled scale-robust meshing publishes no partial surface");
  }
  return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
