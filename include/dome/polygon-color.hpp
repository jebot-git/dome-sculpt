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
// Immutable geometry-side color authority, retained by native Scene archives.
class PolygonColorField {
public:
  static PolygonColorField from_corners(const PolygonMesh &,
                                        std::span<const PolygonCornerRgba>,
                                        const PolygonLimits & = {});
  void validate(const PolygonMesh &, const PolygonLimits & = {}) const;
  PolygonRgba sample(PolygonId, const PolygonLimits & = {}) const;
  std::vector<PolygonRgba> packed(const PolygonMesh &,
                                  const PolygonLimits & = {}) const;
  const std::vector<PolygonCornerRgba> &sources() const {
    return sources_ ? *sources_ : empty_sources_;
  }
  const std::vector<PolygonColorSample> &corners() const { return corners_; }
  std::shared_ptr<const std::vector<PolygonCornerRgba>> source_owner() const {
    return sources_;
  }

private:
  inline static const std::vector<PolygonCornerRgba> empty_sources_{};
  std::shared_ptr<const std::vector<PolygonCornerRgba>> sources_;
  std::vector<PolygonColorSample> corners_;
  friend struct PolygonColorArchiveAccess;
  friend PolygonColorField transfer_polygon_colors(
      const PolygonMesh &, const PolygonColorField &, const PolygonMesh &,
      std::span<const PolygonCornerSource>, const PolygonLimits &);
  friend PolygonColoredEdit edit_colored_polygon(const PolygonMesh &,
                                                 const PolygonColorField &,
                                                 const PolygonEdit &,
                                                 const PolygonLimits &);
};
// Transient geometry-owned current color data. Both immutable inputs must match
// the exact ordered REST derivative before a renderer/exporter can consume it.
struct PolygonColorSurface {
  std::shared_ptr<const PolygonMesh> topology;
  std::shared_ptr<const PolygonColorField> field;
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
// Compose authenticated geometric corner transfers through the existing field.
// The original immutable source basis is shared, never replaced by sampled
// raster/vertex values. Every target corner requires one convex input witness.
PolygonColorField transfer_polygon_colors(const PolygonMesh &,
                                          const PolygonColorField &,
                                          const PolygonMesh &,
                                          std::span<const PolygonCornerSource>,
                                          const PolygonLimits & = {});
} // namespace dome
