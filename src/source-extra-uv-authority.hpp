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
  if(!maps || (maps->extra_uv_sets.empty()&&!paint.source_sampling_prepared()))return;
  maps->validate_surface(paint.mesh());
  if(!appearance.asset || appearance.source_vertices.size()!=paint.mesh().positions.size())
    throw std::invalid_argument("Additional source UVs lack their original vertex mapping");
  const ImportedPrimitive *primitive=nullptr;
  for(const auto &p:appearance.asset->primitives)if(p.source_mesh==appearance.source_mesh && p.source_primitive==appearance.source_primitive) {
    if(primitive)throw std::invalid_argument("Additional source UV primitive identity is ambiguous");
    primitive=&p;
  }
  if(!primitive)throw std::invalid_argument("Additional source UV primitive identity is missing");
  const auto original=paint.source_sampling_original();
  const PaintSourceMaps empty;
  const auto &source=original?(original->source_maps()?*original->source_maps():empty):*maps;
  const auto vertices=paint.source_sampling_vertices();
  if(original&&(!paint.valid_source_sampling_attachment()||vertices.size()!=paint.mesh().positions.size()))
    throw std::invalid_argument("Independent source UVs lost their actual original paint owner");
  auto pair=[&](const auto &values,const auto &authored){
    for(size_t v=0;v<appearance.source_vertices.size();++v) {
      if((v&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Additional source UV association cancelled");
      const auto before=original?vertices[v]:uint32_t(v),source_vertex=appearance.source_vertices[v];
      if(before>=values.size()||source_vertex>=authored.size()||
         std::bit_cast<std::array<uint32_t,2>>(values[before])!=std::bit_cast<std::array<uint32_t,2>>(authored[source_vertex]))
        throw std::invalid_argument("Additional source UV association differs from its original attribute");
    }
  };
  // Generated carriers are authenticated by the source-paint factory/replay.
  // Pair their genuine original arrays to the actual asset, rather than pretend
  // the new carrier number was an authored TEXCOORD_n accessor in that asset.
  if(original){
    if(!primitive->mesh.uv.empty())pair(original->mesh().uv,primitive->mesh.uv);
    else for(const auto *role:{&source.color,&source.metal_rough,&source.emission,&source.normal,&source.occlusion})
      if(role->image&&role->texcoord==0)throw std::invalid_argument("Original UV0 image role lacks its authored attribute");
    if(source.uv1)pair(*source.uv1,primitive->uv1);
    if(const auto uv1=paint.source_sampling_authored_uv1())pair(*uv1,primitive->uv1);
  }
  for(const auto &[set,values]:source.extra_uv_sets) {
    const auto found=primitive->extra_uv_sets.find(set);
    if(found==primitive->extra_uv_sets.end())throw std::invalid_argument("Additional source UV owner has no original attribute");
    pair(*values,found->second);
  }
}
} // namespace dome::detail
