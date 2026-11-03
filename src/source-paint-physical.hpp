// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/source-paint-bake.hpp"
namespace dome::detail {
// A finite, current Core-PBR view. The original Scene, not this quantized
// image or its metrics, owns the editable float fields and source samplers.
struct SourcePaintPhysicalResult {
  SourcePaintBakeResult material;
  Image emission;
  std::array<float,3> factor{};
  float strength=1;
  double maximum_absolute_error=0, maximum_relative_error=0;
  size_t erased_nonzero_samples=0;
};
SourcePaintPhysicalResult bake_source_paint_physical(
    const SourceUvBakeInput &, const Material &, const SourceUvBakeOptions &);
}
