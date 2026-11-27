// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint-composite-kernel.hpp"
#include <functional>
#include <span>
#include <vector>
namespace dome::paint_compute {
struct BakeImages {
  std::span<const uint8_t> coverage,color,orm,emission,normal;
  size_t size_bytes() const noexcept {
    return coverage.size()+color.size()+orm.size()+emission.size()+normal.size();
  }
  Bake view() const noexcept {
    return coverage.empty()?Bake{}:Bake{coverage.data(),color.data(),orm.data(),emission.data(),
                                      normal.empty()?nullptr:normal.data()};
  }
  Bake device_view(const uint8_t *buffer) const noexcept {
    if(coverage.empty())return {};
    const auto *colors=buffer+coverage.size();
    const auto *orms=colors+color.size();
    const auto *emissions=orms+orm.size();
    return {buffer,colors,orms,emissions,normal.empty()?nullptr:emissions+emission.size()};
  }
};
struct Input {
  Parameters parameters;
  std::vector<Layer> layers;
  std::vector<uint32_t> tiles;
  std::vector<Pixel> pixels;
  std::vector<int32_t> padding;
  BakeImages bake;
};
struct Result {
  std::vector<Output> pixels;
  std::vector<uint8_t> normals;
};
const Quantization &quantization();
void validate(const Input &,const std::function<bool()> &cancelled);
void checkpoint(const std::function<bool()> &cancelled);
// Selected backend only: all image results remain detached until every chunk
// and the final cancellation check succeed. No renderer/model memory aliases.
Result composite(const Input &,const std::function<bool()> &cancelled={});
inline constexpr size_t chunk_pixels=65536,maximum_pixels=4*1024*1024,maximum_tile_bytes=128*1024*1024;
}
