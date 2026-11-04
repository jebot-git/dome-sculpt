// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/polygon-color.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/color-preparation.hpp"
#include "dome/normal-preparation.hpp"
#include "normal-projection-identity.hpp"
namespace dome::detail {
// Validate against ordered REST geometry, before any skin/morph/world preview.
// Counts, a retained Origin, or positions alone cannot authenticate a seam.
inline std::vector<PolygonRgba> polygon_target_colors(
    const Mesh &mesh,const PolygonColorSurface &source,
    const std::function<bool()> &cancelled={}) {
  if(!source.topology || !source.field)
    throw std::invalid_argument("Current color projection requires immutable topology and corner field");
  PolygonLimits limits;limits.cancelled=cancelled;
  source.field->validate(*source.topology,limits);
  const auto derivative=source.topology->triangulate(limits);
  const auto &expected=derivative.mesh;
  if(mesh.indices!=expected.indices || mesh.uv!=expected.uv ||
     mesh.positions.size()!=expected.positions.size() || mesh.normals.size()!=expected.normals.size())
    throw std::invalid_argument("Current color projection lost its ordered REST/corner/UV authority");
  for(size_t i=0;i<mesh.positions.size();++i){
    if((i&1023)==0 && cancelled&&cancelled())throw std::runtime_error("Current color projection validation cancelled");
    const auto p=mesh.positions[i],q=expected.positions[i],n=mesh.normals[i],m=expected.normals[i];
    if(p.x!=q.x||p.y!=q.y||p.z!=q.z||n.x!=m.x||n.y!=m.y||n.z!=m.z)
      throw std::invalid_argument("Current color projection lost its exact REST geometry");
  }
  return source.field->packed(*source.topology,limits);
}
inline void polygon_color_owner(const PolygonAuthority *authority,
    const SourceColorPreparation *preparation,bool deep){
  const bool current=authority&&authority->active&&authority->colors;
  const bool projected=preparation&&preparation->current_descriptor();
  if(projected){
    const auto &source=*preparation->current_descriptor();
    if(!current||!source.topology||!source.field||source.field!=authority->colors||
       source.field.owner_before(authority->colors)||authority->colors.owner_before(source.field))
      throw std::invalid_argument("Prepared corner colors require their own active geometry field");
    if(deep&&source.topology->encode()!=authority->topology.encode())
      throw std::invalid_argument("Prepared corner topology differs from its exact current authority");
  }else if(current&&preparation)
    throw std::invalid_argument("Current polygon corner colors conflict with prepared color authority");
}
// Conservative cheap readiness; full ordered geometry and projection validation
// remain detached worker work. Prepared normal/artist/UV1 transfers are separate.
inline bool corner_bake_target_supported(const SculptObject &object) noexcept {
  if(object.material_bindings&&object.material_bindings->native_paint_sampling)return false;
  if(!object.polygon||!object.polygon->active||!object.polygon->colors)return true;
  if(!object.mesh_active||!object.painted||object.appearance)return false;
  const auto binding=object.material_bindings;
  if(binding&&binding->has_mtoon)return false;
  const auto normal=object.painted->normal_projection();
  const auto normal_recipe=binding?binding->normal_preparation:nullptr;
  if(bool(normal)!=bool(normal_recipe))return false;
  // A retained domain requires a new authenticated normal event to rebake.
  // Initial explicitly prepared attributes can use the ordinary Bake path.
  if(normal&&(normal->version!=1||!normal_recipe->projection||!binding->normal_frames||
      binding->normal_frames!=normal_recipe->target_frames||
      !normal_projection_identity(*normal,*normal_recipe->projection)))return false;
  if(!normal&&binding&&binding->normal_frames)return false;
  const auto recipe=binding?binding->color_preparation:nullptr;
  const auto projection=object.painted->color_projection();
  if(bool(recipe)!=bool(projection))return false;
  if(recipe&&(!recipe->projection||!recipe->current_descriptor()||!recipe->current_descriptor()->topology||
      recipe->current_descriptor()->field!=object.polygon->colors||
      recipe->receipt.source_field!=SourceColorField::PolygonCorners||
      recipe->projection->vertex_rgba!=projection->vertex_rgba))return false;
  if(const auto maps=object.painted->source_maps())for(const auto *slot:{&maps->color,&maps->metal_rough,&maps->emission,&maps->normal,&maps->occlusion})
    if(slot->image&&slot->texcoord!=0)return false;
  if(const auto baked=object.painted->baked();baked&&(baked->normal_interpretation!=(normal?BakedNormalInterpretation::PreparedAttributes:BakedNormalInterpretation::CanonicalMikk)||
      baked->color_interpretation!=(recipe?BakedColorInterpretation::ResolvedPbr:BakedColorInterpretation::VertexComponent)))return false;
  return true;
}
} // namespace dome::detail
