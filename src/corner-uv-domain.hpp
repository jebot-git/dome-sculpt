// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
#include "dome/source-maps.hpp"
namespace dome::detail {
// Plain SourceUV's paired sampler is not, by itself, a qualification of the
// current-corner prepared colour plane in a second source-coordinate domain.
// Keep this subcase explicit until its material/field adapter is tested.
inline void require_corner_uv_domain(const PaintedMesh &paint) {
 if(const auto &maps=paint.source_maps();maps&&maps->uses_uv1())
  throw std::invalid_argument("Current corner colour UV1 transfer requires its explicit current-field domain adapter; original state retained");
}
}
