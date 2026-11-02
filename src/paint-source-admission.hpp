// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
#include "dome/source-maps.hpp"
namespace dome::detail {
inline constexpr const char *editable_texture_atlas_notice=
    "Prepare an editable texture atlas before painting these original source coordinates";
inline bool paint_requires_editable_atlas(const PaintedMesh &paint) noexcept {
  return paint.source_maps() && paint.source_maps()->uses_alternate_uv();
}
inline void require_editable_texture_atlas(const PaintedMesh &paint) {
  if(paint_requires_editable_atlas(paint))throw std::logic_error(editable_texture_atlas_notice);
}
}
