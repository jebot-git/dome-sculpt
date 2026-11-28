// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake.hpp"
#include <algorithm>

namespace dome::detail {
inline bool has_bake_mask_layer(const PaintedMesh &paint,uint64_t id) {
  return !id||std::ranges::any_of(paint.layers(),[id](const PaintLayer &layer){return layer.id==id;});
}

// Sample the raw linear mask belonging to a specific layer. The layer's
// display visibility, color, coverage and opacity are independent of Bake.
inline std::vector<uint8_t> bake_layer_writability(const PaintedMesh &paint,uint64_t id,
                                                   const auto &check) {
  if(!id)return {};
  const auto &layers=paint.layers();
  const auto layer=std::ranges::find(layers,id,&PaintLayer::id);
  if(layer==layers.end())throw std::invalid_argument("The saved Bake mask layer is unavailable; select another layer or Whole field");
  constexpr uint32_t side=PaintTile::size;
  const uint32_t resolution=paint.resolution(),columns=(resolution+side-1)/side;
  std::vector<uint8_t> writable;
  for(size_t t=0;t<layer->tiles.size();++t){
    check();
    const auto &tile=layer->tiles[t];
    if(!tile)continue;
    const uint32_t x0=uint32_t(t%columns)*side,y0=uint32_t(t/columns)*side;
    for(uint32_t y=0;y<side&&y0+y<resolution;++y){
      for(uint32_t x=0;x<side&&x0+x<resolution;++x){
        if(tile->pixels[y*side+x].data[13]>=128)continue;
        if(writable.empty())writable.resize(size_t(resolution)*resolution,1);
        writable[size_t(y0+y)*resolution+x0+x]=0;
      }
    }
  }
  return writable;
}

// Transfer authorization by ordered triangle corners to an independent
// retained normal grid. The caller charges the additional raster work.
std::vector<uint8_t> map_bake_writability(const Mesh &paint,const Mesh &normal,
    uint32_t paint_resolution,std::span<const uint8_t> writable,
    const BakeOptions &normal_options,std::span<const uint32_t> faces,
    uint64_t &raster_samples);
}
