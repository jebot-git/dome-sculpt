// SPDX-License-Identifier: GPL-3.0-or-later
#include "mixed-normal-raster-kernel.hpp"
#include <bit>

namespace dome::detail::mixed_normal {
size_t device_bytes(const Input &in) {
 const size_t scratch=capacity(in);
 return in.original.rgba.size()+in.detail.rgba.size()+in.coverage.rgba.size()+in.visible.size_bytes()+
     sizeof(paint_compute::Quantization)+scratch*(sizeof(Tap)+4)+
     2*((scratch+group_size-1)/group_size)*sizeof(float);
}

void validate(const Input &in,std::span<uint8_t> rgba) {
 in.original.validate();
 in.detail.validate();
 in.coverage.validate();
 if(!std::has_single_bit(in.width)||!std::has_single_bit(in.height)||in.width>4096||in.height>4096||
    in.width!=2*std::max(in.original.width,in.detail.width)||
    in.height!=2*std::max(in.original.height,in.detail.height)||
    !std::has_single_bit(in.original.width)||!std::has_single_bit(in.original.height)||
    !std::has_single_bit(in.detail.width)||!std::has_single_bit(in.detail.height)||
    in.coverage.width!=in.detail.width||in.coverage.height!=in.detail.height||
    !std::isfinite(in.scale)||rgba.size()!=4*size_t(in.width)*in.height||
    (!in.visible.empty()&&in.visible.size()!=size_t(in.width+1)*(in.height+1)))
  throw std::invalid_argument("Invalid mixed normal raster grids");
}
}
