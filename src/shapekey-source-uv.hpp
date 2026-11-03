// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/material-bindings.hpp"
#include "dome/paint.hpp"
#include "dome/humanoid-creator.hpp"
namespace dome::detail {
enum class ShapeFrameRole { Pbr, MToon, Canonical };
// Select the frame's captured coordinate authority, independently of the current
// preview and sampling atlas. A later UV atlas changes sampling, not these axes.
inline uint32_t shapekey_frame_coordinate(const SourceMaterialBindings &binding,
    const NormalFrames &frame, ShapeFrameRole role) {
  if(frame.generated_uv)return *frame.generated_uv;
  const auto &origin=binding.uv_origin;
  if(!origin||!origin->paint)throw std::invalid_argument("Shapekey frame has no retained native coordinate owner");
  if(role==ShapeFrameRole::Pbr) {
    const auto &maps=origin->paint->source_maps();
    if(maps&&maps->normal.image)return uint32_t(maps->normal.texcoord);
  }else if(role==ShapeFrameRole::MToon&&origin->material) {
    // MToon0's sampling ST does not change the captured tangent basis.
    if(origin->material->version()==MToonVersion::V0)return 0;
    const auto &slot=origin->material->textures[size_t(MToonSlot::Normal)];
    if(origin->material->parameters.normal_enabled&&slot.index>=0)
      return slot.transform_texcoord>=0?uint32_t(slot.transform_texcoord):slot.texcoord;
  }
  return 0;
}
inline std::vector<std::array<float,2>> shapekey_frame_coordinates(
    const SourceMaterialBindings &binding,uint32_t coordinate,
    const std::function<bool()> &cancelled) {
  if(coordinate>7||!binding.uv_origin||!binding.uv_origin->paint)
    throw std::invalid_argument("Shapekey frame coordinate authority is unsupported");
  const auto &origin=*binding.uv_origin;const auto &mesh=origin.paint->mesh();
  if(mesh.positions.size()>2'000'000)throw std::length_error("Shapekey frame coordinate memory budget exceeded");
  auto stop=[&](size_t v){if((v&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Shapekey frame coordinate capture cancelled");};
  if(coordinate==0)return mesh.uv;
  // PBR source maps own native-aligned copies. Typed-only roles may instead
  // resolve through the exact captured primitive/source-vertex identities.
  std::span<const std::array<float,2>> owned;
  if(const auto &maps=origin.paint->source_maps()) {
    if(coordinate==1&&maps->uv1)owned=*maps->uv1;
    else if(auto i=maps->extra_uv_sets.find(coordinate);i!=maps->extra_uv_sets.end()&&i->second)owned=*i->second;
  }
  const ImportedPrimitive *primitive=nullptr;
  if(origin.appearance) {
    const auto &a=*origin.appearance;
    if(!a.asset||a.source_vertices.size()!=mesh.positions.size())
      throw std::invalid_argument("Shapekey frame source-vertex coordinate identity is incomplete");
    for(const auto &p:a.asset->primitives)if(p.source_mesh==a.source_mesh&&p.source_primitive==a.source_primitive){
      if(primitive)throw std::invalid_argument("Shapekey frame source primitive is ambiguous");primitive=&p;
    }
    if(!primitive)throw std::invalid_argument("Shapekey frame original primitive is missing");
  }
  std::span<const std::array<float,2>> raw;
  if(primitive) {
    if(coordinate==1)raw=primitive->uv1;
    else if(auto i=primitive->extra_uv_sets.find(coordinate);i!=primitive->extra_uv_sets.end())raw=i->second;
    if(raw.size()!=primitive->mesh.positions.size())throw std::invalid_argument("Shapekey frame original UV set is missing");
  }
  if(!owned.empty()&&owned.size()!=mesh.positions.size())throw std::invalid_argument("Shapekey frame native coordinate domain differs");
  if(owned.empty()&&!primitive)throw std::invalid_argument("Shapekey frame has no owning coordinate domain");
  std::vector<std::array<float,2>> result;result.reserve(mesh.positions.size());
  for(size_t v=0;v<mesh.positions.size();++v){stop(v);
    if(owned.empty()&&origin.appearance->source_vertices[v]>=raw.size())throw std::invalid_argument("Shapekey frame source coordinate index is out of range");
    auto uv=owned.empty()?raw[origin.appearance->source_vertices[v]]:owned[v];
    if(primitive){const auto source=origin.appearance->source_vertices[v];if(source>=raw.size()||uv!=raw[source])throw std::invalid_argument("Shapekey frame coordinate differs from its captured original accessor");}
    if(!std::isfinite(uv[0])||!std::isfinite(uv[1]))throw std::invalid_argument("Shapekey frame coordinate is nonfinite");
    result.push_back(uv);
  }
  return result;
}
} // namespace dome::detail
