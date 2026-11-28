// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "emission-raster-kernel.hpp"
#include "paint-composite-backend.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dome::detail::emission_raster {
template<class Device>
class Raster final : public EmissionRasterDevice {
public:
  Raster(const Input &input,const std::function<bool()> &cancelled){
    const auto stop=[&]{paint_compute::checkpoint(cancelled);};
    view_.resolution=input.resolution;
    view_.layer_count=uint32_t(input.layers.size());
    view_.tiles_per_layer=(input.resolution/32)*(input.resolution/32);
    for(unsigned k=0;k<3;++k)view_.background[k]=input.background[k];
    view_.strength=input.strength;
    view_.padding=upload(std::span(input.padding));
    view_.layers=upload(std::span(input.layers));
    view_.entries=upload(std::span(input.entries));
    view_.quantization=upload(std::span(&paint_compute::quantization(),1));
    output_=allocate<uint32_t>(chunk_pixels*10);
    stop();
    upload_common(input.common,stop);
    upload_physical(input.physical,stop);

    auto &bake=view_.bake;
    bake.physical=input.physical_bake;
    if(bake.physical){
      for(unsigned k=0;k<7;++k)bake.packed.constants[k]=std::bit_cast<uint32_t>(input.bake.constants[k]);
      bake.packed.mask=input.bake.varying_mask;
      bake.packed.stride=std::popcount(input.bake.varying_mask);
      bake.dense=reinterpret_cast<const uint32_t *>(upload(std::as_bytes(input.bake.dense)));
      bake.varying=reinterpret_cast<const uint32_t *>(upload(input.bake.varying));
      bake.coverage_bits=upload(input.bake.binary_coverage);
    }else{
      bake.legacy=upload(input.legacy_bake);
      bake.coverage=upload(input.bake_coverage);
    }
    stop();
    if(!input.source.empty()){
      view_.source=input.source_sampler;
      view_.source.rgba=upload(input.source);
      view_.source.coordinates.columns=upload(std::span(input.columns));
      view_.source.coordinates.rows=upload(std::span(input.rows));
    }
    stop();
  }

  void read(size_t first,std::span<Pixel> rows,const std::function<bool()> &cancelled) override {
    static_assert(sizeof(Pixel)==10*sizeof(uint32_t));
    const auto size=size_t(view_.resolution)*view_.resolution;
    if(first>size||rows.size()>size-first)throw std::out_of_range("GPU emission read exceeds its owning grid");
    while(!rows.empty()){
      paint_compute::checkpoint(cancelled);
      auto batch=rows.first(std::min(rows.size(),chunk_pixels));
      device_.render(view_,first,batch.size(),output_);
      device_.download(batch.data(),output_,batch.size_bytes());
      for(const auto &pixel:batch){
        if(std::bit_cast<uint32_t>(pixel[0])==invalid_uv)throw std::runtime_error("MToon UV overflow");
        for(unsigned k=0;k<3;++k)if(!std::isfinite(pixel[k]))
          throw std::invalid_argument("GPU emission produced an invalid native FLOAT coefficient or radiance");
      }
      rows=rows.subspan(batch.size());
      first+=batch.size();
    }
    paint_compute::checkpoint(cancelled);
  }

private:
  template<class T>T *allocate(size_t count){
    return count?static_cast<T *>(device_.allocate(count*sizeof(T))):nullptr;
  }
  template<class T,size_t Extent> const T *upload(std::span<const T,Extent> data){
    auto *buffer=allocate<T>(data.size());
    if(buffer)device_.upload(buffer,data.data(),data.size_bytes());
    return buffer;
  }
  template<class Stop> void upload_common(std::span<const PaintTile *const> tiles,Stop stop){
    if(tiles.empty())return;
    auto *destination=allocate<CommonPixel>(tiles.size()*tile_pixels);
    view_.common=destination;
    constexpr size_t tiles_per_batch=32;
    std::vector<CommonPixel> staging(tiles_per_batch*tile_pixels);
    while(!tiles.empty()){
      stop();
      const auto batch=tiles.first(std::min(tiles.size(),tiles_per_batch));
      size_t next=0;
      for(const auto *tile:batch)for(const auto &pixel:tile->pixels){
        const auto &data=pixel.data;
        staging[next++]={{data[6],data[7],data[8]},data[12],data[13]};
      }
      device_.upload(destination,staging.data(),next*sizeof(CommonPixel));
      destination+=next;
      tiles=tiles.subspan(batch.size());
    }
  }
  template<class Stop> void upload_physical(std::span<const PhysicalTile> tiles,Stop stop){
    if(tiles.empty())return;
    const auto &last=tiles.back();
    const size_t words=size_t(last.field.offset)+last.samples.size();
    auto *destination=allocate<uint32_t>(words);
    view_.physical_values=destination;
    std::vector<PackedField> fields;
    fields.reserve(tiles.size());
    std::vector<float> staging;
    constexpr size_t words_per_batch=65536;
    staging.reserve(std::min(words,words_per_batch));
    auto flush=[&]{
      if(staging.empty())return;
      device_.upload(destination,staging.data(),staging.size()*sizeof(float));
      destination+=staging.size();
      staging.clear();
    };
    for(size_t i=0;i<tiles.size();++i){
      if((i&255)==0)stop();
      const auto &tile=tiles[i];
      fields.push_back(tile.field);
      if(staging.size()+tile.samples.size()>words_per_batch)flush();
      staging.insert(staging.end(),tile.samples.begin(),tile.samples.end());
    }
    flush();
    view_.physical=upload(std::span<const PackedField>(fields));
  }

  Device device_;
  DeviceInput view_;
  uint32_t *output_=nullptr;
};
}
