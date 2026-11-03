// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
#include "dome/source-maps.hpp"
#include <algorithm>
namespace dome::detail {
inline constexpr const char *editable_texture_atlas_notice=
    "Prepare an editable texture atlas before painting these original source coordinates";
inline bool paint_requires_editable_atlas(const PaintedMesh &paint) noexcept {
  if(!paint.source_maps())return false;
  if(paint.source_maps()->uses_alternate_uv()&&!paint.source_sampling_prepared())return true;
  // A retained sampling domain has its own independently validated raster.
  // Otherwise raw UVs outside the first tile require explicit atlas creation.
  return !paint.paint_sampling_domain()&&std::any_of(paint.mesh().uv.begin(),paint.mesh().uv.end(),[](const auto &uv){
    return uv[0]<0||uv[0]>1||uv[1]<0||uv[1]>1;
  });
}
inline void require_editable_texture_atlas(const PaintedMesh &paint) {
  if(paint_requires_editable_atlas(paint))throw std::logic_error(editable_texture_atlas_notice);
}
}
