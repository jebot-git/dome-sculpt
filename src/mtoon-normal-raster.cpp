// SPDX-License-Identifier: GPL-3.0-or-later
#include "mtoon-normal-raster-kernel.hpp"
#include <bit>

namespace dome::detail::mtoon_normal_raster {
namespace {
size_t scratch_bytes(size_t pixels) {
 const auto scratch=std::min(pixels,size_t(batch_texels));
 return sizeof(paint_compute::Quantization)+scratch*(sizeof(Tap)+4)+
     2*((scratch+group_size-1)/group_size)*sizeof(float);
}
}
size_t device_bytes(const Input &in) {
 return (in.original?in.original->rgba.size():0)+in.detail.rgba.size()+
     in.coverage.rgba.size()+in.visible.size_bytes()+scratch_bytes(size_t(in.width)*in.height);
}
size_t native_device_bytes(uint32_t resolution,bool has_source) {
 const auto pixels=size_t(resolution)*resolution;
 return pixels*(has_source?12:8)+scratch_bytes(pixels);
}

void validate(const Input &in,std::span<uint8_t> rgba) {
 if(in.original)in.original->validate();
 in.detail.validate();
 in.coverage.validate();
 const bool native=in.grid==Grid::Native;
 const auto &bake=in.detail;
 const auto *original=in.original;
 const bool aligned=!original||(original->width==in.width&&original->height==in.height);
 const bool valid_grid=native?bake.width==in.width&&bake.height==in.height&&aligned:
     original&&in.width==2*std::max(original->width,bake.width)&&
     in.height==2*std::max(original->height,bake.height)&&
     std::has_single_bit(original->width)&&std::has_single_bit(original->height)&&
     std::has_single_bit(bake.width)&&std::has_single_bit(bake.height);
 const auto margin=native?2u:1u;
 if(!std::has_single_bit(in.width)||!std::has_single_bit(in.height)||in.width>4096||in.height>4096||
    !valid_grid||in.coverage.width!=bake.width||in.coverage.height!=bake.height||
    !std::isfinite(in.scale)||!finite(in.constant)||rgba.size()!=4*size_t(in.width)*in.height||
    (!in.visible.empty()&&in.visible.size()!=size_t(in.width+margin)*(in.height+margin)))
  throw std::invalid_argument("Invalid MToon normal raster grids");
}
}
