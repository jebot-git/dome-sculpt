// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/polygon-scene.hpp"
#include "polygon-conversion.hpp"
#include "polygon-paint.hpp"
namespace dome::detail {
// A color receipt cannot own a polygon witness. Explicit native preparation may
// first prove the *same* mesh/UV grid writable and retain the PA state under its
// own direct Scene12 origin/authority. This does not unwrap overlapping UVs.
inline void strict_polygon_color_target(SculptObject &target,
                                        CanvasTransform world,
                                        const std::function<bool()> &cancelled,
                                        std::shared_ptr<const SourcePlacementCapsule> placement={}) {
  if (!target.painted || target.painted->surface_diagnostics().policy !=
                             PaintUvPolicy::PolygonAuthored)
    return;
  if (!target.polygon || !target.polygon->active)
    throw std::invalid_argument("Color preparation requires the target's own active polygon authority");
  PolygonLimits limits;
  limits.cancelled = cancelled;
  validate_polygon_derivative(*target.polygon, target.painted.get(),
                              target.mesh_active, limits);
  PaintSurfaceOptions options;
  options.cancelled = cancelled;
  std::shared_ptr<PaintedMesh> strict;
  try {
    strict = polygon_paint_geometry(*target.painted, target.painted->mesh(), options);
  } catch (const std::invalid_argument &e) {
    throw std::invalid_argument(std::string("Unwrap polygon UVs into a unique writable atlas before preparing source colors: ") + e.what());
  }
  auto origin = capture_polygon_origin(target, world,std::move(placement));
  auto authority = std::make_shared<PolygonAuthority>(*target.polygon);
  authority->origin = std::move(origin);
  if (cancelled && cancelled())
    throw std::runtime_error("Polygon color preparation cancelled");
  target.painted = std::move(strict);
  target.polygon = std::move(authority);
}
} // namespace dome::detail
