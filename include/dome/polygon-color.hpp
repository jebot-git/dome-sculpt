// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon.hpp"
namespace dome {
using PolygonRgba = std::array<float, 4>;
struct PolygonCornerRgba {
  PolygonId corner = 0;
  PolygonRgba value{};
};
struct PolygonColorSample {
  PolygonId corner = 0;
  // Convex interpolation of immutable initial CORNER samples, not vertices.
  std::vector<std::pair<PolygonId, double>> provenance;
};
struct PolygonColoredEdit;
// Detached, immutable geometry-side color authority. This foundation has no
// Scene codec yet; it cannot on its own remove a conversion/export guard.
class PolygonColorField {
public:
  static PolygonColorField from_corners(const PolygonMesh &,
                                        std::span<const PolygonCornerRgba>,
                                        const PolygonLimits & = {});
  void validate(const PolygonMesh &, const PolygonLimits & = {}) const;
  PolygonRgba sample(PolygonId, const PolygonLimits & = {}) const;
  std::vector<PolygonRgba> packed(const PolygonMesh &,
                                  const PolygonLimits & = {}) const;
  const std::vector<PolygonCornerRgba> &sources() const { return sources_; }
  const std::vector<PolygonColorSample> &corners() const { return corners_; }

private:
  std::vector<PolygonCornerRgba> sources_;
  std::vector<PolygonColorSample> corners_;
  friend PolygonColoredEdit edit_colored_polygon(const PolygonMesh &,
                                                 const PolygonColorField &,
                                                 const PolygonEdit &,
                                                 const PolygonLimits &);
};
struct PolygonColoredEdit {
  PolygonEditResult geometry;
  std::optional<PolygonColorField> colors;
};
// Straight-alpha linear RGBA interpolation. Source geometry, color samples and
// ID counters are unchanged on no-op, invalid mapping, cancellation or failure.
PolygonColoredEdit edit_colored_polygon(const PolygonMesh &,
                                        const PolygonColorField &,
                                        const PolygonEdit &,
                                        const PolygonLimits & = {});
} // namespace dome
