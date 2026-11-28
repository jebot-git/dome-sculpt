// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "key-mirror-overlap.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_OVERLAP_INLINE __host__ __device__ inline
#else
#define DOME_OVERLAP_INLINE inline
#endif

namespace dome::detail::mirror_overlap {
struct View {
  const Box *boxes;
  const Node *nodes;
  const uint32_t *order;
  uint32_t node_count;
};
struct Progress { uint32_t node=0,leaf=0; };
struct Status { uint32_t visits=0,hits=0; };
inline constexpr uint32_t pending=1u<<31;
static_assert(batch_capacity*visit_quantum<pending);

template<class Real>
DOME_OVERLAP_INLINE bool overlaps(Box target,Box donor,Real twice_plane,Real tolerance) {
  const Real low=twice_plane-Real(donor.hi.x),high=twice_plane-Real(donor.lo.x);
  return Real(target.lo.x)<=high+tolerance&&low<=Real(target.hi.x)+tolerance&&
         Real(target.lo.y)<=Real(donor.hi.y)+tolerance&&Real(donor.lo.y)<=Real(target.hi.y)+tolerance&&
         Real(target.lo.z)<=Real(donor.hi.z)+tolerance&&Real(donor.lo.z)<=Real(target.hi.z)+tolerance;
}

template<class Precision,class Emit>
DOME_OVERLAP_INLINE uint32_t visit(View view,uint32_t target,Metric metric,uint32_t allowance,
                                  Progress &state,Emit emit) {
  using Real=typename Precision::Scalar;
  const auto plane=Precision::from_bits(metric.twice_plane);
  const Real tolerance(metric.tolerance);
  const auto box=view.boxes[target];
  uint32_t visits=0;
  for(;state.node<view.node_count&&visits<allowance;++visits){
    const auto node=view.nodes[state.node];
    if(state.leaf){
      const auto donor=view.order[node.first+state.leaf-1];
      if(donor>=target&&overlaps(box,view.boxes[donor],plane,tolerance))emit(Pair{target,donor});
      if(state.leaf==node.count){state.leaf=0;++state.node;}
      else ++state.leaf;
    }else if(node.last_triangle<target||!overlaps(box,node.bounds,plane,tolerance)){
      state.node=node.escape;
    }else if(node.count){
      state.leaf=1;
    }else{
      ++state.node;
    }
  }
  return visits|(state.node<view.node_count?pending:0);
}
}
#undef DOME_OVERLAP_INLINE
