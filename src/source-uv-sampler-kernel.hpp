// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint-composite-kernel.hpp"
#include "paint-source-coordinate-kernel.hpp"
#include "source-uv-sampler.hpp"

namespace dome::detail {
static_assert(sizeof(paint_compute::Quantization) == SourceUvSampler::table_bytes);
struct MappedSourceImage {
  const uint8_t *rgba;
  uint32_t width, height;
  int wrap_s, wrap_t;
  bool nearest, srgb;
  SourceUvSampler::Output format;
};

#if defined(__CUDACC__) || defined(__HIPCC__)
__host__ __device__
#endif
inline void sample_mapped_source(MappedSourceImage image,
    SourceUvSampler::Coordinate coordinate, const paint_compute::Quantization &table,
    uint32_t *output, size_t at) {
  using namespace paint_compute;
  const auto u = paint_grid::widen_float_bits(bits(coordinate.u));
  const auto v = paint_grid::widen_float_bits(bits(coordinate.v));
  const SourcePixelTaps taps{
    coordinates::axis(u, image.width, image.wrap_s, image.nearest),
    coordinates::axis(v, image.height, image.wrap_t, image.nearest)
  };
  float color[4];
  sample_source_taps(image.rgba, image.width, image.nearest, taps, image.srgb, table, color);
  using Output = SourceUvSampler::Output;
  if (image.format == Output::EncodedRgba) {
    uint32_t rgba = 0;
    for (unsigned c = 0; c < 4; ++c) {
      const uint32_t byte = image.srgb && c < 3 ? srgb_byte(color[c], table) : linear_byte(color[c]);
      rgba |= byte << (8 * c);
    }
    output[at] = rgba;
  } else if (image.format == Output::LinearRgba) {
    for (unsigned c = 0; c < 4; ++c) output[4 * at + c] = bits(color[c]);
  } else {
    output[at] = bits(color[unsigned(image.format)]);
  }
}
}
