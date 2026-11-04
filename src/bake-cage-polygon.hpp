// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/polygon-scene.hpp"
#include "dome/polygon-source.hpp"
#include <numeric>
namespace dome::detail {
// Polygon Apply packs one vertex per face corner. Recover identity from the
// retained initial topology, never from nearby/equal current positions.
inline std::vector<uint32_t> polygon_bake_cage_vertices(
    const SculptObject &target_object, const SculptObject &cage,
    const Mesh &target, const std::function<bool()> &cancelled) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Bake cage cancelled");};
  auto refuse=[] {throw std::invalid_argument(
    "Bake cage needs unchanged target topology and its retained original source; duplicate the target before moving cage vertices");};
  auto same_mesh=[&](const Mesh &a,const Mesh &b){
    if(a.positions.size()!=b.positions.size()||a.indices!=b.indices)return false;
    for(size_t v=0;v<a.positions.size();++v){
      if((v&1023)==0)stop();
      const auto p=a.positions[v],q=b.positions[v];
      if(p.x!=q.x||p.y!=q.y||p.z!=q.z)return false;
    }
    return true;
  };
  stop();
  if(!cage.polygon||!cage.polygon->active)refuse();
  PolygonLimits limits;limits.cancelled=cancelled;
  validate_polygon_derivative(*cage.polygon,cage.painted.get(),cage.mesh_active,limits);
  std::optional<PolygonSourceTopology> prepared;
  const PolygonMesh *reference=nullptr;
  std::vector<uint32_t> source_vertices;
  if(target_object.polygon&&target_object.polygon->active&&target_object.painted&&
      same_mesh(target_object.painted->mesh(),target)){
    validate_polygon_derivative(*target_object.polygon,target_object.painted.get(),
      target_object.mesh_active,limits);
    reference=&target_object.polygon->topology;
    source_vertices.resize(target.positions.size());
    std::iota(source_vertices.begin(),source_vertices.end(),0u);
  }else{
    const auto &origin=cage.polygon->origin;
    if(!origin||!origin->mesh_active||!origin->paint||
        !same_mesh(origin->paint->mesh(),target))refuse();
    // Reconstruct the exact initial IDs/corners used by the polygon worker.
    // An already-polygonal origin supplies its authority directly.
    if(origin->polygon&&origin->polygon->active){
      validate_polygon_derivative(*origin->polygon,origin->paint.get(),true,limits);
      reference=&origin->polygon->topology;
      source_vertices.resize(target.positions.size());
      std::iota(source_vertices.begin(),source_vertices.end(),0u);
    }else{
      prepared=prepare_polygon_topology(origin->paint->mesh(),origin->appearance.get(),limits);
      reference=&prepared->topology;
      source_vertices=std::move(prepared->source_vertices);
    }
  }
  const auto &current=cage.polygon->topology;
  if(reference->vertices().size()!=current.vertices().size()||
      reference->faces().size()!=current.faces().size())refuse();
  for(size_t v=0;v<reference->vertices().size();++v){
    if((v&1023)==0)stop();
    const auto &a=reference->vertices()[v],&b=current.vertices()[v];
    if(a.id!=b.id||a.provenance!=b.provenance)refuse();
  }
  size_t corner=0;
  for(size_t f=0;f<reference->faces().size();++f){
    const auto &a=reference->faces()[f],&b=current.faces()[f];
    if(a.id!=b.id||a.corners.size()!=b.corners.size())refuse();
    for(size_t c=0;c<a.corners.size();++c,++corner){
      if((corner&1023)==0)stop();
      if(a.corners[c].id!=b.corners[c].id||a.corners[c].vertex!=b.corners[c].vertex)refuse();
    }
  }
  if(source_vertices.size()!=corner||corner!=cage.painted->mesh().positions.size())refuse();
  // Every duplicate corner of one original vertex must have one logical ID.
  // Positions may legitimately separate only when the source already did.
  std::vector<uint32_t> mapping(target.positions.size(),UINT32_MAX);
  std::vector<PolygonId> logical(target.positions.size());
  corner=0;
  for(const auto &face:current.faces())for(const auto &c:face.corners){
    if((corner&1023)==0)stop();
    const auto source=source_vertices[corner];
    if(source>=mapping.size())refuse();
    if(mapping[source]!=UINT32_MAX&&logical[source]!=c.vertex)refuse();
    mapping[source]=uint32_t(corner++);logical[source]=c.vertex;
  }
  for(auto vertex:mapping)if(vertex==UINT32_MAX)refuse();
  stop();return mapping;
}
} // namespace dome::detail
