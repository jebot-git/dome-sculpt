// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "emission-raster-kernel.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <type_traits>

namespace dome::detail::emission_raster {
// Transfer callbacks finish copying before bounded staging memory is reused.
template<class Upload> void transfer_bake(Bake &bake,const Input &input,Upload upload){
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
}
template<class T> const uint8_t *transfer_layer_tiles(std::span<const T *const> tiles,auto allocate,auto copy,auto stop){
  static_assert(std::is_trivially_copyable_v<T>);
  static_assert(sizeof(PaintTile)==14*tile_pixels&&sizeof(PaintColorTile)==4*tile_pixels);
  if(tiles.empty())return nullptr;
  auto *buffer=allocate.template operator()<uint8_t>(tiles.size()*sizeof(T));
  constexpr size_t batch_tiles=16;
  std::vector<uint8_t> staging(std::min(tiles.size(),batch_tiles)*sizeof(T));
  size_t at=0;
  while(!tiles.empty()){
    stop();
    const auto batch=tiles.first(std::min(tiles.size(),batch_tiles));
    for(size_t n=0;n<batch.size();++n)std::memcpy(staging.data()+n*sizeof(T),batch[n],sizeof(T));
    copy(buffer+at,staging.data(),batch.size()*sizeof(T));
    at+=batch.size()*sizeof(T);
    tiles=tiles.subspan(batch.size());
  }
  return buffer;
}
inline void transfer_common(DeviceInput &view,std::span<const PaintTile *const> tiles,auto allocate,auto copy,auto stop){
  if(tiles.empty())return;
  auto *destination=allocate.template operator()<CommonPixel>(tiles.size()*tile_pixels);
  view.common=destination;
  constexpr size_t tiles_per_batch=32;
  std::vector<CommonPixel> staging(std::min(tiles.size(),tiles_per_batch)*tile_pixels);
  while(!tiles.empty()){
    stop();
    const auto batch=tiles.first(std::min(tiles.size(),tiles_per_batch));
    size_t next=0;
    for(const auto *tile:batch)for(const auto &pixel:tile->pixels){
      const auto &data=pixel.data;
      staging[next++]={{data[6],data[7],data[8]},data[12],data[13]};
    }
    copy(destination,staging.data(),next*sizeof(CommonPixel));
    destination+=next;
    tiles=tiles.subspan(batch.size());
  }
}
inline void transfer_physical(DeviceInput &view,std::span<const PhysicalTile> tiles,auto allocate,auto upload,auto copy,auto stop){
  if(tiles.empty())return;
  const auto &last=tiles.back();
  const size_t words=size_t(last.field.offset)+last.samples.size();
  auto *destination=allocate.template operator()<uint32_t>(words);
  view.physical_values=destination;
  std::vector<PackedField> fields;
  fields.reserve(tiles.size());
  std::vector<float> staging;
  constexpr size_t words_per_batch=65536;
  staging.reserve(std::min(words,words_per_batch));
  auto flush=[&]{
    if(staging.empty())return;
    copy(destination,staging.data(),staging.size()*sizeof(float));
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
  view.physical=upload(std::span<const PackedField>(fields));
}
} // namespace dome::detail::emission_raster
