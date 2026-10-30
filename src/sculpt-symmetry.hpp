// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/brush.hpp"
#include <vector>
namespace dome::sculpt_detail {
// Same exact-plane dedup and object-local reflection as the shared GPU kernel.
// Used only to form bounded sample support, never to evaluate brush distances.
inline std::vector<Brush> images(Brush b) {
  unsigned axes = b.symmetry_axes & 7;
  if (b.from.x == b.symmetry_origin.x && b.to.x == b.symmetry_origin.x &&
      (b.operation != BrushOperation::Sculpt || b.tool != Tool::Flatten || b.normal.x == 0)) axes &= ~1u;
  if (b.from.y == b.symmetry_origin.y && b.to.y == b.symmetry_origin.y &&
      (b.operation != BrushOperation::Sculpt || b.tool != Tool::Flatten || b.normal.y == 0)) axes &= ~2u;
  if (b.from.z == b.symmetry_origin.z && b.to.z == b.symmetry_origin.z &&
      (b.operation != BrushOperation::Sculpt || b.tool != Tool::Flatten || b.normal.z == 0)) axes &= ~4u;
  std::vector<Brush> out;
  for (unsigned bits = 0; bits < 8; ++bits) {
    if (bits & ~axes) continue;
    auto copy = b;
    auto point = [&](Vec3 &p) {
      if (bits & 1) p.x = 2 * b.symmetry_origin.x - p.x;
      if (bits & 2) p.y = 2 * b.symmetry_origin.y - p.y;
      if (bits & 4) p.z = 2 * b.symmetry_origin.z - p.z;
    };
    point(copy.from); point(copy.to);
    if (bits & 1) copy.normal.x = -copy.normal.x;
    if (bits & 2) copy.normal.y = -copy.normal.y;
    if (bits & 4) copy.normal.z = -copy.normal.z;
    copy.symmetry_axes = 0;
    out.push_back(copy);
  }
  return out;
}
} // namespace dome::sculpt_detail
