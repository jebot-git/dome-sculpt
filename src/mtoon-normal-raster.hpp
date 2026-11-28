// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/math.hpp"
#include <cstdint>
#include <functional>
#include <span>

namespace dome { struct Image; }
namespace dome::detail::mtoon_normal_raster {
inline constexpr uint32_t batch_texels=65536,group_size=128;
enum class Grid { Native,Independent };

struct Input {
 const Image *original;
 const Image &detail,&coverage;
 uint32_t width,height;
 float scale;
 std::span<const uint8_t> visible;
 Grid grid=Grid::Independent;
 Vec3 constant{0,0,1};
};

// The export worker budgets one resident copy of each source and cell mask,
// plus bounded scratch. Returns the worst sine bound over the admitted cells
// or 1 if the angular certificate cannot be established.
size_t device_bytes(const Input &);
size_t native_device_bytes(uint32_t resolution,bool has_source);
float raster(const Input &,std::span<uint8_t> rgba,const std::function<bool()> &cancelled);
}
