// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/source-maps.hpp"
#include "paint-overlay-packet.hpp"
#include "source-extra-uv-authority.hpp"
#include "source-paint-pairing.hpp"
namespace dome::detail {
// Export compilation owns a detached coordinate view. The native writable UV
// atlas and its exact original/current correspondence remain untouched.
inline void source_paint_export(const PaintedMesh &paint,ObjectSurface &out,
    const std::function<bool()> &cancelled={}) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Source paint export cancelled");};
  stop();
  if(!paint.source_sampling_prepared()||!paint.valid_source_sampling_attachment())
    throw std::invalid_argument("Source paint export lost its immutable original sampling owner");
  source_paint_pairing(&paint,out.material_bindings.get(),out.appearance.get(),false);
  if(out.appearance)extra_source_uv_pair(paint,*out.appearance,cancelled);
  const auto marker=out.textures;
  if(!marker||!marker->source_only||!marker->overlay||
      marker->overlay->source_maps()!=paint.source_maps())
    throw std::invalid_argument("Source paint export requires its actual captured contribution packet");
  if(!marker->overlay->empty())
    throw std::invalid_argument("Painted independent source sampling requires an explicit portable material bake before standard export; the complete native project is retained");
  const auto before=paint.source_sampling_original();
  const auto vertices=paint.source_sampling_vertices();
  if(!before||vertices.size()!=out.mesh.positions.size()||out.mesh.indices!=paint.mesh().indices)
    throw std::invalid_argument("Source paint export vertex domain differs from its captured owner");
  const PaintSourceMaps empty;
  const auto &original=before->source_maps()?*before->source_maps():empty;
  const size_t arrays=1+bool(original.uv1)+original.extra_uv_sets.size();
  constexpr size_t bytes_limit=128*1024*1024;
  if(vertices.size()>2'000'000||arrays>8||vertices.size()>bytes_limit/(arrays*sizeof(std::array<float,2>)))
    throw std::length_error("Source paint export coordinate staging exceeds its bounded resource policy");
  auto remap=[&](const auto &values){
    if(values.size()!=before->mesh().positions.size())
      throw std::invalid_argument("Source paint export original coordinate domain is incomplete");
    std::vector<std::array<float,2>> result;result.reserve(vertices.size());
    for(size_t v=0;v<vertices.size();++v){if((v&1023)==0)stop();result.push_back(values.at(vertices[v]));}
    return result;
  };
  auto source=std::make_shared<PaintSourceMaps>(original);
  if(original.uv1)source->uv1=std::make_shared<const std::vector<std::array<float,2>>>(remap(*original.uv1));
  for(auto &[set,values]:source->extra_uv_sets)
    values=std::make_shared<const std::vector<std::array<float,2>>>(remap(*values));
  out.mesh.uv=remap(before->mesh().uv);
  // Pins belong to the writable atlas; none is an authored source-coordinate
  // constraint and glTF has no native pin channel.
  out.mesh.uv_pins.clear();out.mesh.uv_seams.clear();
  source->validate_surface(out.mesh);
  auto textures=std::make_shared<TextureMaps>();textures->source_only=true;
  textures->source_maps=std::move(source);textures->transparent=marker->transparent;
  out.textures=std::move(textures);
  stop();
}
} // namespace dome::detail
