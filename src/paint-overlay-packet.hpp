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
 PaintOverlayPacket()=default;
public:
 uint32_t resolution()const noexcept{return resolution_;}
 bool empty()const noexcept{return empty_;}
 size_t storage_bytes()const noexcept{return storage_bytes_;}
 std::span<const PaintOverlayLayer> layers()const noexcept{return layers_;}
 const std::shared_ptr<const std::vector<int32_t>>&padding()const noexcept{return padding_;}
 const std::shared_ptr<const BakedTextures>&baked()const noexcept{return baked_;}
 const std::shared_ptr<const PaintSourceMaps>&source_maps()const noexcept{return source_maps_;}
};
struct PaintOverlayFactory {
 static std::shared_ptr<const PaintOverlayPacket> capture(const PaintedMesh &,const std::function<bool()> &cancelled={});
};
}
