// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
#include <span>
namespace dome::detail {
struct PaintOverlayLayer {
 float opacity=1;
 std::vector<std::shared_ptr<const PaintTile>> tiles;
 std::vector<std::shared_ptr<const PaintColorTile>> pbr_tiles;
 std::vector<std::shared_ptr<const PaintEmissionTile>> emission_tiles;
};
// Factory-captured immutable sparse inputs to the GPU compiler. There is no
// current PaintedMesh/Scene back-edge, serialized seal or dense CPU float image.
class PaintOverlayPacket {
 friend struct PaintOverlayFactory;
 uint32_t resolution_=0;bool empty_=true;size_t storage_bytes_=0;
 std::vector<PaintOverlayLayer> layers_;
 std::shared_ptr<const std::vector<int32_t>> padding_;
 std::shared_ptr<const BakedTextures> baked_;
 std::shared_ptr<const PaintSourceMaps> source_maps_;
 // Logical authored coordinates are independent of writable atlas UV0 and of
 // any generated source-image carrier. Only the sealed Source4 factory owns
 // their correspondence; packed wrappers own their actual expanded domain.
 std::shared_ptr<const std::vector<std::array<float,2>>> original_uv0_,original_uv1_;
 std::shared_ptr<const PaintOverlayPacket> contribution_owner_;
 // Independent native emission-only interpretation, never original Source4 UV.
 std::shared_ptr<const Mesh> native_geometry_;
 std::shared_ptr<const PaintSamplingDomain> native_domain_;
 // Distinct Source8 proof: exact current geometry paired with the immutable
 // baked owner by SourcePaintAccess, never a native32 or raw UV0 permission.
 std::shared_ptr<const Mesh> source_baked_geometry_;
 std::shared_ptr<const Image> source_baked_normal_,source_baked_coverage_;
 PaintOverlayPacket()=default;
public:
 const std::shared_ptr<const Image>&source_baked_normal()const noexcept{return contribution_owner_?contribution_owner_->source_baked_normal():source_baked_normal_;}
 const std::shared_ptr<const Image>&source_baked_coverage()const noexcept{return contribution_owner_?contribution_owner_->source_baked_coverage():source_baked_coverage_;}
 const std::shared_ptr<const Mesh>&source_baked_geometry()const noexcept{return contribution_owner_?contribution_owner_->source_baked_geometry():source_baked_geometry_;}
 const std::shared_ptr<const Mesh>&native_geometry()const noexcept{return contribution_owner_?contribution_owner_->native_geometry():native_geometry_;}
 const std::shared_ptr<const PaintSamplingDomain>&native_domain()const noexcept{return contribution_owner_?contribution_owner_->native_domain():native_domain_;}
 uint32_t resolution()const noexcept{return contribution_owner_?contribution_owner_->resolution():resolution_;}
 bool empty()const noexcept{return contribution_owner_?contribution_owner_->empty():empty_;}
 size_t storage_bytes()const noexcept{return storage_bytes_+(contribution_owner_?contribution_owner_->storage_bytes():0);}
 std::span<const PaintOverlayLayer> layers()const noexcept{return contribution_owner_?contribution_owner_->layers():std::span<const PaintOverlayLayer>(layers_);}
 const std::shared_ptr<const std::vector<int32_t>>&padding()const noexcept{return contribution_owner_?contribution_owner_->padding():padding_;}
 const std::shared_ptr<const BakedTextures>&baked()const noexcept{return contribution_owner_?contribution_owner_->baked():baked_;}
 const std::shared_ptr<const PaintSourceMaps>&source_maps()const noexcept{return source_maps_;}
 const std::shared_ptr<const std::vector<std::array<float,2>>>&original_uv0()const noexcept{return original_uv0_;}
 const std::shared_ptr<const std::vector<std::array<float,2>>>&original_uv1()const noexcept{return original_uv1_;}
 // Packed coordinate metadata shares one actual immutable contribution owner.
 const std::shared_ptr<const PaintOverlayPacket>&contribution_owner()const noexcept{return contribution_owner_;}
};
// Validate a Source8 render view against the factory-owned original/current
// packet and carriers. A copied public marker cannot replace these owners.
inline bool source_baked_preview_metadata(const TextureMaps &maps){
 const auto &p=maps.overlay;if(!p||!p->source_baked_geometry())return false;
 const auto &root=p->contribution_owner()?p->contribution_owner():p;
 if(!maps.source_only||!maps.source_maps||!maps.baked_base||maps.baked_base!=p->baked()||
    maps.source_maps!=p->source_maps()||!root->original_uv0()||
    root->original_uv0()->size()!=root->source_baked_geometry()->positions.size()||
    (bool(p->source_baked_normal())!=!p->baked()->maps.normal.rgba.empty())||
    bool(p->source_baked_normal())!=bool(p->source_baked_coverage()))
   throw std::invalid_argument("Source8 preview differs from its actual immutable source/baked/carrier owners");
 if(p->source_baked_normal()){
   p->source_baked_normal()->validate();p->source_baked_coverage()->validate();
   for(const auto &i:{p->source_baked_normal(),p->source_baked_coverage()})
     if(i->width!=p->resolution()||i->height!=p->resolution())throw std::invalid_argument("Source8 normal carrier changed its current atlas domain");
 }
 return true;
}
// Captured layers already exclude hidden and zero-opacity metadata. A real
// float tile keeps its interpretation even when its current value is zero.
inline bool overlay_uses_float_emission(const PaintOverlayPacket &packet) noexcept {
  if(packet.baked()&&packet.baked()->emission_field)return true;
  for(const auto &layer:packet.layers())for(const auto &tile:layer.emission_tiles)if(tile)return true;
  return false;
}
struct PaintOverlayFactory {
 static std::shared_ptr<const TextureMaps> capture_source_baked(const PaintedMesh &,const std::function<bool()> &cancelled={});
 static std::shared_ptr<const TextureMaps> capture_current_grid(const PaintedMesh &,
     const Material &,const std::function<bool()> &cancelled={});
 // Read only the sealed original coordinate owners; no sparse tile packet or
 // padding upload is needed by a CPU asset worker.
 static std::array<std::shared_ptr<const std::vector<std::array<float,2>>>,2>
   source_coordinates(const PaintedMesh &);
 static std::shared_ptr<const PaintOverlayPacket> capture(const PaintedMesh &,const std::function<bool()> &cancelled={});
 static std::shared_ptr<const PaintOverlayPacket> capture_native_emission(const PaintedMesh &,const std::function<bool()> &cancelled={});
 // Render-only ordered vertex expansion. Does not grant Scene/paint authority.
 static std::shared_ptr<const PaintOverlayPacket> pack_source_maps(
   const std::shared_ptr<const PaintOverlayPacket> &,std::shared_ptr<const PaintSourceMaps>,
   std::span<const uint32_t> source_vertices,const std::function<bool()> &cancelled={});
};
}
