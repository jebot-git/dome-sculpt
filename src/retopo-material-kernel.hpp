// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "retopo-affine-dot.hpp"
#include "retopo-surface-kernel.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_RETOPO_MATERIAL __host__ __device__ inline
#else
#define DOME_RETOPO_MATERIAL inline
#endif

namespace dome::detail::retopo_surface {
using surface_closest::Point;

template<class Precision>
DOME_RETOPO_MATERIAL typename Precision::Scalar outward(typename Precision::Scalar value,bool upper){
  auto bits=Precision::bits(value);
  const auto magnitude=bits&~Extended::leading;
  if(magnitude>Extended::infinity)return value;
  if(!magnitude)return Precision::from_bits(upper?1:Extended::leading|1);
  const bool advancing=upper==!(bits&Extended::leading);
  if(advancing&&magnitude==Extended::infinity)return value;
  return Precision::from_bits(advancing?bits+1:bits-1);
}

template<class Precision>
struct TransferAffine {
  using Real=typename Precision::Scalar;
  const TransferMetric &metric;
  Point<Real> local;

  DOME_RETOPO_MATERIAL static bool indeterminate(Real value) {
    return (Precision::bits(value)&~Extended::leading)>Extended::infinity;
  }

  DOME_RETOPO_MATERIAL Real bounds(const weight_transfer::Node &node) const {
    const float lo[]{node.lo.x,node.lo.y,node.lo.z},hi[]{node.hi.x,node.hi.y,node.hi.z};
    const Real zero(0.f);
    Real low[3],high[3];
    for(int k=0;k<3;++k){
      low[k]=outward<Precision>(Real(lo[k])-local[k],false);
      high[k]=outward<Precision>(Real(hi[k])-local[k],true);
    }
    Real result=zero;
    for(const auto &row:metric.rows){
      Real lower=zero,upper=zero;
      for(int k=0;k<3;++k){
        const auto weight=Precision::from_bits(row[k]);
        // An exactly zero column term stays zero for a finite local query.
        // Widen zero too, matching the host's outward interval operations.
        const auto x=weight==zero?zero:weight*low[k],y=weight==zero?zero:weight*high[k];
        const bool reversed=y<x;
        lower=outward<Precision>(lower+outward<Precision>(reversed?y:x,false),false);
        upper=outward<Precision>(upper+outward<Precision>(reversed?x:y,true),true);
      }
      // An overflow in opposing interval contributions cannot rule out a subtree.
      if(indeterminate(lower)||indeterminate(upper))return zero;
      const Real distance=lower>zero?lower:-upper>zero?-upper:zero;
      const auto square=outward<Precision>(distance*distance,false);
      result=outward<Precision>(result+(square>zero?square:zero),false);
      if(result<zero)result=zero;
    }
    return result;
  }

  DOME_RETOPO_MATERIAL Point<Real> direction(Point<Real> v,bool &valid) const {
    const uint64_t words[]{Precision::bits(v[0]),Precision::bits(v[1]),Precision::bits(v[2])};
    for(const auto word:words)if((word&~Extended::leading)>=Extended::infinity){valid=false;return {};}
    return {Precision::from_bits(affine_dot(metric.rows[0],words,valid)),
            Precision::from_bits(affine_dot(metric.rows[1],words,valid)),
            Precision::from_bits(affine_dot(metric.rows[2],words,valid))};
  }

  DOME_RETOPO_MATERIAL bool triangle(Vec3 a,Vec3 b,Vec3 c, surface_closest::Hit<Real> &result) const {
    using surface_closest::dot;
    const Real zero(0.f),one(1.f);
    bool valid=true;
    const auto ap=direction({local[0]-Real(a.x),local[1]-Real(a.y),local[2]-Real(a.z)},valid);
    const auto ab=direction(surface_closest::difference<Real>(b,a),valid);
    const auto ac=direction(surface_closest::difference<Real>(c,a),valid);
    if(!valid)return false;
    const auto n=surface_closest::cross(ab,ac);
    const auto area=dot(n,n);
    if(!(area>zero&&area<=Precision::finite_max))return false;
    const auto plane=dot(ap,n)/area;
    Point<Real> projection;
    for(int k=0;k<3;++k)projection[k]=ap[k]-n[k]*plane;
    const auto v=dot(surface_closest::cross(ap,ac),n)/area;
    const auto w=dot(surface_closest::cross(ab,ap),n)/area,u=one-v-w;
    if(u>=zero&&v>=zero&&w>=zero){
      result=surface_closest::candidate<Precision>({},ab,ac,ap,projection,{u,v,w});
      return true;
    }
    const Point<Real> points[]{{zero,zero,zero},ab,ac};
    result.planar_distance2=Precision::finite_max;
    for(int i=0;i<3;++i){
      const int j=(i+1)%3;
      Point<Real> edge,offset;
      for(int k=0;k<3;++k){edge[k]=points[j][k]-points[i][k];offset[k]=ap[k]-points[i][k];}
      const auto span=dot(edge,edge);
      if(!(span>zero))return false;
      const auto t=dot(offset,edge)/span;
      if(indeterminate(t))return false;
      const auto clamped=t<zero?zero:t>one?one:t;
      Point<Real> bary{zero,zero,zero};
      bary[i]=one-clamped;bary[j]=clamped;
      const auto hit=surface_closest::candidate<Precision>({},ab,ac,ap,projection,bary);
      if(hit.planar_distance2<result.planar_distance2)result=hit;
    }
    return true;
  }
};

template<class Precision,bool Affine>
DOME_RETOPO_MATERIAL uint32_t search_material(weight_transfer::SurfaceView surface,
    const Vec3 *normals,const uint8_t *membership,TransferMetric metric,const TransferPoint &input,
    TransferHit &hit,Progress &progress,bool first){
  using Real=typename Precision::Scalar;
  if(first){
    begin(progress);
    hit={metric.distance_limit,UINT32_MAX,{}};
  }else if(!progress.length)return 0;
  auto best=Precision::from_bits(hit.distance);
  const Point<Real> position{Precision::from_bits(input.position[0]),Precision::from_bits(input.position[1]),
                             Precision::from_bits(input.position[2])};
  const TransferAffine<Precision> affine{metric,position};
  const Vec3 p{Precision::narrow(position[0]),Precision::narrow(position[1]),Precision::narrow(position[2])};
  const auto bounds=[&](const auto &node){
    if constexpr(Affine)return affine.bounds(node);
    else return surface_closest::box_distance<Real>(node.lo,node.hi,p);
  };
  return walk(surface,2'000'000,progress,bounds,[&]{return best;},[&](uint32_t triangle){
    if((input.restrict_material&&!membership[triangle])||
       !(weight_transfer::product(normals[triangle],input.normal)>0))return true;
    const auto *indices=surface.indices+size_t(triangle)*3;
    const auto a=surface.vertices[indices[0]],b=surface.vertices[indices[1]],c=surface.vertices[indices[2]];
    surface_closest::Hit<Real> closest{};
    if constexpr(Affine){
      if(!affine.triangle(a,b,c,closest))return false;
    }else if(!surface_closest::triangle<Precision>(p,a,b,c,closest))return false;
    const auto distance=closest.distance2;
    if(distance<best||(Affine&&distance==best&&triangle<hit.triangle)){
      best=distance;
      hit={Precision::bits(best),triangle,{closest.bary[0],closest.bary[1],closest.bary[2]}};
    }
    return true;
  });
}
}
#undef DOME_RETOPO_MATERIAL
