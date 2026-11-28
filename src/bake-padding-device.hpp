// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <functional>
#include <span>

namespace dome::detail::bake_padding {
struct Pixel {
  uint32_t target,source;
};
using Apply=std::function<void(std::span<const Pixel>)>;

// Coverage 1 is a seed, 0 is fillable, and other values block the frontier.
// Return only padding pairs. Each source is an original projected texel.
void resolve(uint32_t resolution,uint32_t radius,std::span<const uint8_t> coverage,
             const Apply &,const std::function<bool()> &cancelled);
}
