// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-worker.hpp"
#include "affine-surface.hpp"
#include "baked-normal-field.hpp"
#include "paint-overlay-packet.hpp"
#include "current-paint-preview.hpp"
#include "polygon-surface-preview.hpp"
#include "texture-set-paint-access.hpp"
#include "texture-set-field-source.hpp"
#include "texture-set-native-grid.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <set>
namespace dome::detail {
namespace {
void stop(const TextureSetLimits&l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Texture-set snapshot cancelled");}
void charge(size_t&t,size_t n,size_t unit,const TextureSetLimits&l){if(t>l.maximum_bytes||(unit&&n>(l.maximum_bytes-t)/unit))throw std::length_error("Texture-set snapshot exceeds its declared staging bytes");t+=n*unit;}
void charge_preview_work(size_t &work,size_t count,size_t cost,const TextureSetLimits&limits){
 if(work>limits.maximum_work||count>(limits.maximum_work-work)/cost)
  throw std::length_error("Texture-set field preview exceeds its charged work bound");
 work+=count*cost;
}
using NormalHistoryOwners=std::set<std::shared_ptr<const BakedNormalField>,std::owner_less<>>;
void charge_grid_capture(size_t&bytes,size_t&work,const TextureSetView&field,const TextureSetLimits&limits,
    bool classify_alpha,NormalHistoryOwners *normal_history=nullptr){
 const auto&paint=*field.painted;
 const size_t tile_area=PaintTile::size*PaintTile::size;
 const size_t pixels=size_t(paint.resolution())*paint.resolution(),tiles=pixels/tile_area;
 const auto &layers=paint.layers();
 const auto visible=[](const PaintLayer&layer){return layer.visible&&layer.opacity>0;};
 const size_t visible_layers=std::count_if(layers.begin(),layers.end(),visible);
 const bool transparent_base=classify_alpha&&field.material.color[3]<.999f;
 size_t storage_entries=0;
 for(const auto&layer:layers)
  storage_entries+=layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size();
 // Charge the retained raster, sparse storage, copied directories and the
 // temporary ordered owner sets used by capture. Include cold-cache work.
 const auto capture_bytes=normal_history?TextureSetPaintAccess::snapshot_bytes(paint):
     TextureSetPaintAccess::working_bytes(paint,paint.resolution());
 charge(bytes,1,capture_bytes,limits);
 charge(bytes,tiles,visible_layers*3*sizeof(std::shared_ptr<const PaintTile>)+8,limits);
 charge_preview_work(work,storage_entries,8,limits);
 charge_preview_work(work,tiles,2+2*visible_layers,limits);
 if(transparent_base)charge_preview_work(work,pixels,1,limits);
 size_t scan_tiles=0,owners=0;
 for(size_t tile=0;tile<tiles;++tile){
  if((tile&63)==0)stop(limits);
  bool common=false;
  for(const auto&layer:layers)if(visible(layer)){
   common|=bool(layer.tiles[tile]);
   owners+=bool(layer.tiles[tile]);
   owners+=!layer.pbr_tiles.empty()&&bool(layer.pbr_tiles[tile]);
   owners+=!layer.emission_tiles.empty()&&bool(layer.emission_tiles[tile]);
  }
  scan_tiles+=transparent_base||common;
 }
 charge(bytes,owners,6*sizeof(void*),limits);
 auto storage=paint.storage_bytes();
 if(normal_history&&paint.baked()){
  for(auto node=paint.baked()->normal_field;node;node=node->prior()){
   stop(limits);
   if(!normal_history->insert(node).second){
    storage-=node->retained_bytes();
    break;
   }
   charge(bytes,1,sizeof(std::shared_ptr<const BakedNormalField>)+4*sizeof(void*),limits);
  }
 }
 charge(bytes,1,storage,limits);
 charge_preview_work(work,owners,2*std::bit_width(owners)+4,limits);
 if(classify_alpha)charge_preview_work(work,scan_tiles*tile_area,1+8*visible_layers,limits);
}
size_t map_bytes(const TextureMaps&m){
 size_t bytes=sizeof(TextureMaps);for(const auto*i:{&m.color,&m.metal_rough,&m.emission,&m.normal,&m.mtoon_normal,&m.color_coverage,&m.emission_coverage,&m.baked_normal,&m.baked_coverage,&m.pbr_color}){
  if(i->rgba.size()>SIZE_MAX-bytes)throw std::length_error("Texture-set map storage overflow");bytes+=i->rgba.size();
 }
 if(m.overlay){const auto extra=m.overlay->storage_bytes();if(extra>SIZE_MAX-bytes)throw std::length_error("Texture-set overlay storage overflow");bytes+=extra;}
 if(m.current_grid_preview){const auto extra=m.current_grid_preview->layers()->storage_bytes();if(extra>SIZE_MAX-bytes)throw std::length_error("Texture-set current-grid storage overflow");bytes+=extra;}
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
 std::set<std::shared_ptr<const void>,std::owner_less<>> retained_rasters;
 NormalHistoryOwners normal_history;
 for(size_t i=0;i<plan->fields().size();++i){
  if(!plan->draws()[i].count)continue;
  const auto&field=plan->fields()[i];
  const size_t pixels=size_t(field.painted->resolution())*field.painted->resolution();
  const auto layers=field.painted->layers().size();
  if(view.render_only&&CurrentGridPaintPreview::eligible_field(plan->object(),field)){
   charge_grid_capture(bytes,preview_work,field,limits,true);
  }else if(view.render_only&&!field.source&&field.painted->baked()&&field.painted->baked()->emission_field&&native_hdr_grid(*field.painted)){
   charge_grid_capture(bytes,preview_work,field,limits,false);
   charge(bytes,1,native_material_grid_bytes(*field.painted,TextureSetGridMode::NativeHdr),limits);
   charge_native_material_grid_work(*field.painted,preview_work,limits,TextureSetGridMode::NativeHdr);
  }else if(view.render_only&&retained_material_grid(field)){
   const auto read=field.source->read_view(field,limits);
   const auto normals=retained_hdr_normal_bytes(field);
   charge_grid_capture(bytes,preview_work,field,limits,false,&normal_history);
   // The plan retains each member raster and the packet aliases its read
   // raster. Neither capture allocates a replacement raster.
   for(const auto *paint:{field.painted.get(),read.get()}){
    const auto owner=TextureSetPaintAccess::raster_owner(*paint);
    if(retained_rasters.insert(owner).second)
     charge(bytes,1,PaintOverlayFactory::raster_storage_bytes(*paint),limits);
   }
   const auto mode=retained_grid_mode(field);
   charge(bytes,1,native_material_grid_bytes(*read,mode),limits);
   charge(bytes,1,normals,limits);
   charge_preview_work(preview_work,normals/2,1,limits);
   charge_native_material_grid_work(*read,preview_work,limits,mode);
  }else{
   charge(bytes,pixels,128,limits);
   charge_preview_work(preview_work,pixels,(layers+1)*32,limits);
  }
 }
 const bool affine=layout->has_source()||bool(o.affine_edit)||std::any_of(o.groups.begin(),o.groups.end(),[](const auto&g){return g.source_placement||g.affine_edit;});
 const auto frame=affine?std::optional<AffineInteractionFrame>(placement_frame(o)):std::nullopt;
 const bool reflected=frame&&frame->orientation()==AffineOrientation::Reversing;const auto world=affine?CanvasTransform{}:o.world_transform();
 // Seal every field first. A late allocation/preview failure cannot expose a
 // partially populated range to the publisher or renderer.
 std::vector<SurfaceRange>ranges;ranges.reserve(plan->draws().size());
 for(size_t f=0;f<plan->draws().size();++f){
  stop(limits);if(!plan->draws()[f].count)continue;
  auto proof=prepare_texture_set_range(plan,layout,f,first,offset,limits.cancelled,view.render_only);
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
