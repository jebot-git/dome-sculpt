// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "normal-frame-runtime.hpp"
#include <cmath>

namespace dome::detail {
struct CoordinateFrameLayout {
  NormalFrameLayout frames;
  std::vector<std::array<float,2>> coordinates;
};

// Refine an already authenticated complete frame layout. This is a data-layout
// operation, not paint/normal/source authority. The caller must first validate
// the owning REST geometry and coordinate witness. Never use this result to
// attach an old REST witness to expanded geometry as if it were new authority.
inline CoordinateFrameLayout split_frame_corner_coordinates(const Mesh &mesh,
    const NormalFrameLayout &base,std::span<const std::array<float,2>> corners,
    const std::function<bool()> &cancelled={},size_t max_work=128*1024*1024,
    size_t max_vertices=2'000'000) {
  if(!max_work||max_work>256*1024*1024||!max_vertices||max_vertices>2'000'000||
      base.vertices.size()>max_vertices||mesh.indices.size()>6'000'000)
    throw std::length_error("Coordinate frame export resource policy exceeded");
  if(mesh.indices.size()%3||base.indices.size()!=mesh.indices.size()||
      corners.size()!=mesh.indices.size()||base.corners.size()!=base.vertices.size())
    throw std::invalid_argument("Coordinate frame export dimensions differ");
  size_t work=0;
  auto visit=[&]{
    if(++work>max_work)throw std::length_error("Coordinate frame export work exceeded");
    if((work==1||(work&1023)==0)&&cancelled&&cancelled())
      throw std::runtime_error("Coordinate frame export cancelled");
  };
  for(size_t v=0;v<base.vertices.size();++v){
    visit();const auto c=base.corners[v];
    if(c>=base.indices.size()||base.indices[c]!=v||
        base.vertices[v]>=mesh.positions.size()||mesh.indices[c]!=base.vertices[v])
      throw std::invalid_argument("Coordinate frame representative differs from REST");
  }
  for(size_t c=0;c<mesh.indices.size();++c){
    visit();const auto v=base.indices[c];
    if(v>=base.vertices.size()||base.vertices[v]!=mesh.indices[c]||
        !std::isfinite(corners[c][0])||!std::isfinite(corners[c][1]))
      throw std::invalid_argument("Coordinate frame corner differs from REST");
  }
  CoordinateFrameLayout out;
  out.frames.indices.resize(mesh.indices.size());
  const auto capacity=std::min(mesh.indices.size(),max_vertices);
  out.frames.vertices.reserve(capacity);out.frames.corners.reserve(capacity);
  out.coordinates.reserve(capacity);
  struct Key {
    uint32_t frame,x,y;
    bool operator==(const Key &) const = default;
  };
  struct Hash {
    size_t operator()(Key k)const noexcept{
      uint64_t hash=1469598103934665603ull;
      for(auto v:{k.frame,k.x,k.y})hash=(hash^v)*1099511628211ull;
      return size_t(hash);
    }
  };
  std::unordered_map<Key,uint32_t,Hash> vertices;
  vertices.reserve(std::min(base.vertices.size(),capacity));
  for(size_t c=0;c<mesh.indices.size();++c){
    visit();const auto uv=corners[c];
    const Key key{base.indices[c],std::bit_cast<uint32_t>(uv[0]),std::bit_cast<uint32_t>(uv[1])};
    auto found=vertices.find(key);
    if(found==vertices.end()){
      if(out.frames.vertices.size()==max_vertices)
        throw std::length_error("Coordinate frame split exceeds retained-frame vertex policy");
      const auto vertex=uint32_t(out.frames.vertices.size());
      vertices.emplace(key,vertex);out.frames.vertices.push_back(mesh.indices[c]);
      out.frames.corners.push_back(uint32_t(c));out.coordinates.push_back(uv);
      out.frames.indices[c]=vertex;
    }else out.frames.indices[c]=found->second;
  }
  if(cancelled&&cancelled())throw std::runtime_error("Coordinate frame export cancelled");
  return out;
}
} // namespace dome::detail
