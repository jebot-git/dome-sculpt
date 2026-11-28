// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "weight-transfer-device.hpp"

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_TRANSFER_FN __host__ __device__ inline
#else
#define DOME_TRANSFER_FN inline
#endif

namespace dome::detail::weight_transfer {
struct SurfaceView {
  const Node *nodes;
  const uint32_t *order,*indices;
  const Vec3 *vertices;
};
DOME_TRANSFER_FN Vec3 difference(Vec3 a,Vec3 b) {
  return {a.x-b.x,a.y-b.y,a.z-b.z};
}
DOME_TRANSFER_FN float product(Vec3 a,Vec3 b) {
  return a.x*b.x+a.y*b.y+a.z*b.z;
}
DOME_TRANSFER_FN float squared_distance(Vec3 a,Vec3 b) {
  const auto d=difference(a,b);
  return product(d,d);
}
DOME_TRANSFER_FN float clamp_coordinate(float p,float lo,float hi) {
  return p<lo?lo:(p>hi?hi:p);
}
DOME_TRANSFER_FN float box_distance(const Node &node,Vec3 p) {
  return squared_distance(p,{clamp_coordinate(p.x,node.lo.x,node.hi.x),
                             clamp_coordinate(p.y,node.lo.y,node.hi.y),
                             clamp_coordinate(p.z,node.lo.z,node.hi.z)});
}

// Convex closest-point regions. Match the original transfer expression and
// its binary32 evaluation, including the vertex and edge boundary tests.
DOME_TRANSFER_FN Barycentric closest(Vec3 p, Vec3 a, Vec3 b, Vec3 c) {
  const auto ab=difference(b,a),ac=difference(c,a),ap=difference(p,a);
  const float d1=product(ab,ap),d2=product(ac,ap);
  if(d1<=0&&d2<=0)return {1,0,0};
  const auto bp=difference(p,b);
  const float d3=product(ab,bp),d4=product(ac,bp);
  if(d3>=0&&d4<=d3)return {0,1,0};
  const float vc=d1*d4-d3*d2;
  if(vc<=0&&d1>=0&&d3<=0){
    const float v=d1/(d1-d3);
    return {1-v,v,0};
  }
  const auto cp=difference(p,c);
  const float d5=product(ab,cp),d6=product(ac,cp);
  if(d6>=0&&d5<=d6)return {0,0,1};
  const float vb=d5*d2-d1*d6;
  if(vb<=0&&d2>=0&&d6<=0){
    const float w=d2/(d2-d6);
    return {1-w,0,w};
  }
  const float va=d3*d6-d5*d4;
  if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0){
    const float w=(d4-d3)/((d4-d3)+(d5-d6));
    return {0,1-w,w};
  }
  const float inverse=1/(va+vb+vc);
  const float v=vb*inverse,w=vc*inverse;
  return {1-v-w,v,w};
}

template<class Charge>
DOME_TRANSFER_FN Hit search(SurfaceView surface,Vec3 point,float best,Charge &charge) {
  Hit hit;
  // The host splits at the median and admits at most two million triangles.
  // Push the farther child first to retain the original depth-first order.
  uint32_t pending[32]={0};
  unsigned length=1;
  while(length){
    if(!charge(1))return {};
    const auto &node=surface.nodes[pending[--length]];
    if(box_distance(node,point)>best)continue;
    if(node.count){
      if(!charge(node.count))return {};
      for(uint32_t i=node.first;i<node.first+node.count;++i){
        const auto triangle=surface.order[i];
        const auto *indices=surface.indices+3*triangle;
        const auto a=surface.vertices[indices[0]],b=surface.vertices[indices[1]],c=surface.vertices[indices[2]];
        const auto weights=closest(point,a,b,c);
        const Vec3 nearest{a.x*weights.a+b.x*weights.b+c.x*weights.c,
                           a.y*weights.a+b.y*weights.b+c.y*weights.c,
                           a.z*weights.a+b.z*weights.b+c.z*weights.c};
        const float distance=squared_distance(point,nearest);
        if(distance<best||(distance==best&&triangle<hit.triangle)){
          best=distance;
          hit={triangle,weights};
        }
      }
    }else{
      const auto left=node.left,right=node.right;
      const bool right_first=box_distance(surface.nodes[left],point)>box_distance(surface.nodes[right],point);
      pending[length++]=right_first?left:right;
      pending[length++]=right_first?right:left;
    }
  }
  return hit;
}

template<class Counter> struct SharedWork {
  Counter &counter;
  uint32_t limit;
  DOME_TRANSFER_FN bool operator()(uint32_t amount) {
    const uint32_t before=counter.add(amount);
    return amount<=limit&&before<=limit-amount;
  }
};
struct LocalWork {
  uint32_t used=0;
  DOME_TRANSFER_FN bool operator()(uint32_t amount){used+=amount;return true;}
};

template<class Counter>
DOME_TRANSFER_FN Hit budgeted_search(SurfaceView view,Vec3 point,float radius,Counter &counter,
                                     uint32_t work_limit,bool all_queries_fit){
  // A proven worst-case fit needs one atomic per vertex. Pathological trees
  // use shared accounting at each node and before each bounded leaf.
  if(all_queries_fit){
    LocalWork local;
    const auto hit=search(view,point,radius,local);
    counter.add(local.used);
    return hit;
  }
  SharedWork<Counter> shared{counter,work_limit};
  return search(view,point,radius,shared);
}
} // namespace dome::detail::weight_transfer
#undef DOME_TRANSFER_FN
