// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/source-maps.hpp"
#include "source-texture-axis.hpp"
#include <span>

namespace dome::detail {
// Validated immutable source sampled at output texel centers. Axis-aligned
// transforms reuse the same wrapped taps down each column and across each row.
class SourceTextureGrid {
public:
  SourceTextureGrid(const PaintSourceTexture &, uint32_t width, uint32_t height,
                    bool srgb);
  std::array<float,4> sample(uint32_t x, uint32_t y) const;
  std::span<const SourceTextureAxis> columns() const noexcept { return columns_; }
  std::span<const SourceTextureAxis> rows() const noexcept { return rows_; }

private:
  PaintSourceTexture source_;
  uint32_t width_, height_;
  bool srgb_;
  std::vector<SourceTextureAxis> columns_, rows_;
};
}
