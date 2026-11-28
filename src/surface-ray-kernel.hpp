// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/math.hpp"
#include <bit>
#include <cfloat>

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_RAY_INLINE __host__ __device__ inline
#else
#define DOME_RAY_INLINE inline
#endif

namespace dome::detail::surface_ray {
template<class T> DOME_RAY_INLINE T magnitude(T value) { return value<0?-value:value; }
// 1e-20f is the largest binary32 magnitude strictly below the original
// binary64 threshold, so this includes exactly the same ray directions.
inline constexpr float parallel_limit=1e-20f;
static_assert(double(parallel_limit)<1e-20);

struct NativePrecision {
  using Scalar=double;
  static constexpr Scalar finite_max=DBL_MAX;
  DOME_RAY_INLINE static Scalar from_bits(uint64_t bits){
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
    return __longlong_as_double(static_cast<long long>(bits));
#else
    return std::bit_cast<double>(bits);
#endif
  }
  DOME_RAY_INLINE static float narrow(Scalar value){return float(value);}
};

template<class Real>
DOME_RAY_INLINE bool intersects_box(Vec3 lo,Vec3 hi,Vec3 origin,Vec3 direction,
                                    Real radius,Real maximum) {
  Real near{0.f},far=maximum;
  const float p[]={origin.x,origin.y,origin.z},d[]={direction.x,direction.y,direction.z},
              lower[]={lo.x,lo.y,lo.z},upper[]={hi.x,hi.y,hi.z};
  for(int axis=0;axis<3;++axis){
    if(magnitude(d[axis])<=parallel_limit){
      if(Real(p[axis])<Real(lower[axis])-radius||Real(p[axis])>Real(upper[axis])+radius)return false;
      continue;
    }
    Real a=(Real(lower[axis])-radius-Real(p[axis]))/Real(d[axis]);
    Real b=(Real(upper[axis])+radius-Real(p[axis]))/Real(d[axis]);
    if(a>b){const Real saved=a;a=b;b=saved;}
    if(a>near)near=a;
    if(b<far)far=b;
    if(near>far)return false;
  }
  return true;
}

template<class Real> struct Hit {
  Real distance,u,v,w,determinant;
};

// Dominant-axis shear/edge tests adapted from Blender 5.4 Alpha's
// isect_ray_tri_watertight_v3[_precalc] (GPL-2.0-or-later), pinned in
// third_party/blender/math_geom.cc.reference. Keep shared-edge expressions
// identical in binary64 on the host and in the compute kernel.
template<class Precision> class WatertightRay {
  using Real=typename Precision::Scalar;
  struct Sheared { Real x,y,z; };
  Vec3 origin_;
  int x_,y_,z_;
  Real sx_,sy_,sz_;

  DOME_RAY_INLINE Sheared shear(Vec3 vertex) const {
    const Real delta[]={Real(vertex.x)-Real(origin_.x),Real(vertex.y)-Real(origin_.y),Real(vertex.z)-Real(origin_.z)};
    return {delta[x_]-sx_*delta[z_],delta[y_]-sy_*delta[z_],delta[z_]};
  }
public:
  DOME_RAY_INLINE WatertightRay():x_(0),y_(0),z_(0),sx_(),sy_(),sz_(){}
  DOME_RAY_INLINE WatertightRay(Vec3 origin,Vec3 direction):origin_(origin) {
    const float d[]={direction.x,direction.y,direction.z};
    z_=magnitude(d[0])>magnitude(d[1])?0:1;
    if(magnitude(d[2])>magnitude(d[z_]))z_=2;
    x_=(z_+1)%3;y_=(x_+1)%3;
    if(d[z_]<0){const int saved=x_;x_=y_;y_=saved;}
    sz_=Real(1.f)/Real(d[z_]);sx_=Real(d[x_])*sz_;sy_=Real(d[y_])*sz_;
  }

  DOME_RAY_INLINE bool hit(Vec3 a,Vec3 b,Vec3 c,Real maximum,Hit<Real> &result) const {
    const auto av=shear(a),bv=shear(b),cv=shear(c);
    const Real u=cv.x*bv.y-cv.y*bv.x,v=av.x*cv.y-av.y*cv.x,
               w=bv.x*av.y-bv.y*av.x,det=u+v+w;
    const Real zero{0.f},limit=Precision::finite_max;
    if(((u<zero||v<zero||w<zero)&&(u>zero||v>zero||w>zero))||
       det==zero||!(det>=-limit&&det<=limit))return false;
    const Real distance=(u*av.z+v*bv.z+w*cv.z)*sz_/det;
    if(!(distance>=zero&&distance<=maximum&&distance<=limit))return false;
    result={distance,u,v,w,det};
    return true;
  }
};

template<class Real> struct EdgeHit {
  Real separation2,distance,weight;
};

template<class Precision> class BoundaryRay {
  using Real=typename Precision::Scalar;
  Vec3 origin_;
  Real axis_[3],inverse_length2_,maximum_;

  DOME_RAY_INLINE static void difference(Vec3 a,Vec3 b,Real (&delta)[3]){
    delta[0]=Real(a.x)-Real(b.x);
    delta[1]=Real(a.y)-Real(b.y);
    delta[2]=Real(a.z)-Real(b.z);
  }
  DOME_RAY_INLINE static Real dot(const Real (&a)[3],const Real (&b)[3]){
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
  }

public:
  DOME_RAY_INLINE BoundaryRay(Vec3 origin,Vec3 direction,Real maximum)
      :origin_(origin),axis_{Real(direction.x),Real(direction.y),Real(direction.z)},
       inverse_length2_(Real(1.f)/dot(axis_,axis_)),maximum_(maximum){}

  DOME_RAY_INLINE bool nearest_edge(Vec3 a,Vec3 b,EdgeHit<Real> &hit) const {
    Real start[3],edge[3];
    difference(a,origin_,start);difference(b,a,edge);
    const Real start_depth=dot(start,axis_)*inverse_length2_;
    const Real depth_step=dot(edge,axis_)*inverse_length2_;
    const Real zero(0.f);
    Real lo=zero,hi(1.f);
    if(!(depth_step==zero)){
      Real first=-start_depth/depth_step,last=(maximum_-start_depth)/depth_step;
      if(first>last){const Real saved=first;first=last;last=saved;}
      if(first>lo)lo=first;
      if(last<hi)hi=last;
      if(lo>hi)return false;
    }else if(start_depth<zero||start_depth>maximum_)return false;

    Real across[3],across_step[3];
    for(int k=0;k<3;++k){
      across[k]=start[k]-start_depth*axis_[k];
      across_step[k]=edge[k]-depth_step*axis_[k];
    }
    const Real square=dot(across_step,across_step);
    Real weight=depth_step<zero?hi:lo;
    if(square>zero){
      const Real nearest=-dot(across,across_step)/square;
      weight=nearest<lo?lo:nearest>hi?hi:nearest;
    }
    const Real depth=start_depth+weight*depth_step;
    if(depth<zero||depth>maximum_)return false;
    for(int k=0;k<3;++k)across[k]=across[k]+weight*across_step[k];
    hit={dot(across,across),depth,weight};
    return true;
  }

  DOME_RAY_INLINE static Vec3 position(Vec3 a,Vec3 b,Real weight) {
    Real edge[3];difference(b,a,edge);
    return {Precision::narrow(Real(a.x)+weight*edge[0]),
            Precision::narrow(Real(a.y)+weight*edge[1]),
            Precision::narrow(Real(a.z)+weight*edge[2])};
  }
};
} // namespace dome::detail::surface_ray

#undef DOME_RAY_INLINE
