// SPDX-License-Identifier: GPL-3.0-or-later
#include "corner-key-expansion.hpp"
#include <algorithm>
#include <tuple>
namespace dome::detail {
CornerKeyExpansion expand_corner_key_basis(const Mesh &current,const Mesh &origin,
    const CornerKeyExpansionLimits &limits) {
  if(!limits.max_vertices||limits.max_vertices>2'000'000||!limits.max_corners||limits.max_corners>6'000'000||
      !limits.max_bytes||limits.max_bytes>256*1024*1024||!limits.max_work||limits.max_work>256*1024*1024)
    throw std::invalid_argument("Invalid corner-key expansion resource policy");
  size_t work=0;auto stop=[&]{if(++work>limits.max_work)throw std::length_error("Corner-key expansion work exceeds budget");if((work==1||(work&1023)==0)&&limits.cancelled&&limits.cancelled())throw std::runtime_error("Corner-key expansion cancelled");};stop();
  const auto count=current.indices.size();
  if(count!=origin.indices.size())throw std::invalid_argument("Corner-key expansion requires exact ordered triangle correspondence");
  if(count>limits.max_corners||current.positions.size()>limits.max_vertices||origin.positions.size()>limits.max_vertices)
    throw std::length_error("Corner-key expansion geometry exceeds budget");
  current.validate();origin.validate();
  if(origin.uv.size()!=origin.positions.size())throw std::invalid_argument("Corner-key expansion needs its genuine frame-coordinate basis");
  struct Corner {uint32_t current,origin,corner;};
  size_t bytes=0;auto add=[&](size_t n,size_t stride){if(n>(limits.max_bytes-bytes)/stride)throw std::length_error("Corner-key expansion scratch exceeds budget");bytes+=n*stride;};
  add(count,sizeof(Corner));add(current.positions.size(),1);
  std::vector<Corner> corners;corners.reserve(count);std::vector<uint8_t> seen(current.positions.size());
  for(size_t c=0;c<count;++c){stop();auto a=current.indices[c],b=origin.indices[c];const auto x=current.positions[a],y=origin.positions[b];
    if(x.x!=y.x||x.y!=y.y||x.z!=y.z)throw std::invalid_argument("Corner-key expansion changed ordered REST position authority");
    corners.push_back({a,b,uint32_t(c)});seen[a]=1;
  }
  // Unreferenced native vertices need their own explicit original certificate;
  // corner equality cannot invent one. No live admission uses this kernel yet.
  if(std::find(seen.begin(),seen.end(),0)!=seen.end())throw std::invalid_argument("Corner-key expansion lacks an unreferenced native vertex certificate");
  std::sort(corners.begin(),corners.end(),[&](const Corner&a,const Corner&b){stop();return std::tie(a.current,a.origin,a.corner)<std::tie(b.current,b.origin,b.corner);});
  size_t vertices=0;uint32_t previous_current=UINT32_MAX,previous_origin=UINT32_MAX;
  for(const auto &c:corners){stop();if(c.current!=previous_current||c.origin!=previous_origin){++vertices;previous_current=c.current;previous_origin=c.origin;}}
  if(vertices>limits.max_vertices)throw std::length_error("Corner-key derived vertex count exceeds budget");
  add(vertices,2*sizeof(Vec3)+sizeof(std::array<float,2>)+2*sizeof(uint32_t)+(origin.uv_pins.empty()?0:1));add(count,sizeof(uint32_t)+(origin.uv_seams.empty()?0:1));
  CornerKeyExpansion out;out.current_vertices.reserve(vertices);out.origin_vertices.reserve(vertices);
  auto &mesh=out.basis;mesh.positions.reserve(vertices);mesh.normals.reserve(vertices);mesh.uv.reserve(vertices);if(!origin.uv_pins.empty())mesh.uv_pins.reserve(vertices);
  mesh.indices.resize(count);mesh.uv_seams=origin.uv_seams;previous_current=previous_origin=UINT32_MAX;
  for(const auto &c:corners){stop();if(c.current!=previous_current||c.origin!=previous_origin){
      out.current_vertices.push_back(c.current);out.origin_vertices.push_back(c.origin);mesh.positions.push_back(origin.positions[c.origin]);mesh.normals.push_back(origin.normals[c.origin]);mesh.uv.push_back(origin.uv[c.origin]);if(!origin.uv_pins.empty())mesh.uv_pins.push_back(origin.uv_pins[c.origin]);previous_current=c.current;previous_origin=c.origin;
    }mesh.indices[c.corner]=uint32_t(out.current_vertices.size()-1);
  }
  mesh.validate();if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Corner-key expansion cancelled");return out;
}
} // namespace dome::detail
