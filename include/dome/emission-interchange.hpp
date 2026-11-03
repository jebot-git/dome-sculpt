// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "emission.hpp"
#include <cstdint>
#include <functional>
#include <span>
#include <vector>
namespace dome {
struct EmissionInterchangeLimits {
 uint32_t max_dimension=8192;
 size_t max_pixels=16*1024*1024,max_encoded_bytes=256*1024*1024,max_working_bytes=256*1024*1024;
 std::function<bool()> cancelled;
};
// Linear FLOAT32 channels: resolved R/G/B, premultiplied legacy RGB,
// premultiplied absolute physical RGB, then replacement coverage. Resolved RGB
// is an explicitly evaluated view; the seven contribution channels retain the
// independent material-relative and absolute terms for a capable consumer.
struct EmissionInterchangeImage {
 uint32_t width=0,height=0;
 std::vector<std::array<float,10>> pixels;
 void validate(const EmissionInterchangeLimits & = {})const;
};
std::vector<uint8_t> encode_emission_exr(const EmissionInterchangeImage &,const EmissionInterchangeLimits & = {});
EmissionInterchangeImage decode_emission_exr(std::span<const uint8_t>,const EmissionInterchangeLimits & = {});
}
