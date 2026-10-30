// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/humanoid-creator.hpp"
#include "dome/polygon-color.hpp"
#include "dome/polygon-source.hpp"
namespace dome {
struct PolygonNativeColorCorner {
  PolygonId corner = 0;
  uint32_t native_vertex = 0, raw_accessor = 0;
};
struct PolygonSourceColors {
  PolygonSourceTopology geometry;
  PolygonColorField field;
  // Detached immutable authoring inputs, not a fabricated replacement asset.
  std::shared_ptr<const PaintedMesh> original_paint;
  std::shared_ptr<const CreatorAppearance> original_appearance;
  std::vector<PolygonNativeColorCorner> native_corners;
};
// Only succeeds when actual raw COLOR_0 and exact source-corner/triangle
// witnesses prove that native N-gon triangulation preserves the current field.
// Arbitrary piecewise fields requiring a different diagonal remain refused.
PolygonSourceColors
prepare_polygon_source_colors(const PaintedMesh &,
                              std::shared_ptr<const CreatorAppearance>,
                              const PolygonLimits & = {});
} // namespace dome
