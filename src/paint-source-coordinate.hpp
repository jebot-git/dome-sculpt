// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "source-texture-axis.hpp"
#include <cstdint>

namespace dome::paint_compute {
struct SourceTerms { uint64_t u,v; };
struct SourceTransform {
  uint64_t offset_u=0,offset_v=0;
  int wrap_s=10497,wrap_t=10497;
};
struct SourcePixelTaps { detail::SourceTextureAxis column,row; };
struct SourceCoordinateJob {
  const SourceTerms *columns=nullptr,*rows=nullptr;
  SourcePixelTaps *output=nullptr;
  SourceTransform transform;
  uint32_t width=0,height=0;
  bool nearest=false;
};
inline constexpr unsigned source_coordinate_roles=4;
struct SourceCoordinateBatch {
  SourceCoordinateJob jobs[source_coordinate_roles];
  unsigned size=0;
};
}
