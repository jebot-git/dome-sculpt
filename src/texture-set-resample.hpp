// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/texture-set-resolution.hpp"
namespace dome::detail {
bool can_resize_native_texture_grid(const SculptObject &) noexcept;
TextureSetLimits native_texture_grid_limits();

// No geometry interpretation: filters every authored texel, including hidden,
// masked and currently uncovered values, on independently preserved planes.
std::vector<PaintLayer> resample_texture_set_layers(
    const PaintedMesh &, const TextureSetResolutionOptions &,
    const TextureSetLimits &, TextureSetResolutionReport &, bool native_cells = false);
} // namespace dome::detail
