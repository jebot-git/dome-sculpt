// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "volume-exact.hpp"
#include "dome/mesh.hpp"

namespace dome::detail::volume_topology {
using Point=std::array<float,3>;
using Triangle=std::array<Point,3>;
inline std::array<float,2> project(Point p,int drop){return {p[(drop+1)%3],p[(drop+2)%3]};}
// Exact orientation-sign intersections. Shared structural vertices/edges are
// legal; every other contact, including tangency and coplanar overlap, fails.
// No intersection point is rounded or inserted into the source mesh.
template<class Budget>bool unexpected_contact(const Triangle&a,const Triangle&b,Budget&budget){
  volume_exact::Arithmetic exact(budget);
  for(int axis=0;axis<3;++axis){budget.step();auto alo=std::min({a[0][axis],a[1][axis],a[2][axis]}),ahi=std::max({a[0][axis],a[1][axis],a[2][axis]});auto blo=std::min({b[0][axis],b[1][axis],b[2][axis]}),bhi=std::max({b[0][axis],b[1][axis],b[2][axis]});if(ahi<blo||bhi<alo)return false;}
  auto shared=[&](const Point&p){budget.step(6);return std::find(a.begin(),a.end(),p)!=a.end()&&std::find(b.begin(),b.end(),p)!=b.end();};
  unsigned common=0;for(const auto&p:a)if(shared(p))++common;
  if(common==3)return true; // coincident duplicate faces are not an adjacency
  auto edges=[&](const Triangle&from,const Triangle&target){
    int drop=-1;std::array<std::array<float,2>,3> flat;
    for(int axis=0;axis<3&&drop<0;++axis){for(int k=0;k<3;++k)flat[k]=project(target[k],axis);if(exact.orient2(flat[0],flat[1],flat[2]).sign)drop=axis;}
    if(drop<0)throw std::invalid_argument("Volume topology has a degenerate triangle");
    auto inside=[&](const Point&p){auto q=project(p,drop);int orientation=exact.orient2(flat[0],flat[1],flat[2]).sign;for(int k=0;k<3;++k){int s=exact.orient2(flat[k],flat[(k+1)%3],q).sign;if(s&&s!=orientation)return false;}return true;};
    for(int edge=0;edge<3;++edge){budget.step();auto p=from[edge],q=from[(edge+1)%3];int sp=exact.orient3(target[0],target[1],target[2],p),sq=exact.orient3(target[0],target[1],target[2],q);
      if(sp&&sq&&sp==sq)continue;
      if(!sp&&!sq){
        if(shared(p)&&shared(q))continue; // actual common triangle edge
        volume_exact::Point2 pp,qq;pp.ordinary=qq.ordinary=true;auto u=project(p,drop),v=project(q,drop);pp.free={u[0],u[1]};qq.free={v[0],v[1]};auto overlap=exact.clip(flat,pp,qq);if(!overlap)continue;
        if(exact.compare(overlap->enter,overlap->exit)==0){if(exact.compare(overlap->enter,exact.zero())==0&&shared(p))continue;if(exact.compare(overlap->enter,exact.one())==0&&shared(q))continue;}
        return true;
      }
      if(!sp){if(!shared(p)&&inside(p))return true;continue;}
      if(!sq){if(!shared(q)&&inside(q))return true;continue;}
      int x=exact.orient3(p,q,target[0],target[1]),y=exact.orient3(p,q,target[1],target[2]),z=exact.orient3(p,q,target[2],target[0]);
      if((x>=0&&y>=0&&z>=0)||(x<=0&&y<=0&&z<=0))return true;
    }
    return false;
  };
  return edges(a,b)||edges(b,a);
}
template<class Budget,class Memory>void validate(const Mesh&mesh,Budget&budget,Memory&memory){
  struct Bounds{Point lo,hi;uint32_t triangle;};const size_t count=mesh.indices.size()/3;
  memory.add(count,sizeof(Bounds));memory.add(1,8192);
  std::vector<Bounds> bounds;bounds.reserve(count);Point lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
  volume_exact::Arithmetic exact(budget);
  auto triangle=[&](uint32_t t){Triangle out;for(int k=0;k<3;++k){auto p=mesh.positions[mesh.indices[size_t(t)*3+k]];out[k]={p.x,p.y,p.z};}return out;};
  for(uint32_t t=0;t<count;++t){budget.step();auto p=triangle(t);bool area=false;for(int axis=0;axis<3&&!area;++axis)area=exact.orient2(project(p[0],axis),project(p[1],axis),project(p[2],axis)).sign!=0;
    if(!area)throw std::invalid_argument("Volume binding cannot admit a degenerate triangle");
    Bounds b{p[0],p[0],t};for(int axis=0;axis<3;++axis){budget.step();for(int k=1;k<3;++k){b.lo[axis]=std::min(b.lo[axis],p[k][axis]);b.hi[axis]=std::max(b.hi[axis],p[k][axis]);}lo[axis]=std::min(lo[axis],b.lo[axis]);hi[axis]=std::max(hi[axis],b.hi[axis]);}bounds.push_back(b);
  }
  int axis=0;for(int a=1;a<3;++a)if(double(hi[a])-lo[a]>double(hi[axis])-lo[axis])axis=a;
  std::sort(bounds.begin(),bounds.end(),[&](const Bounds&a,const Bounds&b){budget.step();return a.lo[axis]!=b.lo[axis]?a.lo[axis]<b.lo[axis]:a.triangle<b.triangle;});
  for(size_t i=0;i<count;++i)for(size_t j=i+1;j<count;++j){budget.step();auto a=bounds[i],b=bounds[j];if(b.lo[axis]>a.hi[axis])break;bool overlap=true;for(int k=0;k<3;++k){budget.step();if(a.hi[k]<b.lo[k]||b.hi[k]<a.lo[k]){overlap=false;break;}}if(!overlap)continue;
    if(unexpected_contact(triangle(a.triangle),triangle(b.triangle),budget))throw std::invalid_argument("Volume binding refuses self-intersecting, overlapping or unowned touching surfaces");
  }
  memory.release(count,sizeof(Bounds));memory.release(1,8192);
}
} // namespace dome::detail::volume_topology
