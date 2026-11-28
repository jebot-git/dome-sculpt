// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-ray-device.hpp"
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
struct ProjectionFace {
  Vec3 positions[3],normals[3],cage[3];
  Vec3 geometric_normal;
  uint64_t radius_bits;
};
struct ProjectionSettings {
  float front,back;
  bool supplied_tangents,explicit_cage;
};
struct Batch {
  // Block ranges recover the original face/row/column order even when the
  // device reserves compact output ranges out of order.
  std::span<const Block> blocks;
  std::span<const Sample> samples;
  std::span<const bake_rays::ProjectionHit> hits;
  uint32_t overlaps;
};

class Raster {
public:
  virtual ~Raster()=default;
  virtual Batch rasterize(std::span<const Span>,uint32_t candidates,
                          const std::function<bool()> &cancelled)=0;
  virtual Batch rasterize_and_project(std::span<const Span>,std::span<const ProjectionFace>,
      uint32_t candidates,uint64_t ray_budget,uint32_t triangle_budget,
      const std::function<bool()> &cancelled)=0;
};

std::unique_ptr<Raster> create(std::span<const float> axis,std::span<const uint8_t> writable);
} // namespace dome::detail::bake_raster
