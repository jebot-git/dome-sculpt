// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-ray-device.hpp"
#include "surface-ray-kernel.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_AMBIENT_INLINE __host__ __device__ inline
#else
#define DOME_AMBIENT_INLINE inline
#endif

namespace dome::detail::bake_rays {
struct SurfaceView {
  const Node *nodes;
  const uint32_t *order,*indices;
  const Vec3 *vertices;
  uint32_t node_count;

  DOME_AMBIENT_INLINE Vec3 point(uint32_t triangle,size_t corner) const {
    return vertices[indices[size_t(triangle)*3+corner]];
  }
};

constexpr uint32_t blocked=UINT32_MAX;
constexpr uint32_t nodes_per_invocation=512;

DOME_AMBIENT_INLINE float direction_component(float tangent,float bitangent,float normal,Vec3 sample) {
  return tangent*sample.x+bitangent*sample.y+normal*sample.z;
}

template<class Real>
DOME_AMBIENT_INLINE bool intersects_node(const Node &node,Vec3 origin,Vec3 direction,Real maximum) {
  const float p[]={origin.x,origin.y,origin.z},d[]={direction.x,direction.y,direction.z},
              lo[]={node.lo.x,node.lo.y,node.lo.z},hi[]={node.hi.x,node.hi.y,node.hi.z};
  bool inside=true;
  for(int axis=0;axis<3;++axis){
    if(p[axis]<lo[axis]){
      if(d[axis]<=surface_ray::parallel_limit)return false;
      inside=false;
    }else if(p[axis]>hi[axis]){
      if(d[axis]>=-surface_ray::parallel_limit)return false;
      inside=false;
    }
  }
  return inside||surface_ray::intersects_box(node.lo,node.hi,origin,direction,Real(0.f),maximum);
}

template<class Precision,class Normalize>
DOME_AMBIENT_INLINE uint32_t trace(SurfaceView surface,Frame frame,Vec3 sample,
                                   float maximum,uint32_t node_id,Normalize reciprocal_length) {
  using Real=typename Precision::Scalar;
  const Vec3 raw{
    direction_component(frame.tangent.x,frame.bitangent.x,frame.normal.x,sample),
    direction_component(frame.tangent.y,frame.bitangent.y,frame.normal.y,sample),
    direction_component(frame.tangent.z,frame.bitangent.z,frame.normal.z,sample),
  };
  const float squared_length=raw.x*raw.x+raw.y*raw.y+raw.z*raw.z;
  const float inverse=reciprocal_length(squared_length);
  const Vec3 direction{raw.x*inverse,raw.y*inverse,raw.z*inverse};
  surface_ray::WatertightRay<Precision> ray;
  bool prepared=false;
  const Real limit(maximum);
  // A preorder escape skips the whole rejected subtree. Resume long
  // traversals between bounded kernels so cancellation can be observed.
  for(uint32_t steps=0;steps<nodes_per_invocation&&node_id<surface.node_count;++steps){
    const auto node=surface.nodes[node_id];
    // Preserve the original unbounded slab acceptance. The triangle test
    // applies the finite inclusive segment using the watertight metric.
    if(!intersects_node(node,frame.origin,direction,Precision::finite_max)){
      node_id=node.escape;
      continue;
    }
    if(node.count&&!prepared){
      ray=surface_ray::WatertightRay<Precision>(frame.origin,direction);
      prepared=true;
    }
    for(uint32_t i=0;i<node.count;++i){
      const auto triangle=surface.order[node.first+i];
      surface_ray::Hit<Real> hit;
      if(ray.hit(surface.point(triangle,0),surface.point(triangle,1),
                 surface.point(triangle,2),limit,hit))return blocked;
    }
    ++node_id;
  }
  return node_id;
}

DOME_AMBIENT_INLINE uint16_t count_blocked_rays(const uint32_t *states,size_t samples) {
  uint16_t total=0;
  for(size_t i=0;i<samples;++i)total+=states[i]==blocked;
  return total;
}

inline constexpr uint32_t more_nodes=1,needs_boundary=2,triangle_limit_exceeded=4;

template<class Real> struct ProjectionProgress {
  uint32_t node,visited;
  Real depth,separation2;
};

template<class Precision>
DOME_AMBIENT_INLINE uint32_t trace_projection(SurfaceView surface,const ProjectionRay &input,
    uint32_t budget,bool first,ProjectionProgress<typename Precision::Scalar> &progress,ProjectionHit &result){
  using Real=typename Precision::Scalar;
  if(first){
    progress={0,0,Real(input.maximum),Real(0.f)};
    result={};
  }else if(progress.node==surface.node_count)return 0;

  auto next=progress;
  uint32_t winner=UINT32_MAX;
  surface_ray::Hit<Real> winning;
  surface_ray::WatertightRay<Precision> ray;
  bool prepared=false;
  for(uint32_t steps=0;steps<nodes_per_invocation&&next.node<surface.node_count;++steps){
    const auto node=surface.nodes[next.node];
    if(!intersects_node(node,input.origin,input.direction,Precision::finite_max)){
      next.node=node.escape;
      continue;
    }
    if(node.count>budget-next.visited)return triangle_limit_exceeded;
    next.visited+=node.count;
    if(node.count&&!prepared){
      ray=surface_ray::WatertightRay<Precision>(input.origin,input.direction);
      prepared=true;
    }
    for(uint32_t i=0;i<node.count;++i){
      const auto triangle=surface.order[node.first+i];
      surface_ray::Hit<Real> hit;
      if(!ray.hit(surface.point(triangle,0),surface.point(triangle,1),surface.point(triangle,2),next.depth,hit))continue;
      // The host query replaces equal-depth hits in BVH traversal order.
      // Keep the exact depth across passes, and round only the final payload.
      next.depth=hit.distance;
      winning=hit;
      winner=triangle;
    }
    ++next.node;
  }
  progress=next;
  if(winner!=UINT32_MAX){
    const float distance=Precision::narrow(winning.distance);
    result={{input.origin.x+input.direction.x*distance,input.origin.y+input.direction.y*distance,
             input.origin.z+input.direction.z*distance},
            {Precision::narrow(winning.u/winning.determinant),Precision::narrow(winning.v/winning.determinant),
             Precision::narrow(winning.w/winning.determinant)},distance,winner};
  }
  if(next.node<surface.node_count)return more_nodes;
  return result.triangle==UINT32_MAX&&input.radius_bits?needs_boundary:0;
}

template<class Precision>
DOME_AMBIENT_INLINE uint32_t trace_boundary(SurfaceView surface,const ProjectionRay &input,
    uint32_t budget,bool first,ProjectionProgress<typename Precision::Scalar> &progress,ProjectionHit &result){
  using Real=typename Precision::Scalar;
  if(first){
    if(result.triangle!=UINT32_MAX||!input.radius_bits)return 0;
    progress.node=0;
  }
  if(progress.node==surface.node_count)return 0;

  auto next=progress;
  auto nearest=result;
  const Real maximum(input.maximum),radius=Precision::from_bits(input.radius_bits),radius2=radius*radius;
  const surface_ray::BoundaryRay<Precision> ray(input.origin,input.direction,maximum);
  for(uint32_t steps=0;steps<nodes_per_invocation&&next.node<surface.node_count;++steps){
    const auto node=surface.nodes[next.node];
    if(!surface_ray::intersects_box(node.lo,node.hi,input.origin,input.direction,radius,maximum)){
      next.node=node.escape;
      continue;
    }
    if(node.count>budget-next.visited)return triangle_limit_exceeded;
    next.visited+=node.count;
    for(uint32_t i=0;i<node.count;++i){
      const auto triangle=surface.order[node.first+i];
      for(unsigned a=0;a<3;++a){
        const unsigned b=(a+1)%3;
        const auto start=surface.point(triangle,a),end=surface.point(triangle,b);
        surface_ray::EdgeHit<Real> edge;
        if(!ray.nearest_edge(start,end,edge)||edge.separation2>radius2)continue;
        if(nearest.triangle!=UINT32_MAX&&
           (edge.separation2>next.separation2||
            (edge.separation2==next.separation2&&(edge.distance>next.depth||
             (edge.distance==next.depth&&triangle>=nearest.triangle)))))continue;
        next.separation2=edge.separation2;
        next.depth=edge.distance;
        nearest={ray.position(start,end,edge.weight),{},Precision::narrow(edge.distance),triangle};
        nearest.barycentric[a]=Precision::narrow(Real(1.f)-edge.weight);
        nearest.barycentric[b]=Precision::narrow(edge.weight);
      }
    }
    ++next.node;
  }
  progress=next;
  result=nearest;
  return next.node<surface.node_count?more_nodes:0;
}
} // namespace dome::detail::bake_rays

#undef DOME_AMBIENT_INLINE
