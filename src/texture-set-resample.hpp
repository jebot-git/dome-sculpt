// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/texture-set-resolution.hpp"
namespace dome::detail {
// No geometry interpretation: filters every authored texel, including hidden,
// masked and currently uncovered values, on independently preserved planes.
std::vector<PaintLayer> resample_texture_set_layers(
    const PaintedMesh &, const TextureSetResolutionOptions &,
    const TextureSetLimits &, TextureSetResolutionReport &);
} // namespace dome::detail
