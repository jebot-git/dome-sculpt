// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-ambient-device.hpp"
#include "surface-ray-kernel.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_AMBIENT_INLINE __host__ __device__ inline
#else
#define DOME_AMBIENT_INLINE inline
#endif

namespace dome::detail::bake_ambient {
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
} // namespace dome::detail::bake_ambient

#undef DOME_AMBIENT_INLINE
