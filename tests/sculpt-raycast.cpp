// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/volume.hpp"
#include "dome/mesh.hpp"
#include "ray-corridor.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
Volume physical(float voxel,float band) {
 Volume v(voxel);auto settings=v.sculpt_settings();settings.mask_visible=false;v.sculpt_settings(settings);
 auto json=v.encode();json["version"]=4;json["distance_band"]=band;return Volume::decode(json);
}
void sphere(Volume &v,Vec3 center,float radius) {v.begin_stroke();Brush b;b.radius=radius;b.blend=0;b.from=b.to=center;v.stamp(b);v.end_stroke();}
}
int main()try {
 Volume nominal(.01f);auto wide=physical(.01f,.16f);sphere(nominal,{.8f,.001f,.003f},.021f);sphere(wide,{.8f,.001f,.003f},.021f);
 check(!surface_mesh(wide).indices.empty(),"Wide-band thin fixture has an actual closed extracted surface");
 for(float z:{.557f,.8f,1.13f}) {
  auto a=nominal.raycast({.8f,0,z},{0,0,-1}),b=wide.raycast({.8f,0,z},{0,0,-1});
  check(a && b && std::abs(a->z-b->z)<.0001f,"Ray traversal never skips a representable thin sphere when physical band is wide");
 }
 auto narrow=physical(.1f,.00004f);sphere(narrow,{},.23f);
 check(!narrow.raycast({1,0,1},{0,0,-1},2),"Physical band smaller than voxel never turns positive background into a hit");
 auto n=narrow.raycast({0,0,1},{0,0,-1},2);
 check(n && n->z>.19f && n->z<.31f,"Narrow-band closed geometry is still picked");
 auto far=physical(.001f,.04f);sphere(far,{},.0021f);
 auto hit=far.raycast({0,0,5},{0,0,-1},6);check(hit && hit->z>0 && hit->z<.003f,"Long-distance fine-grid traversal remains bounded and finds thin geometry");
 auto translated=physical(.01f,.16f);sphere(translated,{80,-70,60},.021f);
 hit=translated.raycast({80,-70,60.557f},{0,0,-1},1);
 check(hit && std::abs(hit->z-60.022f)<.005f,"Large translated grid coordinates retain thin geometry hits");
 check(!wide.raycast({.8f,0,1},{0,0,-1},.9f),"Ray maximum clips intersections beyond the requested segment");
 hit=wide.raycast({.8f,0,.003f},{0,0,-1},1);check(hit && std::abs(hit->z-.003f)<1e-7f,"Rays starting inside retain the original immediate-hit policy");
 // Endpoints along the diagonal are positive, but a trilinear field can dip
 // below the picking level inside one cell. Endpoint-only traversal would miss.
 for(bool cubic:{false,true}) {
  Volume::Voxels values;
  for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x) {
    double px=x*.01,py=y*.01,pz=z*.01;
    values[{x,y,z}]=float(cubic?10000*(px-.003)*(py-.007)*(pz+.01):100*(px-.005)*(py-.005));
  }
  auto field=Volume::from_samples(.01f,std::move(values));
  auto ray=normalized(Vec3{1,1,cubic?1.f:0.f});
  auto origin=Vec3{0,0,cubic?0.f:.005f};
  auto found=field.raycast(origin,ray,.016f);check(bool(found),"Interior cubic critical points find two crossings despite equal-sign segment endpoints");
  double lo=0,hi=cubic?.003:.005;
  for(int i=0;i<60;++i) {
    double m=(lo+hi)*.5;
    double value=cubic?10000*(m-.003)*(m-.007)*(m+.01):100*(m-.005)*(m-.005);
    if(value<=.0012)hi=m;else lo=m;
  }
  check(std::abs(found->x-hi)<1e-6,"First trilinear crossing agrees with an independent analytic polynomial oracle");
 }
 Volume empty(.00001f);check(!empty.raycast({0,0,1},{0,0,-1},2),"Empty high-resolution fields reject without whole-field scanning or traversal");
 auto tiny=physical(.00001f,.00004f);sphere(tiny,{},.000021f);
 check(bool(tiny.raycast({0,0,1},{0,0,-1},2)),"Fine-grid picking skips empty camera space and finds actual10um geometry");
 auto dense=test::dense_ray_corridor();bool exhausted=false;try{dense.raycast({0,0,1},{0,0,-1},2);}catch(const std::length_error &){exhausted=true;}
 check(exhausted,"Genuinely dense candidate-cell traversal still reports bounded exhaustion explicitly");
 bool invalid=false;try{nominal.raycast({0,0,1},{INFINITY,0,0},2);}catch(const std::invalid_argument &){invalid=true;}
 check(invalid,"Invalid ray directions are refused explicitly");
 std::cout<<"PASS "<<checks<<" bounded trilinear ray checks\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
