// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/source-paint.hpp"
namespace dome::detail {
// Only UV affine parameters change, never images/samplers/coordinates or paint.
// The factory authenticates the existing owner and replays genuine Source4
// transport. Current paint remains in its independent writable basis.
std::shared_ptr<PaintedMesh> rebind_source_paint_transforms(const PaintedMesh &,
 const std::array<std::array<float,6>,5> &,const SourcePaintOptions & = {});
}
