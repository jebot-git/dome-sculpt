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
 uint32_t resolution_=0;
 std::vector<PaintOverlayLayer> layers_;
 std::shared_ptr<const std::vector<int32_t>> padding_;
 std::shared_ptr<const BakedTextures> baked_;
 std::shared_ptr<const PaintSourceMaps> source_maps_;
 std::shared_ptr<const PaintOverlayPacket> contribution_owner_;
 PaintOverlayPacket()=default;
public:
 uint32_t resolution()const noexcept{return contribution_owner_?contribution_owner_->resolution():resolution_;}
 std::span<const PaintOverlayLayer> layers()const noexcept{return contribution_owner_?contribution_owner_->layers():std::span<const PaintOverlayLayer>(layers_);}
 const std::shared_ptr<const std::vector<int32_t>>&padding()const noexcept{return contribution_owner_?contribution_owner_->padding():padding_;}
 const std::shared_ptr<const BakedTextures>&baked()const noexcept{return contribution_owner_?contribution_owner_->baked():baked_;}
 const std::shared_ptr<const PaintSourceMaps>&source_maps()const noexcept{return source_maps_;}
 // Packed coordinate metadata shares one actual immutable contribution owner.
 const std::shared_ptr<const PaintOverlayPacket>&contribution_owner()const noexcept{return contribution_owner_;}
};
struct PaintOverlayFactory {
 static std::shared_ptr<const PaintOverlayPacket> capture(const PaintedMesh &,const std::function<bool()> &cancelled={});
 // Render-only ordered vertex expansion. Does not grant Scene/paint authority.
 static std::shared_ptr<const PaintOverlayPacket> pack_source_maps(
   const std::shared_ptr<const PaintOverlayPacket> &,std::shared_ptr<const PaintSourceMaps>,
   std::span<const uint32_t> source_vertices,const std::function<bool()> &cancelled={});
};
}
