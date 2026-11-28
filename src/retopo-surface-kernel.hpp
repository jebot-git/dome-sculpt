// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "retopo-surface-device.hpp"
#include "surface-closest-kernel.hpp"
#include "weight-transfer-kernel.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_RETOPO_FN __host__ __device__ inline
#else
#define DOME_RETOPO_FN inline
#endif

namespace dome::detail::retopo_surface {
inline constexpr uint32_t pending_nodes=1,invalid_triangle=2,over_budget=4;
inline constexpr uint32_t nodes_per_dispatch=256;

struct Progress {
  uint32_t pending[32];
  uint32_t length,visited;
};

DOME_RETOPO_FN void begin(Progress &progress){
  progress.pending[0]=0;
  progress.length=1;
  progress.visited=0;
}
template<class Bounds,class Best,class Test>
DOME_RETOPO_FN uint32_t walk(weight_transfer::SurfaceView surface,uint32_t budget,Progress &progress,
                            const Bounds &box_distance,const Best &best,const Test &test_triangle){
  for(uint32_t step=0;step<nodes_per_dispatch&&progress.length;++step){
    const auto &node=surface.nodes[progress.pending[--progress.length]];
    if(box_distance(node)>best())continue;
    if(node.count){
      if(node.count>budget-progress.visited)return over_budget;
      progress.visited+=node.count;
      for(uint32_t i=node.first;i<node.first+node.count;++i){
        if(!test_triangle(surface.order[i]))return invalid_triangle;
      }
    }else{
      // The host tree visits the closer child first and keeps the first hit
      // at the same distance. Left wins an exact child-bound tie.
      const bool right_first=box_distance(surface.nodes[node.left])>box_distance(surface.nodes[node.right]);
      progress.pending[progress.length++]=right_first?node.left:node.right;
      progress.pending[progress.length++]=right_first?node.right:node.left;
    }
  }
  return progress.length?pending_nodes:0;
}

template<class Precision>
DOME_RETOPO_FN uint32_t search(weight_transfer::SurfaceView surface,Vec3 point,uint32_t budget,
                              Hit &hit,Progress &progress,bool first){
  using Real=typename Precision::Scalar;
  if(first){
    begin(progress);
    hit={Precision::bits(Precision::finite_max),{},{},UINT32_MAX};
  }else if(!progress.length)return 0;
  auto best=Precision::from_bits(hit.distance_bits);
  const auto box_distance=[&](const auto &node){return surface_closest::box_distance<Real>(node.lo,node.hi,point);};
  return walk(surface,budget,progress,box_distance,[&]{return best;},[&](uint32_t triangle){
    const auto *indices=surface.indices+size_t(triangle)*3;
    surface_closest::Hit<Real> closest{};
    if(!surface_closest::triangle<Precision>(point,surface.vertices[indices[0]],
        surface.vertices[indices[1]],surface.vertices[indices[2]],closest))return false;
    if(closest.distance2<best){
      best=closest.distance2;
      hit={Precision::bits(best),closest.position,{closest.bary[0],closest.bary[1],closest.bary[2]},triangle};
    }
    return true;
  });
}
}
#undef DOME_RETOPO_FN
