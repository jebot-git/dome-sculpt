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
  enum class Output : uint8_t { Red, Green, Blue, Alpha, LinearRgba, EncodedRgba };
  static constexpr size_t words(Output output) { return output == Output::LinearRgba ? 4 : 1; }
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

protected:
  SourceUvSampler(size_t capacity, Output output) : capacity_(capacity), words_per_sample_(words(output)) {
    if (!capacity || capacity > max_batch)
      throw std::invalid_argument("Invalid source UV sample capacity");
    if (output > Output::EncodedRgba)
      throw std::invalid_argument("Invalid source UV sample output");
  }

private:
  virtual void dispatch(std::span<const Coordinate>, std::span<uint32_t>) = 0;
  size_t capacity_;
  size_t words_per_sample_;
};
std::unique_ptr<SourceUvSampler> upload_source_uv_sampler(
    const PaintSourceTexture &, bool srgb, SourceUvSampler::Output, size_t capacity);
}
}
