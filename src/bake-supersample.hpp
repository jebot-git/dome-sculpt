// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh.hpp"
#include <algorithm>
#include <numeric>
#include <span>
#include <tuple>

namespace dome::detail {
// A capture may split each polygon corner into its own raster vertex. Recover
// only opposite half edges with matching logical IDs and UVs. Explicit seams
// and ambiguous non-manifold edges stay separate.
inline std::vector<uint32_t> bake_sample_charts(const Mesh &mesh,std::span<const uint32_t> faces,
                                               std::span<const uint32_t> corners,std::span<const uint8_t> seams,
                                               const auto &check){
  using Endpoint=std::tuple<uint32_t,float,float>;
  struct HalfEdge { Endpoint start,end; uint32_t face; bool reversed; };
  std::vector<uint32_t> parent(mesh.indices.size()/3);
  std::iota(parent.begin(),parent.end(),0);
  std::vector<HalfEdge> edges;
  const size_t count=faces.empty()?parent.size():faces.size();
  edges.reserve(count*3);
  const auto endpoint=[&](size_t corner){
    const auto uv=mesh.uv[mesh.indices[corner]];
    return Endpoint{corners[corner],uv[0],uv[1]};
  };
  for(size_t i=0;i<count;++i){
    if((i&1023)==0)check();
    const uint32_t face=faces.empty()?uint32_t(i):faces[i];
    const size_t first=size_t(face)*3;
    for(size_t corner=0;corner<3;++corner){
      if(!seams.empty()&&seams[first+corner])continue;
      auto a=endpoint(first+corner),b=endpoint(first+(corner+1)%3);
      const bool reversed=b<a;
      if(reversed)std::swap(a,b);
      edges.push_back({a,b,face,reversed});
    }
  }
  std::ranges::sort(edges,[](const HalfEdge &a,const HalfEdge &b){
    return std::tie(a.start,a.end)<std::tie(b.start,b.end);
  });
  check();
  const auto root=[&](uint32_t face){
    while(parent[face]!=face){
      parent[face]=parent[parent[face]];
      face=parent[face];
    }
    return face;
  };
  for(size_t first=0;first<edges.size();){
    check();
    const auto &a=edges[first];
    size_t end=first+1;
    while(end<edges.size()&&a.start==edges[end].start&&a.end==edges[end].end){
      if((end&1023)==0)check();
      ++end;
    }
    if(end-first==2&&a.reversed!=edges[first+1].reversed){
      const auto first_root=root(a.face),second_root=root(edges[first+1].face);
      parent[std::max(first_root,second_root)]=std::min(first_root,second_root);
    }
    first=end;
  }
  for(size_t i=0;i<parent.size();++i){
    if((i&1023)==0)check();
    parent[i]=root(uint32_t(i));
  }
  return parent;
}

struct BakeSampleSum {
  uint32_t chart=UINT32_MAX,triangle=UINT32_MAX;
  uint8_t hits=0,misses=0,radiance_samples=0;
  std::array<float,3> bary{};
  std::array<Vec3,2> normal{};
  // Keep both straight and associated RGB. Fully transparent authored RGB is
  // retained when every admitted hit has zero alpha.
  struct Color {
    std::array<float,3> straight{},associated{};
    float alpha=0;
    void add(const std::array<float,4> &rgba){
      for(size_t c=0;c<3;++c){
        straight[c]+=rgba[c];
        associated[c]+=rgba[c]*rgba[3];
      }
      alpha+=rgba[3];
    }
    std::array<float,4> resolve(unsigned count) const {
      std::array<float,4> rgba{};
      for(size_t c=0;c<3;++c)rgba[c]=alpha>0?associated[c]/alpha:straight[c]/float(count);
      rgba[3]=alpha/float(count);
      return rgba;
    }
  } color;
  std::array<double,3> emission{};
  double ao=0;
  float roughness=0,metallic=0;
};
} // namespace dome::detail
