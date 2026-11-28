// SPDX-License-Identifier: GPL-3.0-or-later

#include "emission-raster-device.hpp"
#include "emission-paint-access.hpp"
#include "dome/mtoon.hpp"
#include "paint-composite-backend.hpp"
#include "dome/source-maps.hpp"
#include "source-texture-grid.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace dome::detail {
namespace {
SourceTextureProducts typed_products(const PaintSourceTexture &source,const MToonTexture &slot,uint32_t r,
                                     bool &coupled){
  source.validate();
  const auto bits=[](double value){return std::bit_cast<uint64_t>(value);};
  const auto center=[r](uint32_t at){return double((float(at)+.5f)/r);};
  const auto transformed=[&](uint32_t at,unsigned axis){return center(at)*slot.scale[axis]+slot.offset[axis];};
  const auto representable=[&](unsigned axis){
    constexpr double limit=std::numeric_limits<float>::max();
    return std::abs(transformed(0,axis))<=limit&&std::abs(transformed(r-1,axis))<=limit;
  };
  // Each axis is monotone before FLOAT rounding. If its endpoints are valid,
  // store the already-rounded axis in the retained affine sampler's terms.
  // Overflowing axes stay with the per-texel device validation.
  coupled=slot.rotation!=0||!representable(0)||!representable(1);
  SourceTextureProducts result;
  result.columns.reserve(r);
  result.rows.reserve(r);
  if(!coupled){
    const auto &m=source.uv_transform;
    result.transform={bits(m[4]),bits(m[5]),source.wrap_s,source.wrap_t};
    for(uint32_t i=0;i<r;++i){
      const double x=float(transformed(i,0)),y=float(transformed(i,1));
      result.columns.push_back({bits(m[0]*x),bits(m[2]*x)});
      result.rows.push_back({bits(m[1]*y),bits(m[3]*y)});
    }
    return result;
  }
  const double cosine=std::cos(slot.rotation),sine=std::sin(slot.rotation);
  result.transform={bits(slot.offset[0]),bits(slot.offset[1]),source.wrap_s,source.wrap_t};
  for(uint32_t i=0;i<r;++i){
    const double x=center(i)*slot.scale[0],y=center(i)*slot.scale[1];
    result.columns.push_back({bits(cosine*x),bits(sine*x)});
    result.rows.push_back({bits(-(sine*y)),bits(cosine*y)});
  }
  return result;
}
}
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

namespace {
std::unique_ptr<EmissionRasterDevice> create_raster(
    const PaintedMesh &paint,std::span<const int32_t> padding,const PaintSourceTexture *source,
    const MToonTexture *typed,std::array<float,3> background,float strength,
    uint32_t finite_resolution,float offset,const std::function<bool()> &cancelled){
  using namespace emission_raster;
  const auto stop=[&]{paint_compute::checkpoint(cancelled);};
  stop();
  if(!std::isfinite(strength)||strength<0||std::any_of(background.begin(),background.end(),
      [](float color){return !std::isfinite(color)||color<0;}))
    throw std::invalid_argument("Invalid native HDR background or legacy strength");
  Input input;
  input.padding=padding;
  input.resolution=paint.resolution();
  if(finite_resolution){
    if(finite_resolution>8192||!std::isfinite(offset)||(source&&offset!=0))
      throw std::invalid_argument("Invalid GPU finite emission sampling grid");
    input.finite_axis.reserve(finite_resolution);
    const int last=int(input.resolution)-1;
    for(uint32_t i=0;i<finite_resolution;++i){
      const float uv=(float(i)+.5f)/finite_resolution-offset;
      const float at=std::clamp(uv,0.f,1.f)*input.resolution-.5f;
      const int first=int(std::floor(at));
      input.finite_axis.push_back({std::clamp(first,0,last),std::clamp(first+1,0,last),at-first});
    }
  }
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
    const auto extent=finite_resolution?finite_resolution:input.resolution;
    auto products=typed?typed_products(*source,*typed,input.resolution,input.source_sampler.typed_transform):
                        source_texture_products(*source,extent,extent);
    input.source=source->image->rgba;
    input.source_sampler.coordinates={nullptr,nullptr,nullptr,products.transform,
        source->image->width,source->image->height,source->mag_filter==9728};
    if(input.source_sampler.typed_transform)for(unsigned k=0;k<source->uv_transform.size();++k)
      input.source_sampler.sampler_affine[k]=std::bit_cast<uint32_t>(source->uv_transform[k]);
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
    for(unsigned mask=0;mask<scales.size();++mask){
      const float scale=finite_resolution?(float(mask)/255.f)*layer.opacity:(layer.opacity*float(mask))/255.f;
      scales[mask]=std::bit_cast<uint32_t>(scale);
    }
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
      if(physical&&(!finite_resolution||tile)){
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

std::unique_ptr<EmissionRasterDevice> create_emission_raster_device(
    const PaintedMesh &paint,std::span<const int32_t> padding,const PaintSourceTexture *source,
    const MToonTexture *typed,std::array<float,3> background,float strength,const std::function<bool()> &cancelled){
  return create_raster(paint,padding,source,typed,background,strength,0,0,cancelled);
}

std::unique_ptr<EmissionRasterDevice> create_finite_emission_raster_device(
    const PaintedMesh &paint,const PaintSourceTexture *source,std::array<float,3> background,
    uint32_t resolution,float offset,const std::function<bool()> &cancelled){
  if(!resolution)throw std::invalid_argument("Missing GPU finite emission resolution");
  return create_raster(paint,EmissionExportAccess::writable_padding(paint),source,nullptr,
                       background,1,resolution,offset,cancelled);
}
}
