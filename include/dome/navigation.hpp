// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/math.hpp"
#include "navigation.h"
namespace dome {
// OpenBrush Native's one/two-hand rebase and bounded scale behavior.
using CanvasTransform = ob::Transform;
using TrackedHand = ob::TrackedHand;
using CanvasNavigation = ob::Navigation;
inline Vec3 world_to_model(CanvasTransform canvas, Vec3 point) {
  auto p = ob::transform(ob::inverse(canvas), {point.x, point.y, point.z});
  return {p.x, p.y, p.z};
}
} // namespace dome
