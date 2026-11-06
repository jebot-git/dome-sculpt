// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-sets.hpp"
namespace dome {
// Measured authored-grid transfer; original normal/source grids require a
// separate authenticated transition and are never relabelled by this API.
struct TextureSetResolutionOptions {
  uint32_t resolution = 0;
  double maximum_component_error = .30, maximum_rms_error = .08;
  void validate() const;
};
struct TextureSetResolutionLayerQuality {
  uint64_t layer = 0;
  std::array<double, 14> contribution_error{};
  std::array<double, 9> value_error{};
  std::array<double, 4> pbr_error{}, pbr_value_error{};
  std::array<double, 7> emission_relative_error{};
  std::array<double, 6> emission_value_error{};
  double physical_absolute_error = 0, rms_error = 0;
};
struct TextureSetResolutionReport {
  bool changed = false;
  uint32_t original_resolution = 0, resolution = 0;
  size_t peak_bytes = 0, work = 0, filtered_samples = 0, output_tiles = 0;
  std::array<double, 14> material_error{};
  double material_rms_error = 0;
  std::vector<TextureSetResolutionLayerQuality> layers;
};
// Detached whole-field operation. Failure/cancellation never publishes a
// partial grid, sibling change or modified membership. Same-size is an exact
// no-op.
TextureSetResolutionReport
resample_texture_set(Scene &, uint64_t, TextureSetId,
                     const TextureSetResolutionOptions &,
                     const TextureSetLimits & = {});
} // namespace dome
