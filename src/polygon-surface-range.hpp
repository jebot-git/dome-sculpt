// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon-surface-preview.hpp"
#include "dome/paint-sampling.hpp"
#include "dome/document.hpp"
#include "affine-surface.hpp"
namespace dome::detail {
inline AffineTransform polygon_range_world(const SurfaceRange&r){return r.affine_world?*r.affine_world:r.world_frame_valid?affine_similarity(r.object_world):AffineTransform{};}
inline void validate_polygon_range(const Mesh&mesh,const SurfaceRange&r,
 std::span<const std::array<float,2>>paint,bool vertex_coordinates,
 std::span<const Vec3>normals={},std::span<const std::array<float,4>>tangents={},
 std::span<const std::array<float,4>>role_uv={},bool renderer=false,const std::function<bool()>&cancelled={}){
 const auto packet=r.textures?r.textures->polygon_surface:nullptr;
 const bool declared=r.material_bindings&&bool(r.material_bindings->polygon_surface_transfer);
 if(bool(packet)!=declared)throw std::invalid_argument("Polygon range omitted or fabricated its actual current field packet");if(!packet)return;
 if(!packet->matches(r.material_bindings,r.paint_sampling_attachment))throw std::invalid_argument("Polygon range lost its exact field/binding/paint attachment");packet->validate_maps(*r.textures);
 const auto world=polygon_range_world(r);const AffineInteractionFrame frame(world);packet->validate_world(world,1e-6,cancelled);
 const auto&source=*packet->attachment()->authored_geometry();
 if(r.first>mesh.indices.size()||r.count!=source.indices.size()||r.count>mesh.indices.size()-r.first||paint.size()!=(vertex_coordinates?mesh.positions.size():mesh.indices.size()))throw std::invalid_argument("Polygon range current corner dimensions differ from its owning field");
 if(renderer&&(!r.source_frame||r.canonical_frame||r.source_normal_only!=(packet->model()==PolygonSurfaceNormalModel::NativeNOnly)||normals.size()!=mesh.positions.size()||(!packet->tangents().empty()&&tangents.size()!=mesh.positions.size())||(!packet->normal_coordinates().empty()&&role_uv.size()!=mesh.positions.size())))throw std::invalid_argument("Polygon range omitted its actual before role streams");
 const auto same=[](Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};
 for(size_t c=0;c<r.count;++c){if((c&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Polygon field range validation cancelled");const auto logical=r.paint_winding_reversed&&c%3?c+(c%3==1?1:-1):c;const auto v=mesh.indices.at(r.first+c),sv=source.indices.at(logical);
  if(v<r.first_vertex||v>=mesh.positions.size()||!same(mesh.positions[v],placed_position(frame,source.positions.at(sv)))||mesh.uv.at(v)!=source.uv.at(sv))throw std::invalid_argument("Polygon range geometry/UV differs from its actual field and world context");
  if(paint[vertex_coordinates?v:r.first+c]!=packet->paint_coordinates().at(logical))throw std::invalid_argument("Polygon range writable grid coordinates differ from its actual field");
  if(renderer){
   if(!same(normals[v],placed_normal(frame,packet->normals().at(logical))))throw std::invalid_argument("Polygon range before normals differ from its genuine posed inputs");
   if(!packet->tangents().empty()&&tangents[v]!=placed_tangent(frame,packet->tangents().at(logical)))throw std::invalid_argument("Polygon range before tangent/W differs from its genuine posed inputs");
   if(!packet->normal_coordinates().empty()){const auto uv=packet->normal_coordinates().at(logical);if(role_uv[v]!=std::array<float,4>{uv[0],uv[1],uv[0],uv[1]})throw std::invalid_argument("Polygon range normal coordinates borrowed another paint/normal domain");}
  }
 }
}
} // namespace dome::detail
