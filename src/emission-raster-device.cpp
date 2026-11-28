// SPDX-License-Identifier: GPL-3.0-or-later

#include "emission-raster-device.hpp"
#include "emission-paint-access.hpp"
#include "paint-composite-backend.hpp"
#include "source-texture-grid.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <unordered_map>

namespace dome::detail {
emission_raster::PhysicalTile EmissionExportAccess::emission_tile(const PaintEmissionTile &tile){
  emission_raster::PhysicalTile view;
  for(unsigned k=0;k<7;++k)view.field.constants[k]=std::bit_cast<uint32_t>(tile.constants_[k]);
  view.field.mask=tile.varying_mask_;
  view.field.stride=tile.varying_count_;
  if(tile.varying_count_)view.samples={tile.values_.get(),tile.sample_count*tile.varying_count_};
  return view;
}

size_t emission_raster_staging_bytes(const PaintedMesh &paint) noexcept {
  // The table and sparse references grow with authored tiles. Full FLOAT
  // payloads stay in their existing owners until the bounded upload.
  size_t bytes=768u*1024;
  for(const auto &layer:paint.layers()){
    bytes+=sizeof(layer)+(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size())*
           sizeof(std::shared_ptr<const PaintTile>);
    if(layer.visible&&layer.opacity>0){
      bytes+=layer.tiles.size()*sizeof(emission_raster::Entry)*2+sizeof(emission_raster::Layer)*2;
      for(const auto &tile:layer.tiles)if(tile)bytes+=256;
    }
  }
  return bytes;
}

std::unique_ptr<EmissionRasterDevice> create_emission_raster_device(
    const PaintedMesh &paint,std::span<const int32_t> padding,const PaintSourceTexture *source,
    std::array<float,3> background,float strength,const std::function<bool()> &cancelled){
  using namespace emission_raster;
  const auto stop=[&]{paint_compute::checkpoint(cancelled);};
  stop();
  if(!std::isfinite(strength)||strength<0||std::any_of(background.begin(),background.end(),
      [](float color){return !std::isfinite(color)||color<0;}))
    throw std::invalid_argument("Invalid native HDR background or legacy strength");
  Input input;
  input.padding=padding;
  input.resolution=paint.resolution();
  for(unsigned k=0;k<3;++k)input.background[k]=std::bit_cast<uint32_t>(background[k]);
  input.strength=std::bit_cast<uint32_t>(strength);
  if(auto bake=paint.baked()){
    if(bake->emission_field){
      input.physical_bake=true;
      input.bake=bake->emission_field->storage();
    }else{
      input.legacy_bake=bake->maps.emission.rgba;
      input.bake_coverage=bake->coverage;
    }
  }
  if(source&&source->image){
    auto products=source_texture_products(*source,input.resolution,input.resolution);
    input.source=source->image->rgba;
    input.source_sampler={nullptr,nullptr,nullptr,products.transform,source->image->width,source->image->height,
                          source->mag_filter==9728};
    input.columns=std::move(products.columns);
    input.rows=std::move(products.rows);
  }

  std::unordered_map<const PaintTile *,uint32_t> common_ids;
  std::unordered_map<const PaintEmissionTile *,uint32_t> physical_ids;
  uint32_t coefficient_offset=0;
  for(const auto &layer:paint.layers()){
    if(!layer.visible||layer.opacity<=0)continue;
    stop();
    const auto first_entry=input.entries.size();
    bool contributes=false;
    auto &scales=input.layers.emplace_back().opacity;
    for(unsigned mask=0;mask<scales.size();++mask)
      scales[mask]=std::bit_cast<uint32_t>((layer.opacity*float(mask))/255.f);
    for(size_t i=0;i<layer.tiles.size();++i){
      if((i&255)==0)stop();
      auto &entry=input.entries.emplace_back();
      const auto *tile=layer.tiles[i].get();
      const auto *physical=layer.emission_tiles.empty()?nullptr:layer.emission_tiles[i].get();
      const bool use_mask=physical||(tile&&std::any_of(tile->pixels.begin(),tile->pixels.end(),
          [](const auto &pixel){return pixel.data[12]!=0;}));
      if(use_mask&&tile&&!PaintTile::is_shared_default(tile)){
        contributes=true;
        const auto [found,inserted]=common_ids.try_emplace(tile,input.common.size());
        entry.common=found->second;
        if(inserted)input.common.push_back(tile);
      }
      if(physical){
        contributes=true;
        const auto [found,inserted]=physical_ids.try_emplace(physical,input.physical.size());
        entry.physical=found->second;
        if(inserted){
          auto &view=input.physical.emplace_back(EmissionExportAccess::emission_tile(*physical));
          if(view.samples.size()>UINT32_MAX-coefficient_offset)
            throw std::length_error("GPU emission tile offsets exceed the native raster range");
          view.field.offset=coefficient_offset;
          coefficient_offset+=uint32_t(view.samples.size());
        }
      }
    }
    if(!contributes){
      input.layers.pop_back();
      input.entries.resize(first_entry);
    }
  }
  stop();
  return upload(input,cancelled);
}
}
