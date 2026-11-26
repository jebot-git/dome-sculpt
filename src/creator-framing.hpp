// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
namespace dome::detail {
// Fits the actual accepted model bounds to both physical eye frusta. Only the
// session canvas translation may change; source placement and eye poses do not.
std::optional<CanvasTransform> initial_creator_fit(Vec3 minimum,Vec3 maximum,
    CanvasTransform canvas,CanvasTransform head,const std::array<SpatialView,2>&);
}
