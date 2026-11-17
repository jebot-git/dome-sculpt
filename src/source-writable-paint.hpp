// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint-overlay-packet.hpp"
#include "native-paint-sampling.hpp"
#include "source-paint-pairing.hpp"
namespace dome::detail {
// Material authorization and geometry correspondence are independent. The
// immutable packet's factory captured this exact binding after authenticating
// its attachment; an arbitrary structural PaintSamplingDomain is insufficient.
inline std::array<float,2> source_overlay_coordinates(const PaintOverlayPacket &packet,
    uint32_t triangle,std::array<float,3> bary,std::array<float,2> current) {
 const auto &binding=packet.normal_source_binding();
 const auto attachment=material_paint_attachment(binding.get());
 if(!attachment)return current;
 const auto &domain=*attachment->domain();const auto &basis=*domain.basis();
 if(size_t(triangle)>=domain.source_corners().size()/3)
   throw std::out_of_range("Writable source triangle lacks exact original paint corners");
 std::array<double,2> result{};
 for(size_t k=0;k<3;++k){
  const auto corner=domain.source_corners()[size_t(triangle)*3+k];
  const auto q=basis.uv.at(basis.indices.at(corner));
  for(size_t axis=0;axis<2;++axis)result[axis]+=double(q[axis])*bary[k];
 }
 for(size_t axis=0;axis<2;++axis){
  if(!std::isfinite(result[axis]))throw std::invalid_argument("Nonfinite writable source paint coordinate");
  current[axis]=float(result[axis]);
 }
 return current;
}
}
