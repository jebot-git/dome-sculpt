// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint-composite-kernel.hpp"
#include <functional>
#include <span>
#include <vector>
namespace dome::paint_compute {
struct Input {Parameters parameters;std::vector<Layer> layers;std::vector<uint32_t> tiles;std::vector<Pixel> pixels;std::vector<int32_t> padding;};
const Quantization &quantization();
void validate(const Input &,const std::function<bool()> &cancelled);
void checkpoint(const std::function<bool()> &cancelled);
// Selected backend only: all image results remain detached until every chunk
// and the final cancellation check succeed. No renderer/model memory aliases.
std::vector<Output> composite(const Input &,const std::function<bool()> &cancelled={});
inline constexpr size_t chunk_pixels=65536,maximum_pixels=4*1024*1024,maximum_tile_bytes=128*1024*1024;
}
