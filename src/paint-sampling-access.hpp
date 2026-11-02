// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
#include "dome/paint-sampling.hpp"
namespace dome {
// Detached geometry/grid stage only. The owning colour/material/normal recipe
// must be admitted atomically before Scene24 can expose this editable result.
std::shared_ptr<PaintedMesh> retain_paint_grid(const PaintedMesh &,Mesh,
    std::shared_ptr<const PaintSamplingDomain>,const PaintSamplingLimits & = {});
}
