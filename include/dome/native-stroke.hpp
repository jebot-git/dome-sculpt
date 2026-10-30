// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "brush.hpp"
namespace dome {
// Explicit sample coordinates. Ray uses the existing scene-space camera ray;
// EvaluatedWorldBrush contains a complete capsule in captured scene-world space.
enum class NativeStrokeSpace { Ray, EvaluatedWorldBrush };
// Camera rays and brush metadata only. Picking, normals and mutation belong to
// the native gesture worker, never the FLTK event callback.
struct NativeStrokeSample {
  Vec3 origin{}, direction{0,0,-1};
  Brush brush;
  double material_time = 0;
  NativeStrokeSpace space = NativeStrokeSpace::Ray;
};
}
