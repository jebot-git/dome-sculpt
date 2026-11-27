// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/texture-set-resolution.hpp"
namespace dome::detail {
struct PaintBilinearTap {
  size_t texel = 0;
  double weight = 0;
};
struct FilteredPaintTexel {
  PaintPixel common;
  PaintEmissionValue emission;
};
std::array<PaintBilinearTap,4> paint_bilinear_taps(uint32_t resolution,
    std::array<float,2> uv,std::span<const int32_t> padding);
FilteredPaintTexel filter_paint_layer(const PaintLayer &,uint32_t resolution,
    const std::array<PaintBilinearTap,4> &);

bool can_resize_native_texture_grid(const SculptObject &) noexcept;
TextureSetLimits native_texture_grid_limits();

// No geometry interpretation: filters every authored texel, including hidden,
// masked and currently uncovered values, on independently preserved planes.
std::vector<PaintLayer> resample_texture_set_layers(
    const PaintedMesh &, const TextureSetResolutionOptions &,
    const TextureSetLimits &, TextureSetResolutionReport &, bool native_cells = false);
} // namespace dome::detail
