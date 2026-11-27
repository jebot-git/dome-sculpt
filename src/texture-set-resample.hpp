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
  std::array<uint8_t,4> pbr{};
  bool pbr_present = false;
};
std::array<PaintBilinearTap,4> paint_bilinear_taps(uint32_t resolution,
    std::array<float,2> uv,std::span<const int32_t> padding,bool require_padding_owner=false);
FilteredPaintTexel filter_paint_layer(const PaintLayer &,uint32_t resolution,
    const std::array<PaintBilinearTap,4> &);

bool can_resize_native_texture_grid(const SculptObject &) noexcept;
bool can_resize_retained_texture_grid(const TextureSetView &) noexcept;
TextureSetLimits native_texture_grid_limits();
inline bool resizable_canonical_bake(const BakedTextures *bake) noexcept {
  return !bake || (!bake->normal_authority && !bake->normal_field &&
                    bake->normal_interpretation == BakedNormalInterpretation::CanonicalMikk);
}
inline bool resizable_native_uv_bake(const BakedTextures *bake) noexcept {
  return resizable_canonical_bake(bake) ||
      (bake && !bake->normal_authority && !bake->normal_field &&
       bake->normal_interpretation == BakedNormalInterpretation::NativeAttributes);
}

// No geometry interpretation: filters every authored texel, including hidden,
// masked and currently uncovered values, on independently preserved planes.
std::vector<PaintLayer> resample_texture_set_layers(
    const PaintedMesh &, const TextureSetResolutionOptions &,
    const TextureSetLimits &, TextureSetResolutionReport &, bool native_cells = false);
std::shared_ptr<const BakedTextures> resample_native_bake_grid(
    const BakedTextures &,uint32_t original,const TextureSetResolutionOptions &,
    const TextureSetLimits &,TextureSetResolutionReport &,bool retained_native_uv,
    bool independent_normal_grid);
} // namespace dome::detail
