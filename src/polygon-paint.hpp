// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
namespace dome {
// Keep the same physical UV tile domain, every layer/channel/ID and immutable
// source/bake payload. Caller owns explicit conversion of any external source
// accessor/frame authority before adopting the new geometric derivative.
std::shared_ptr<PaintedMesh>
polygon_paint_geometry(const PaintedMesh &, Mesh,
                       const PaintSurfaceOptions & = {});
} // namespace dome
