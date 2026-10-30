// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/humanoid-creator.hpp"
#include <functional>
namespace dome::detail {
// Colors are linear glTF RGBA, independently indexed from the current atlas.
// Never write through the original asset or infer identities from positions.
inline std::vector<std::array<float,4>> source_vertex_colors(
    const CreatorAppearance *appearance,size_t vertices,
    const std::function<bool()> &cancelled={}) {
  if(!appearance || !appearance->asset)return {};
  const ImportedPrimitive *primitive=nullptr;
  for(const auto &p:appearance->asset->primitives)
    if(p.source_mesh==appearance->source_mesh && p.source_primitive==appearance->source_primitive) {
      if(primitive)throw std::invalid_argument("Vertex-color source primitive is ambiguous");
      primitive=&p;
    }
  if(!primitive)throw std::invalid_argument("Vertex-color source primitive identity is unavailable");
  if(primitive->colors.empty())return {};
  if(vertices>2'000'000 || appearance->source_vertices.size()!=vertices || primitive->colors.size()!=primitive->mesh.positions.size())
    throw std::invalid_argument("Vertex-color authority or current source mapping is incomplete");
  std::vector<std::array<float,4>> result;result.reserve(vertices);
  for(size_t v=0;v<vertices;++v) {
    if((v&1023)==0 && cancelled && cancelled())throw std::runtime_error("Vertex-color mapping cancelled");
    const auto id=appearance->source_vertices[v];
    if(id>=primitive->colors.size())throw std::invalid_argument("Vertex-color source identity is outside its original accessor");
    auto color=primitive->colors[id];
    for(float c:color)if(!std::isfinite(c) || c<0 || c>1)throw std::invalid_argument("Original vertex RGBA must be finite linear unit values");
    result.push_back(color);
  }
  return result;
}
}
