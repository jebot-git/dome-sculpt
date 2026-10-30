// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/affine.hpp"
#include <functional>
namespace dome::detail {
struct AffineGltfFactors {
  // left = translation*proper_rotation*signed_scale, right = proper_rotation.
  // Neither is a sheared glTF node. Unedited original locals bypass this path.
  std::array<AffineTransform,2> nodes;
  unsigned sweeps=0;
  double column_relative_error=0,float_column_relative_error=0,
         orthogonality_error=0;
};
// Export/admission preparation only, never a per-frame or per-vertex operation.
// Fixed3x3 DGESVJ has30 bounded sweeps and fixed stack workspace. Nonconvergence,
// singular/unrepresentable results and cancellation never return partial factors.
AffineGltfFactors factor_edit_for_gltf(const AffineTransform &,
                                     const std::function<bool()> &cancelled={});
} // namespace dome::detail
