// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-worker.hpp"
#include "affine-surface.hpp"
#include "paint-overlay-packet.hpp"
#include "polygon-surface-preview.hpp"
#include <limits>
namespace dome::detail {
namespace {
void stop(const TextureSetLimits&l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Texture-set snapshot cancelled");}
void charge(size_t&t,size_t n,size_t unit,const TextureSetLimits&l){if(t>l.maximum_bytes||(unit&&n>(l.maximum_bytes-t)/unit))throw std::length_error("Texture-set snapshot exceeds its declared staging bytes");t+=n*unit;}
size_t map_bytes(const TextureMaps&m){
 size_t bytes=sizeof(TextureMaps);for(const auto*i:{&m.color,&m.metal_rough,&m.emission,&m.normal,&m.mtoon_normal,&m.color_coverage,&m.emission_coverage,&m.baked_normal,&m.baked_coverage,&m.pbr_color}){
  if(i->rgba.size()>SIZE_MAX-bytes)throw std::length_error("Texture-set map storage overflow");bytes+=i->rgba.size();
 }
 if(m.overlay){const auto extra=m.overlay->storage_bytes();if(extra>SIZE_MAX-bytes)throw std::length_error("Texture-set overlay storage overflow");bytes+=extra;}
 return bytes;
}
}
void append_texture_set_surface(MeshSnapshot&snapshot,const ObjectSurface&o,
    const std::shared_ptr<const TextureSetPackedGeometry>&layout,
    const TextureSetWorkerView&view,const TextureSetLimits&limits){
 limits.validate();stop(limits);const auto&plan=o.texture_set_surface;
 if(!plan||!layout)throw std::invalid_argument("Texture-set worker lacks its complete genuine surface/layout owners");
 plan->authenticate_owner(plan);layout->authenticate_owner(layout);layout->authenticate_plan(*plan);
 if(o.id!=plan->object()||o.deformation!=plan->deformation()||o.deformation.owner_before(plan->deformation())||plan->deformation().owner_before(o.deformation)||
    !o.mesh.positions.empty()||!o.mesh.indices.empty()||o.textures||o.appearance||o.material_bindings||o.polygon_colors||o.paint_sampling_attachment||!o.paint_corner_coordinates.empty()||o.normal_preview_proof||o.source_dormant)
  throw std::invalid_argument("Texture-set worker bundle fabricated or omitted independent geometry/field authority");
 const auto&posed=*layout->posed();const size_t vertices=layout->vertices().size(),corners=layout->indices().size();
 const size_t offset=snapshot.mesh.positions.size(),first=snapshot.mesh.indices.size();
 if(offset>UINT32_MAX||vertices>uint64_t(UINT32_MAX)+1-offset||corners>SIZE_MAX-first)throw std::length_error("Texture-set packed scene exceeds its address/index bound");
 if(view.selected&&view.weights){if(!view.weight_values||!view.weight_values->skin)throw std::invalid_argument("Texture-set weight view has no actual current candidate influences");view.weight_values->validate(*plan->geometry());}
 // Charge the complete existing output plus prospective streams, images and
 // range seals before preview/image allocation. Per-field geometry is absent.
 size_t bytes=layout->owned_bytes();charge(bytes,1,plan->preparation_bytes(),limits);
 charge(bytes,offset+vertices,2*sizeof(Vec3)+sizeof(std::array<float,2>)+sizeof(Tangent),limits);
 charge(bytes,first+corners,sizeof(uint32_t),limits);
 // The actual MaterialSurfaceQuery owns one geometry copy and its BVH. Node
 // growth is bounded by four allocated nodes per triangle (binary-tree vector
 // capacity), including order and temporary build bookkeeping. It owns no
 // per-set mesh/tint copy for these plain ranges.
 charge(bytes,offset+vertices,2*sizeof(Vec3)+sizeof(std::array<float,2>),limits);
 charge(bytes,first+corners,sizeof(uint32_t),limits);charge(bytes,(first+corners)/3,256,limits);
 if(layout->has_source()||!snapshot.source_normals.empty())charge(bytes,offset+vertices,2*sizeof(Vec3)+sizeof(Tangent),limits);
 if(!snapshot.source_colors.empty())charge(bytes,offset+vertices,sizeof(std::array<float,4>),limits);
 if(layout->has_source()||!snapshot.normal_coordinates.empty())charge(bytes,offset+vertices,sizeof(std::array<float,4>),limits);
 if(layout->has_source()||!snapshot.paint_coordinates.empty())charge(bytes,offset+vertices,sizeof(std::array<float,2>),limits);
 if(view.weights||!snapshot.selected_weights.empty())charge(bytes,offset+vertices,sizeof(float),limits);
 if(view.masks||!snapshot.sculpt_masks.empty())charge(bytes,offset+vertices,sizeof(float),limits);
 charge(bytes,snapshot.ranges.size()+plan->fields().size(),sizeof(SurfaceRange)+sizeof(TextureSetRangeProof)+128,limits);
 for(const auto&r:snapshot.ranges)if(r.textures)charge(bytes,1,map_bytes(*r.textures),limits);
 size_t preview_work=layout->preparation_work();
 if(preview_work>limits.maximum_work||corners>limits.maximum_work-preview_work||vertices>limits.maximum_work-preview_work-corners)throw std::length_error("Texture-set snapshot geometry exceeds its work bound");
 preview_work+=corners+vertices;
 for(const auto&field:plan->fields()){
  const size_t pixels=size_t(field.painted->resolution())*field.painted->resolution();
  // Plain composite maps, coverage, legacy normal and native F32 preview packet
  // plus its detached compositing scratch; complex source fields remain guarded.
  charge(bytes,pixels,128,limits);
  const auto layers=field.painted->layers().size();
  if(pixels&&(layers+1)>(limits.maximum_work-preview_work)/pixels/32)throw std::length_error("Texture-set field preview exceeds its charged work bound");
  preview_work+=pixels*(layers+1)*32;
 }
 const bool affine=layout->has_source()||bool(o.affine_edit)||std::any_of(o.groups.begin(),o.groups.end(),[](const auto&g){return g.source_placement||g.affine_edit;});
 const auto frame=affine?std::optional<AffineInteractionFrame>(placement_frame(o)):std::nullopt;
 const bool reflected=frame&&frame->orientation()==AffineOrientation::Reversing;const auto world=affine?CanvasTransform{}:o.world_transform();
 // Seal every field first. A late allocation/preview failure cannot expose a
 // partially populated range to the publisher or renderer.
 std::vector<SurfaceRange>ranges;ranges.reserve(plan->draws().size());
 for(size_t f=0;f<plan->draws().size();++f){
  stop(limits);if(!plan->draws()[f].count)continue;
  auto proof=prepare_texture_set_range(plan,layout,f,first,offset,limits.cancelled);
  SurfaceRange range{o.id,proof->first_index(),proof->draw().count,proof->field().material,proof->textures(),nullptr,offset};
  range.object_world=world;range.world_frame_valid=!affine;if(frame)range.affine_world=frame->world();
  if(const auto&packet=range.textures->polygon_surface){packet->validate_world(frame->world(),1e-6,limits.cancelled);range.material_bindings=proof->field().material_bindings;range.paint_sampling_attachment=packet->attachment();range.source_frame=true;range.source_normal_only=packet->model()==PolygonSurfaceNormalModel::NativeNOnly;}
  range.paint_winding_reversed=reflected;range.pbr_view=view.pbr;range.texture_set=proof->field().id;range.texture_set_proof=std::move(proof);
  ranges.push_back(std::move(range));
 }
 if(layout->has_source()){
  if(snapshot.source_normals.empty()){snapshot.source_normals=snapshot.mesh.normals;snapshot.canonical_normals=snapshot.mesh.normals;snapshot.source_tangents.assign(offset,{1,0,0,1});}
  if(snapshot.paint_coordinates.empty())snapshot.paint_coordinates=snapshot.mesh.uv;
  if(snapshot.normal_coordinates.empty())for(auto uv:snapshot.mesh.uv)snapshot.normal_coordinates.push_back({uv[0],uv[1],uv[0],uv[1]});
 }
 for(size_t p=0;p<vertices;++p){
  if((p&1023)==0)stop(limits);const auto vertex=layout->vertices()[p];const auto uv=posed.uv.at(vertex);const auto native_normal=posed.normals.at(vertex);
  const auto rotated_normal=ob::rotate(world.orientation,{native_normal.x,native_normal.y,native_normal.z});
  const auto normal=frame?placed_normal(*frame,native_normal):Vec3{rotated_normal.x,rotated_normal.y,rotated_normal.z};
  const auto position=posed.positions.at(vertex);const auto transformed=ob::transform(world,{position.x,position.y,position.z});
  snapshot.mesh.positions.push_back(frame?placed_position(*frame,position):Vec3{transformed.x,transformed.y,transformed.z});snapshot.mesh.normals.push_back(normal);snapshot.mesh.uv.push_back(uv);
  const auto tangent=layout->tangents()[p];const auto direction=ob::rotate(world.orientation,{tangent[0],tangent[1],tangent[2]});
  snapshot.tangents.push_back(frame?placed_tangent(*frame,tangent):Tangent{direction.x,direction.y,direction.z,tangent[3]});
  if(layout->has_source()||!snapshot.source_normals.empty()){
   const auto n=layout->has_source()?layout->source_normals()[p]:native_normal;const auto rotated=ob::rotate(world.orientation,{n.x,n.y,n.z});snapshot.source_normals.push_back(frame?placed_normal(*frame,n):Vec3{rotated.x,rotated.y,rotated.z});snapshot.canonical_normals.push_back(normal);
   const auto t=layout->has_source()?layout->source_tangents()[p]:Tangent{1,0,0,1};const auto direction=ob::rotate(world.orientation,{t[0],t[1],t[2]});snapshot.source_tangents.push_back(frame?placed_tangent(*frame,t):Tangent{direction.x,direction.y,direction.z,t[3]});
  }
  if(!snapshot.source_colors.empty())snapshot.source_colors.push_back({1,1,1,1});
  if(layout->has_source()||!snapshot.normal_coordinates.empty()){const auto role=layout->has_source()?layout->normal_coordinates()[p]:uv;snapshot.normal_coordinates.push_back({role[0],role[1],role[0],role[1]});}
  if(layout->has_source()||!snapshot.paint_coordinates.empty())snapshot.paint_coordinates.push_back(layout->has_source()?layout->paint_coordinates()[p]:uv);
  if(view.weights){float weight=view.selected?0.f:-1.f;if(view.selected)for(const auto&i:view.weight_values->skin->vertices.at(vertex))if(i.joint==view.joint)weight=i.weight;snapshot.selected_weights.push_back(weight);}
  if(view.masks)snapshot.sculpt_masks.push_back(view.mask?view.mask(vertex):-1.f);
 }
 for(size_t c=0;c<corners;c+=3){if((c&1023)==0)stop(limits);snapshot.mesh.indices.push_back(uint32_t(offset+layout->indices()[c]));snapshot.mesh.indices.push_back(uint32_t(offset+layout->indices()[c+(reflected?2:1)]));snapshot.mesh.indices.push_back(uint32_t(offset+layout->indices()[c+(reflected?1:2)]));}
 for(auto&range:ranges){range.texture_set_proof->authenticate_stream(snapshot.mesh,range);snapshot.ranges.push_back(std::move(range));}
 stop(limits);
}
} // namespace dome::detail
