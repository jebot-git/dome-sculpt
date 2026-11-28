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
  uint32_t affine[6]{};
  bool apply_affine=false;
};

inline MappedSourceImage mapped_source_image(const PaintSourceTexture &source,const uint8_t *device,
    bool srgb,SourceUvSampler::Output format,SourceUvSampler::Transform mode) {
  MappedSourceImage result{device,source.image->width,source.image->height,source.wrap_s,source.wrap_t,
      source.mag_filter==9728,srgb,format};
  result.apply_affine=mode==SourceUvSampler::Transform::SourceAffine&&
      source.uv_transform!=std::array<float,6>{1,0,0,1,0,0};
  for(size_t k=0;k<6;++k)result.affine[k]=std::bit_cast<uint32_t>(source.uv_transform[k]);
  return result;
}

#if defined(__CUDACC__) || defined(__HIPCC__)
__host__ __device__
#elif defined(__SYCL_DEVICE_ONLY__)
[[clang::noinline]]
#endif
inline uint64_t source_affine_axis(uint32_t c0,uint32_t c1,uint32_t offset,uint64_t u,uint64_t v) {
  using namespace paint_grid;
  return add(add(mul(widen_float_bits(c0),u),mul(widen_float_bits(c1),v)),
             widen_float_bits(offset));
}

#if defined(__CUDACC__) || defined(__HIPCC__)
__host__ __device__
#endif
inline void sample_mapped_source(MappedSourceImage image,
    SourceUvSampler::Coordinate coordinate, const paint_compute::Quantization &table,
    uint32_t *output, size_t at) {
  using namespace paint_compute;
  auto u = paint_grid::widen_float_bits(bits(coordinate.u));
  auto v = paint_grid::widen_float_bits(bits(coordinate.v));
  if(image.apply_affine){
    // Each product has two binary32 operands and fits exactly in binary64.
    // Add in the same order as the reference sampler without FMA or FLOAT narrowing.
    const auto x=u,y=v;
    u=source_affine_axis(image.affine[0],image.affine[1],image.affine[4],x,y);
    v=source_affine_axis(image.affine[2],image.affine[3],image.affine[5],x,y);
  }
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
