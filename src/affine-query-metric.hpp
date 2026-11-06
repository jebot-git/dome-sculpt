// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Private local-BVH query math. Transform edge DIFFERENCES in binary64, never
// materialize absolute world positions in a float Mesh. Translation must not
// collapse a valid local triangle or change its retained authoring topology.
#include "dome/affine.hpp"
#include "dome/mesh.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace dome::detail {
using QueryPoint=std::array<double,3>;
inline QueryPoint query_point(Vec3 p){return {p.x,p.y,p.z};}
inline QueryPoint query_sub(QueryPoint a,QueryPoint b){for(size_t k=0;k<3;++k)a[k]-=b[k];return a;}
inline double query_dot(QueryPoint a,QueryPoint b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline QueryPoint query_cross(QueryPoint a,QueryPoint b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
struct MetricClosest {
  QueryPoint relative;
  std::array<float,3> bary;
  double distance2=INFINITY,planar_distance2=INFINITY;
};
inline MetricClosest metric_closest(QueryPoint ap,QueryPoint ab,QueryPoint ac){
  const auto n=query_cross(ab,ac);const auto area=query_dot(n,n);
  if(!(area>0)||!std::isfinite(area))throw std::runtime_error("Affine query lost a valid local triangle metric");
  const auto plane=query_dot(ap,n)/area;QueryPoint projection;
  for(size_t k=0;k<3;++k)projection[k]=ap[k]-n[k]*plane;
  auto candidate=[&](const std::array<double,3> &bary){
    QueryPoint q,delta,planar;
    for(size_t k=0;k<3;++k){q[k]=ab[k]*bary[1]+ac[k]*bary[2];delta[k]=ap[k]-q[k];planar[k]=projection[k]-q[k];}
    return MetricClosest{q,{float(bary[0]),float(bary[1]),float(bary[2])},query_dot(delta,delta),query_dot(planar,planar)};
  };
  const auto v=query_dot(query_cross(ap,ac),n)/area,w=query_dot(query_cross(ab,ap),n)/area,u=1-v-w;
  if(u>=0&&v>=0&&w>=0)return candidate({u,v,w});
  const std::array<QueryPoint,3> points{QueryPoint{},ab,ac};MetricClosest best;
  for(size_t i=0;i<3;++i){const auto j=(i+1)%3;const auto edge=query_sub(points[j],points[i]);const auto length=query_dot(edge,edge);
    if(!(length>0))throw std::runtime_error("Affine query collapsed a valid local edge");
    const auto t=std::clamp(query_dot(query_sub(ap,points[i]),edge)/length,0.,1.);std::array<double,3>bary{};bary[i]=1-t;bary[j]=t;auto value=candidate(bary);
    if(value.planar_distance2<best.planar_distance2)best=value;
  }
  return best;
}
// Outward-rounded interval image of a local AABB relative to the query. Its
// distance is a lower bound in the actual authored world metric, including
// shear/nonuniform scale/reflection, without an approximate similarity.
inline double metric_bounds_distance(Vec3 minimum,Vec3 maximum,QueryPoint local,const AffineTransform &world){
  const auto &m=world.matrix();const auto lo=query_point(minimum),hi=query_point(maximum);double result=0;
  for(size_t row=0;row<3;++row){double lower=0,upper=0;
    for(size_t k=0;k<3;++k){const auto a=std::nextafter(lo[k]-local[k],-INFINITY),b=std::nextafter(hi[k]-local[k],INFINITY),weight=m[k*4+row];
      const auto x=weight*a,y=weight*b;
      lower=std::nextafter(lower+std::nextafter(std::min(x,y),-INFINITY),-INFINITY);
      upper=std::nextafter(upper+std::nextafter(std::max(x,y),INFINITY),INFINITY);
    }
    const auto distance=std::max({lower,0.,-upper});
    const auto square=std::max(0.,std::nextafter(distance*distance,-INFINITY));result=std::max(0.,std::nextafter(result+square,-INFINITY));
  }
  return result;
}
struct MetricRay {
  QueryPoint origin,direction;
  int x,y,z;double sx,sy,sz;
  MetricRay(QueryPoint p,QueryPoint d):origin(p),direction(d){
    z=std::abs(d[0])>std::abs(d[1])?0:1;if(std::abs(d[2])>std::abs(d[z]))z=2;x=(z+1)%3;y=(x+1)%3;if(d[z]<0)std::swap(x,y);
    if(d[z]==0||!std::isfinite(d[z]))throw std::invalid_argument("Invalid affine local ray direction");sz=1/d[z];sx=d[x]*sz;sy=d[y]*sz;
  }
  bool box(Vec3 minimum,Vec3 maximum,double maximum_distance) const {
    const auto lo=query_point(minimum),hi=query_point(maximum);double a=0,b=maximum_distance;
    for(size_t k=0;k<3;++k){if(direction[k]==0){if(origin[k]<lo[k]||origin[k]>hi[k])return false;continue;}
      auto near=(lo[k]-origin[k])/direction[k],far=(hi[k]-origin[k])/direction[k];if(near>far)std::swap(near,far);
      a=std::max(a,std::nextafter(near,-INFINITY));b=std::min(b,std::nextafter(far,INFINITY));if(a>b)return false;
    }
    return true;
  }
  bool hit(Vec3 a,Vec3 b,Vec3 c,double maximum,double &distance,std::array<float,3> &bary) const {
    auto shear=[&](Vec3 v){auto p=query_sub(query_point(v),origin);return QueryPoint{p[x]-sx*p[z],p[y]-sy*p[z],p[z]};};
    const auto av=shear(a),bv=shear(b),cv=shear(c);
    const auto u=cv[0]*bv[1]-cv[1]*bv[0],v=av[0]*cv[1]-av[1]*cv[0],w=bv[0]*av[1]-bv[1]*av[0],det=u+v+w;
    if(((u<0||v<0||w<0)&&(u>0||v>0||w>0))||!det||!std::isfinite(det))return false;
    distance=(u*av[2]+v*bv[2]+w*cv[2])*sz/det;
    if(!std::isfinite(distance)||distance<0||distance>maximum)return false;
    bary={float(u/det),float(v/det),float(w/det)};return true;
  }
};
inline Vec3 metric_normal(const Mesh &mesh,uint32_t triangle,const std::array<float,3> &bary,const AffineTransform &inverse){
  QueryPoint sum{};const auto &m=inverse.matrix();
  for(size_t c=0;c<3;++c){auto n=query_point(mesh.normals.at(mesh.indices.at(size_t(triangle)*3+c)));QueryPoint transformed{};
    for(size_t row=0;row<3;++row)for(size_t k=0;k<3;++k)transformed[row]+=m[row*4+k]*n[k];
    const auto magnitude=std::hypot(transformed[0],transformed[1],transformed[2]);
    if(!(magnitude>0)||!std::isfinite(magnitude))throw std::invalid_argument("Undefined affine surface normal");
    for(size_t k=0;k<3;++k)sum[k]+=transformed[k]/magnitude*bary[c];
  }
  auto magnitude=std::hypot(sum[0],sum[1],sum[2]);
  if(!(magnitude>1e-12)){
    const auto a=mesh.positions.at(mesh.indices.at(size_t(triangle)*3)),b=mesh.positions.at(mesh.indices.at(size_t(triangle)*3+1)),c=mesh.positions.at(mesh.indices.at(size_t(triangle)*3+2));
    const auto local=query_cross(query_sub(query_point(b),query_point(a)),query_sub(query_point(c),query_point(a)));sum={};
    for(size_t row=0;row<3;++row)for(size_t k=0;k<3;++k)sum[row]+=m[row*4+k]*local[k];magnitude=std::hypot(sum[0],sum[1],sum[2]);
  }
  if(!(magnitude>0)||!std::isfinite(magnitude))throw std::invalid_argument("Undefined affine geometric normal");
  return {float(sum[0]/magnitude),float(sum[1]/magnitude),float(sum[2]/magnitude)};
}
} // namespace dome::detail
