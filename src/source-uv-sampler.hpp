// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>

namespace dome {
struct PaintSourceTexture;
namespace detail {
// UVs supplied by the caller already include the source role's transform.
class SourceUvSampler {
public:
  struct Coordinate { float u, v; };
  struct TexelAffine {
    float scale[4]{},offset[4]{};
    unsigned channels=0;
  };
  enum class Output : uint8_t { Red, Green, Blue, Alpha, LinearRgba, EncodedRgba, LinearAssociatedRgba };
  enum class Transform { ResolvedCoordinates, SourceAffine };
  static constexpr size_t words(Output output) {
    return output == Output::LinearRgba || output == Output::LinearAssociatedRgba ? 4 : 1;
  }
  static constexpr size_t max_batch = 16384;
  static constexpr size_t table_bytes = 2 * 256 * sizeof(float) + 256 * sizeof(uint32_t);
  virtual ~SourceUvSampler() = default;
  // Encoded output is one RGBA word per sample, with R in the low byte.
  // Linear output is four binary32 channel words or one selected channel.
  void read(std::span<const Coordinate> coordinates, std::span<uint32_t> output) {
    if (coordinates.empty() || coordinates.size() > capacity_ ||
        output.size() != coordinates.size() * words_per_sample_)
      throw std::invalid_argument("Invalid source UV sample batch");
    for (const auto &uv : coordinates)
      if (!std::isfinite(uv.u) || !std::isfinite(uv.v))
        throw std::invalid_argument("Invalid original texture sample coordinate");
    dispatch(coordinates, output);
  }

  // Rewrite original texels in place in their own coordinate domain. The
  // caller owns the detached destination. Unselected channel bytes pass through.
  void rewrite(size_t first,const TexelAffine &affine,std::span<uint32_t> output) {
    if(output.empty()||output.size()>capacity_||first>pixel_count_||output.size()>pixel_count_-first||
       !affine.channels||affine.channels>15)
      throw std::invalid_argument("Invalid source texture rewrite batch");
    for(unsigned c=0;c<4;++c)if(affine.channels&(1u<<c)){
      const auto a=affine.scale[c],b=affine.offset[c];
      if(!std::isfinite(a)||!std::isfinite(b)||a<0||b<0||a+b>1.000001f)
        throw std::invalid_argument("Invalid source texture channel transform");
    }
    rewrite_texels(first,affine,output);
  }

protected:
  SourceUvSampler(size_t capacity, Output output,size_t pixel_count)
      : capacity_(capacity),words_per_sample_(words(output)),pixel_count_(pixel_count) {
    if (!capacity || capacity > max_batch)
      throw std::invalid_argument("Invalid source UV sample capacity");
    if (output > Output::LinearAssociatedRgba)
      throw std::invalid_argument("Invalid source UV sample output");
  }

private:
  virtual void dispatch(std::span<const Coordinate>, std::span<uint32_t>) = 0;
  virtual void rewrite_texels(size_t,const TexelAffine &,std::span<uint32_t>) = 0;
  size_t capacity_;
  size_t words_per_sample_;
  size_t pixel_count_;
};
std::unique_ptr<SourceUvSampler> upload_source_uv_sampler(
    const PaintSourceTexture &, bool srgb, SourceUvSampler::Output, size_t capacity,
    SourceUvSampler::Transform = SourceUvSampler::Transform::ResolvedCoordinates);
}
}
