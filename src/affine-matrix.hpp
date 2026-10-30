// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/rig.hpp"
namespace dome::detail {
// glTF node.matrix is affine AND TRS-decomposable. World matrices can contain
// shear even when every original local node satisfies this condition.
inline bool gltf_trs_matrix(const RigMatrix &m) {
  for (float f : m)
    if (!std::isfinite(f))
      return false;
  if (m[3] != 0 || m[7] != 0 || m[11] != 0 || m[15] != 1)
    return false;
  const double columns[3][3] = {
      {m[0], m[1], m[2]}, {m[4], m[5], m[6]}, {m[8], m[9], m[10]}};
  for (int a = 0; a < 3; ++a)
    for (int b = a + 1; b < 3; ++b) {
      double aa = 0, bb = 0, ab = 0;
      for (int k = 0; k < 3; ++k) {
        aa += columns[a][k] * columns[a][k];
        bb += columns[b][k] * columns[b][k];
        ab += columns[a][k] * columns[b][k];
      }
      if (std::abs(ab) > std::sqrt(aa) * std::sqrt(bb) * 1e-6)
        return false;
    }
  return true;
}
} // namespace dome::detail
