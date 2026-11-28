// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "volume-binding-device.hpp"
#include <cmath>
#include <limits>

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_VOLUME_FN __host__ __device__ inline
#else
#define DOME_VOLUME_FN inline
#endif

namespace dome::detail::volume_device {
inline constexpr auto infinity = std::numeric_limits<float>::infinity();
inline constexpr unsigned lanes = 128;
struct View {
  const InteriorNode *interior;
  const int32_t *grid_to_node;
  const uint32_t *starts;
  const Edge *edges;
  Grid grid;
  size_t interior_size;
};

DOME_VOLUME_FN float minimum(float a,float b) { return a<b?a:b; }
DOME_VOLUME_FN void sort_pair(float &a,float &b) {
  if(b<a){const auto old=a; a=b; b=old;}
}
DOME_VOLUME_FN float upwind(float first,float second,float third) {
  sort_pair(first,second);
  sort_pair(second,third);
  sort_pair(first,second);
  float best=first+1.f;
  if(second>=best)return best;
  const float a=second-first;
  const float root2=2.f-a*a;
  const float two=first+.5f*(a+::sqrtf(root2));
  if(two>=second)best=minimum(best,two);
  if(third>=best)return best;
  const float b=third-first;
  const float sum=a+b;
  const float root3=sum*sum-3.f*(a*a+b*b-1.f);
  if(root3>=0.f){
    const float three=first+(sum+::sqrtf(root3))/3.f;
    if(three>=third)best=minimum(best,three);
  }
  return best;
}

DOME_VOLUME_FN float relax(size_t id,const View &graph,const float *field) {
  float best=field[id];
  for(uint32_t edge=graph.starts[id];edge<graph.starts[id+1];++edge){
    const auto arc=graph.edges[edge];
    best=minimum(best,field[arc.to]+arc.cells);
  }
  if(id>=graph.interior_size)return best;
  const auto node=graph.interior[id];
  float axis[3][2]={{infinity,infinity},{infinity,infinity},{infinity,infinity}};
  unsigned direction=0;
  for(int z=-1;z<=1;++z)for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
    const unsigned dimensions=unsigned(x!=0)+unsigned(y!=0)+unsigned(z!=0);
    if(!dimensions)continue;
    if(node.links & (uint32_t(1)<<direction)){
      const auto at=uint32_t(int32_t(node.grid)+graph.grid.offset[direction]);
      const float distance=field[graph.grid_to_node[at]];
      if(dimensions==1){
        axis[x?0:y?1:2][(x+y+z)>0]=distance;
        best=minimum(best,distance+1.f);
      }else best=minimum(best,distance+(dimensions==2?1.414213562373095f:1.732050807568877f));
    }
    ++direction;
  }
  if(node.clear_cubes==255){
    return minimum(best,upwind(minimum(axis[0][0],axis[0][1]),
        minimum(axis[1][0],axis[1][1]),minimum(axis[2][0],axis[2][1])));
  }
  // At a cut cell consider every certified octant separately. Choosing the
  // current smallest direction first would make the blocked stencil change
  // as a different front approaches from the other side of the cut.
  for(unsigned octant=0;octant<8;++octant)
    if(node.clear_cubes & (1u<<octant))
      best=minimum(best,upwind(axis[0][octant&1],axis[1][(octant>>1)&1],axis[2][octant>>2]));
  return best;
}
} // namespace dome::detail::volume_device
#undef DOME_VOLUME_FN
