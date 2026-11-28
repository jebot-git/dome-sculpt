// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/math.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_CLOSEST_FN __host__ __device__ inline
#else
#define DOME_CLOSEST_FN inline
#endif

namespace dome::detail::surface_closest {
template<class Real>
struct Point {
  Real data[3];
  DOME_CLOSEST_FN Real &operator[](size_t axis){return data[axis];}
  DOME_CLOSEST_FN Real operator[](size_t axis)const{return data[axis];}
};
template<class Real>
DOME_CLOSEST_FN Point<Real> difference(Vec3 a,Vec3 b){
  return {Real(a.x)-Real(b.x),Real(a.y)-Real(b.y),Real(a.z)-Real(b.z)};
}
template<class Real>
DOME_CLOSEST_FN Real dot(Point<Real> a,Point<Real> b){
  return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
template<class Real>
DOME_CLOSEST_FN Point<Real> cross(Point<Real> a,Point<Real> b){
  return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
template<class Real>
DOME_CLOSEST_FN Real box_distance(Vec3 lo,Vec3 hi,Vec3 p){
  const auto distance=[](float lower,float upper,float point){
    return point<lower?Real(lower)-Real(point):point>upper?Real(point)-Real(upper):Real(0.f);
  };
  const Point<Real> delta{distance(lo.x,hi.x,p.x),distance(lo.y,hi.y,p.y),distance(lo.z,hi.z,p.z)};
  return dot(delta,delta);
}
template<class Real>
struct Hit {
  Vec3 position;
  float bary[3];
  Real distance2,planar_distance2;
};

template<class Precision>
DOME_CLOSEST_FN Hit<typename Precision::Scalar> candidate(Vec3 a,
    Point<typename Precision::Scalar> ab,Point<typename Precision::Scalar> ac,
    Point<typename Precision::Scalar> ap,Point<typename Precision::Scalar> projection,
    Point<typename Precision::Scalar> bary){
  using Real=typename Precision::Scalar;
  Point<Real> q,delta,planar_delta;
  for(size_t k=0;k<3;++k){
    q[k]=ab[k]*bary[1]+ac[k]*bary[2];
    delta[k]=ap[k]-q[k];
    planar_delta[k]=projection[k]-q[k];
  }
  return {{Precision::narrow(Real(a.x)+q[0]),Precision::narrow(Real(a.y)+q[1]),
           Precision::narrow(Real(a.z)+q[2])},
          {Precision::narrow(bary[0]),Precision::narrow(bary[1]),Precision::narrow(bary[2])},
          dot(delta,delta),dot(planar_delta,planar_delta)};
}

// Cross products avoid cancellation in |AB|²|AC|²-(AB.AC)² for thin
// triangles. Keep barycentrics before rounding the reconstructed point.
template<class Precision>
DOME_CLOSEST_FN bool triangle(Vec3 p,Vec3 a,Vec3 b,Vec3 c,
                              Hit<typename Precision::Scalar> &result){
  using Real=typename Precision::Scalar;
  const Real zero(0.f),one(1.f);
  const auto ab=difference<Real>(b,a),ac=difference<Real>(c,a),ap=difference<Real>(p,a);
  const auto n=cross(ab,ac);
  const Real area2=dot(n,n);
  if(!(area2>zero))return false;
  const Real plane_factor=dot(ap,n)/area2;
  Point<Real> projection;
  for(size_t k=0;k<3;++k)projection[k]=ap[k]-n[k]*plane_factor;
  const Real v=dot(cross(ap,ac),n)/area2,w=dot(cross(ab,ap),n)/area2,u=one-v-w;
  if(u>=zero&&v>=zero&&w>=zero){
    result=candidate<Precision>(a,ab,ac,ap,projection,{u,v,w});
    return true;
  }
  const Vec3 vertices[]{a,b,c};
  result.planar_distance2=Precision::finite_max;
  for(size_t i=0;i<3;++i){
    const size_t j=(i+1)%3;
    const auto edge=difference<Real>(vertices[j],vertices[i]);
    const Real t=dot(difference<Real>(p,vertices[i]),edge)/dot(edge,edge);
    const Real clamped=t<zero?zero:t>one?one:t;
    Point<Real> bary{zero,zero,zero};
    bary[i]=one-clamped;
    bary[j]=clamped;
    const auto hit=candidate<Precision>(a,ab,ac,ap,projection,bary);
    // The common plane-normal distance can round distinct edge distances to
    // a tie. Compare only the in-plane components during segment selection.
    if(hit.planar_distance2<result.planar_distance2)result=hit;
  }
  return true;
}
} // namespace dome::detail::surface_closest
#undef DOME_CLOSEST_FN
