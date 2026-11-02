// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/humanoid-creator.hpp"
#include "dome/paint.hpp"
#include "dome/source-maps.hpp"
#include <bit>
#include <cmath>
namespace dome::detail {
inline constexpr size_t extra_source_uv_budget=112*1024*1024;
// Charge every retained raw array, including attributes unused by materials.
// Source-map owners have a separate paint-storage charge.
inline size_t extra_source_uv_storage(const ImportedAsset &asset,
    const std::function<bool()> &cancelled={}) {
  size_t bytes=0,work=0;
  if(asset.primitives.size()>65536)throw std::length_error("Original primitive table exceeds native bounds");
  for(const auto &p:asset.primitives) {
    if(p.extra_uv_sets.size()>6)throw std::length_error("Original additional UV set count exceeds bounds");
    if(!p.extra_uv_sets.empty() && (p.mesh.uv.size()!=p.mesh.positions.size() || p.uv1.size()!=p.mesh.positions.size()))
      throw std::invalid_argument("Original indexed texture coordinates must include UV0 and UV1");
    uint32_t expected=2;
    for(const auto &[set,values]:p.extra_uv_sets) {
      if(set!=expected++)throw std::invalid_argument("Original indexed texture coordinate attributes must be contiguous");
      if(set<2 || set>7 || values.empty() || values.size()!=p.mesh.positions.size() || values.size()>2'000'000)
        throw std::invalid_argument("Original additional UV array differs from its source vertex domain");
      const size_t stored=values.size()*sizeof(values.front())+sizeof(std::pair<const uint32_t,std::vector<std::array<float,2>>>)+4*sizeof(void*);
      if(stored>extra_source_uv_budget-bytes)throw std::length_error("Original additional UV storage exceeds bounds");
      bytes+=stored;
      for(const auto &uv:values) {
        if((++work&1023)==0 && cancelled && cancelled())throw std::runtime_error("Original additional UV validation cancelled");
        if(!std::isfinite(uv[0]) || !std::isfinite(uv[1]) || std::abs(uv[0])>10000 || std::abs(uv[1])>10000)
          throw std::invalid_argument("Original additional UV coordinates are outside native bounds");
      }
    }
  }
  if(cancelled && cancelled())throw std::runtime_error("Original additional UV validation cancelled");
  return bytes;
}
inline void extra_source_uv_pair(const PaintedMesh &paint,const CreatorAppearance &appearance,
    const std::function<bool()> &cancelled={}) {
  const auto &maps=paint.source_maps();
  if(!maps || maps->extra_uv_sets.empty())return;
  maps->validate_surface(paint.mesh());
  if(!appearance.asset || appearance.source_vertices.size()!=paint.mesh().positions.size())
    throw std::invalid_argument("Additional source UVs lack their original vertex mapping");
  const ImportedPrimitive *primitive=nullptr;
  for(const auto &p:appearance.asset->primitives)if(p.source_mesh==appearance.source_mesh && p.source_primitive==appearance.source_primitive) {
    if(primitive)throw std::invalid_argument("Additional source UV primitive identity is ambiguous");
    primitive=&p;
  }
  if(!primitive)throw std::invalid_argument("Additional source UV primitive identity is missing");
  for(const auto &[set,values]:maps->extra_uv_sets) {
    const auto found=primitive->extra_uv_sets.find(set);
    if(found==primitive->extra_uv_sets.end())throw std::invalid_argument("Additional source UV owner has no original attribute");
    for(size_t v=0;v<values->size();++v) {
      if((v&1023)==0 && cancelled && cancelled())throw std::runtime_error("Additional source UV association cancelled");
      const auto original=appearance.source_vertices[v];
      if(original>=found->second.size() || std::bit_cast<std::array<uint32_t,2>>(values->at(v))!=std::bit_cast<std::array<uint32_t,2>>(found->second[original]))
        throw std::invalid_argument("Additional source UV association differs from its original attribute");
    }
  }
}
} // namespace dome::detail
