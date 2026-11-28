// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace dome::detail::bake_raster {
inline constexpr uint32_t candidate_capacity=65'536;
inline constexpr uint32_t span_capacity=4096;
inline constexpr uint32_t candidates_per_block=32;

struct Span {
  uint64_t area_bits;
  float uv[3][2];
  uint32_t first,left,bottom,width,rectangle_offset,top_left_edges;
};
struct Sample {
  uint32_t pixel=UINT32_MAX,span;
  float barycentric[3];
};
struct Block {
  uint32_t offset,count;
};
struct Totals {
  uint32_t samples,overlaps;
};
struct Batch {
  // Block ranges recover the original face/row/column order even when the
  // device reserves compact output ranges out of order.
  std::span<const Block> blocks;
  std::span<const Sample> samples;
  uint32_t overlaps;
};

class Raster {
public:
  virtual ~Raster()=default;
  virtual Batch rasterize(std::span<const Span>,uint32_t candidates,
                          const std::function<bool()> &cancelled)=0;
};

std::unique_ptr<Raster> create(std::span<const float> axis,std::span<const uint8_t> writable);
} // namespace dome::detail::bake_raster
