// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "dome/normal-preparation.hpp"
#include <span>

namespace dome {
struct SculptObject;
namespace detail {
struct NativeBakeConversion {
  std::shared_ptr<PaintedMesh> paint;
  double maximum_sample_error = 0;
};

bool native_bake_conversion_supported(const SculptObject &) noexcept;
NativeBakeConversion convert_native_baked_normals(const SculptObject &,
    const NormalPreparationOptions &);

struct NativeBakeTexel {
  size_t face;
  std::array<double, 3> bary;
};
struct ConvertedNativeBakeGrid {
  Image normal;
  double maximum_sample_error = 0;
};
ConvertedNativeBakeGrid convert_native_bake_grid(const Mesh &,const NormalFrames &,
    const BakedTextures &,std::span<const int32_t> padding,
    const std::function<NativeBakeTexel(size_t)> &,const NormalPreparationOptions &);

// Native baked normals have no separate intrinsic texture. Use the same
// coverage blend and quantization for the checked candidate and compositor.
std::array<uint8_t, 4> composite_native_bake_normal(Vec3 detail,float coverage);
}
}
