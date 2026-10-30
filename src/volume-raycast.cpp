// SPDX-License-Identifier: GPL-3.0-or-later
#include "volume-raycast.hpp"
#include <algorithm>
#include <array>
#include <limits>
namespace dome::sculpt_detail {
namespace {
// Along a ray inside one cell a trilinear scalar field is cubic. Derivative
// roots partition it into monotone intervals, so endpoint bisection does not
// jump over a thin crossing or tangency. No signed-distance assumption is used.
std::optional<double> first_crossing(const std::array<double,4> &c) {
  auto value=[&](double t){return ((c[3]*t+c[2])*t+c[1])*t+c[0];};
  double scale=0;for(auto coefficient:c)scale+=std::abs(coefficient);
  const double epsilon=scale*std::numeric_limits<double>::epsilon()*64;
  std::array<double,4> cuts{0,1,0,0};size_t count=2;
  auto add=[&](double t){if(std::isfinite(t) && t>0 && t<1)cuts[count++]=t;};
  double a=3*c[3],b=2*c[2],d=c[1];
  if(a==0) {if(b!=0)add(-d/b);}
  else {
    double discriminant=b*b-4*a*d;
    double roundoff=(b*b+std::abs(4*a*d))*std::numeric_limits<double>::epsilon()*16;
    if(discriminant>=-roundoff) {
      double root=std::sqrt(std::max(0.,discriminant));
      double q=-.5*(b+std::copysign(root,b));
      if(q!=0){add(q/a);add(d/q);}else add(-b/(2*a));
    }
  }
  std::sort(cuts.begin(),cuts.begin()+count);
  if(value(0)<=epsilon)return 0;
  double previous=0;
  for(size_t i=1;i<count;++i) {
    double next=cuts[i];
    if(value(next)<=epsilon) {
      double lo=previous,hi=next;
      for(int step=0;step<48;++step) {
        double middle=lo+(hi-lo)*.5;
        if(value(middle)<=epsilon)hi=middle;else lo=middle;
      }
      return hi;
    }
    previous=next;
  }
  return {};
}
}
std::optional<Vec3> raycast_field(const Volume &field,Vec3 origin,Vec3 direction,float maximum) {
  float magnitude=length(direction);
  if(!finite(origin) || !finite(direction) || !std::isfinite(magnitude) || magnitude<1e-8f ||
      !std::isfinite(maximum) || maximum<=0 || maximum>1000 ||
      std::max({std::abs(origin.x),std::abs(origin.y),std::abs(origin.z)})+maximum>1000)
    throw std::invalid_argument("Invalid sculpt ray");
  if(field.voxels().empty())return {};
  direction=direction/magnitude;
  const double voxel=field.voxel_size();
  // The authoring pick follows the trilinear field, with the established
  // positive tolerance. A narrow physical band must never make background a hit.
  const double threshold=std::min(voxel*.12,double(field.band())*.1);
  const double start[]={origin.x,origin.y,origin.z},ray[]={direction.x,direction.y,direction.z};
  int cell[3],step[3];double boundary[3],delta[3];
  for(int k=0;k<3;++k) {
    double lattice=start[k]/voxel;cell[k]=int(std::floor(lattice));
    step[k]=ray[k]>0?1:ray[k]<0?-1:0;
    if(step[k]<0 && lattice==cell[k])--cell[k];
    if(!step[k]) {boundary[k]=delta[k]=INFINITY;continue;}
    boundary[k]=((double(cell[k])+(step[k]>0?1:0))*voxel-start[k])/ray[k];
    delta[k]=voxel/std::abs(ray[k]);
  }
  double begin=0;
  constexpr unsigned maximum_cells=20'000;
  for(unsigned visited=0;visited<maximum_cells;++visited) {
    double end=std::min({double(maximum),boundary[0],boundary[1],boundary[2]});
    if(end<begin)throw std::runtime_error("Sculpt ray grid traversal lost numeric ordering");
    std::array<double,8> distances{};double minimum=INFINITY;
    for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x) {
      auto &distance=distances[size_t(z*4+y*2+x)];distance=field.at({cell[0]+x,cell[1]+y,cell[2]+z});minimum=std::min(minimum,distance);
    }
    if(minimum<=threshold) {
      double fraction[3],motion[3];
      for(int k=0;k<3;++k) {
        fraction[k]=std::clamp((start[k]+ray[k]*begin)/voxel-cell[k],0.,1.);
        motion[k]=ray[k]*(end-begin)/voxel;
      }
      std::array<double,4> polynomial{};
      for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x) {
        std::array<double,4> term{distances[size_t(z*4+y*2+x)]-threshold,0,0,0};int corner[]={x,y,z};
        for(int k=0;k<3;++k) {
          const double base=corner[k]?fraction[k]:1-fraction[k],slope=corner[k]?motion[k]:-motion[k];
          std::array<double,4> product{};
          for(int power=0;power<=k;++power){product[power]+=term[power]*base;product[power+1]+=term[power]*slope;}
          term=product;
        }
        for(int power=0;power<4;++power)polynomial[power]+=term[power];
      }
      if(auto root=first_crossing(polynomial)) {
        double t=begin+*root*(end-begin);
        return Vec3{float(start[0]+ray[0]*t),float(start[1]+ray[1]*t),float(start[2]+ray[2]*t)};
      }
    }
    if(end>=maximum)return {};
    for(int k=0;k<3;++k)if(boundary[k]<=end){cell[k]+=step[k];boundary[k]+=delta[k];}
    begin=end;
  }
  throw std::length_error("Sculpt ray exceeds the bounded grid traversal budget at this resolution");
}
} // namespace dome::sculpt_detail
