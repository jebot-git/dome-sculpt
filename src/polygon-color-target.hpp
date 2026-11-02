// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/polygon-color.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/color-preparation.hpp"
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
  const bool projected=preparation&&preparation->polygon_colors;
  if(projected){
    const auto &source=*preparation->polygon_colors;
    if(!current||!source.topology||!source.field||source.field!=authority->colors||
       source.field.owner_before(authority->colors)||authority->colors.owner_before(source.field))
      throw std::invalid_argument("Prepared corner colors require their own active geometry field");
    if(deep&&source.topology->encode()!=authority->topology.encode())
      throw std::invalid_argument("Prepared corner topology differs from its exact current authority");
  }else if(current&&preparation)
    throw std::invalid_argument("Current polygon corner colors conflict with prepared color authority");
}
} // namespace dome::detail
