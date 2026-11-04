// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "weight-surface-system.hpp"
#include "volume-topology.hpp"
#include <bit>
#include <map>
namespace dome::detail {
// A declared geometric sewing hypothesis for one current source owner. This
// does not modify raw topology or establish original skin/topology authority.
// Only unique, oppositely oriented co-moving boundary edges participate; no
// proximity weld, normals/style match, or original artist values are inputs.
struct GeometricBoundarySewing {
  std::vector<uint32_t> groups;
  size_t pairs=0, ambiguous=0, rejected=0, bytes=0;
};
inline GeometricBoundarySewing geometric_boundary_sewing(
    const Mesh &mesh,std::span<const std::vector<Vec3>> morphs,
    size_t maximum_bytes,weight_surface::Budget &work) {
  using Key=std::pair<std::vector<uint32_t>,std::vector<uint32_t>>;
  struct Edge{uint32_t a,b,face;};
  const size_t n=mesh.positions.size(),corners=mesh.indices.size();
  if(!n||n>2'000'000||corners%3||corners>6'000'000||morphs.size()>128)
    throw std::invalid_argument("Geometric sewing dimensions");
  const size_t bytes=n*16+corners*(256+48*morphs.size())+4096;
  if(bytes>maximum_bytes)throw std::length_error("Geometric sewing bytes before construction");
  work.step_items(n+corners,32);
  for(const auto&delta:morphs)if(delta.size()!=n)
    throw std::invalid_argument("Geometric sewing complete morph dimensions");
  std::map<std::array<uint32_t,2>,std::vector<Edge>> indexed;
  for(size_t c=0;c<corners;c+=3)for(size_t k=0;k<3;++k){
    work.step(16);auto a=mesh.indices[c+k],b=mesh.indices[c+(k+1)%3];
    if(a>=n||b>=n||a==b)throw std::invalid_argument("Geometric sewing triangle indices");
    indexed[{std::min(a,b),std::max(a,b)}].push_back({a,b,uint32_t(c/3)});
  }
  auto key=[&](uint32_t i){std::vector<uint32_t> out;out.reserve(3*(morphs.size()+1));
    auto add=[&](Vec3 p){for(float f:{p.x,p.y,p.z}){work.step();if(!std::isfinite(f))throw std::invalid_argument("Nonfinite sewing geometry");out.push_back(std::bit_cast<uint32_t>(f==0?0.f:f));}};
    add(mesh.positions[i]);for(const auto&d:morphs)add(d[i]);return out;};
  struct Less {weight_surface::Budget *work;bool operator()(const Key&a,const Key&b)const{work->step(4+a.first.size()+a.second.size()+b.first.size()+b.second.size());return a<b;}};
  std::map<Key,std::vector<Edge>,Less> boundary(Less{&work});
  for(const auto&[unused,edges]:indexed)if(edges.size()==1){(void)unused;work.step(8);auto e=edges.front();auto a=key(e.a),b=key(e.b);if(b<a)std::swap(a,b);boundary[{std::move(a),std::move(b)}].push_back(e);}
  GeometricBoundarySewing out;out.bytes=bytes;out.groups.resize(n);std::iota(out.groups.begin(),out.groups.end(),0);
  auto root=[&](uint32_t i){while(out.groups[i]!=i){work.step();out.groups[i]=out.groups[out.groups[i]];i=out.groups[i];}return i;};
  auto join=[&](uint32_t a,uint32_t b){a=root(a);b=root(b);if(a!=b)out.groups[std::max(a,b)]=std::min(a,b);};
  auto tri=[&](uint32_t f){volume_topology::Triangle t;for(size_t k=0;k<3;++k){auto p=mesh.positions[mesh.indices[f*3+k]];t[k]={p.x,p.y,p.z};}return t;};
  for(const auto&[unused,pair]:boundary){(void)unused;work.step(8);if(pair.size()!=2){out.ambiguous+=pair.size()>2;continue;}
    auto a=pair[0],b=pair[1];if(key(a.a)!=key(b.b)||key(a.b)!=key(b.a)||
      volume_topology::unexpected_contact(tri(a.face),tri(b.face),work)){++out.rejected;continue;}
    join(a.a,b.b);join(a.b,b.a);++out.pairs;
  }
  for(uint32_t i=0;i<n;++i)out.groups[i]=root(i);
  // Quotient may not collapse any actual triangle: only boundary duplicates.
  for(size_t c=0;c<corners;c+=3){work.step(8);auto a=out.groups[mesh.indices[c]],b=out.groups[mesh.indices[c+1]],d=out.groups[mesh.indices[c+2]];if(a==b||a==d||b==d)throw std::invalid_argument("Sewing would collapse a real triangle");}
  return out;
}
} // namespace dome::detail
