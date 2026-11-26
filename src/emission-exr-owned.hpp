// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/emission-interchange.hpp"
#include "emission-exr-access.hpp"
namespace dome::detail {
// Per-call raw storage provider, including deterministic null-return tests.
// The codec owns every successful allocation until its paired release.
using EmissionExrPixel = std::array<float, 10>;

struct EmissionExrMemory {
 void *state=nullptr;
 void *(*allocate)(void *,size_t) noexcept=nullptr;
 void (*release)(void *,void *) noexcept=nullptr;
};
std::vector<uint8_t> encode_emission_exr_owned(const EmissionInterchangeImage &,
 const EmissionInterchangeLimits & = {},EmissionExrMemory={});
// Callbacks consume or produce consecutive scanlines in row-major order.
using EmissionExrSource = std::function<void(uint32_t, std::span<EmissionExrPixel>)>;
using EmissionExrSink = std::function<void(uint32_t, std::span<const EmissionExrPixel>)>;
std::vector<uint8_t> encode_emission_exr_rows(uint32_t width, uint32_t height,
    uint32_t version, const EmissionExrSource &, const EmissionInterchangeLimits &,
    EmissionExrMemory = {});
EmissionExrDescription read_emission_exr_rows(std::span<const uint8_t>,
    const EmissionExrSink &, const EmissionInterchangeLimits &, EmissionExrMemory = {});
void validate_emission_exr_pixels(std::span<const EmissionExrPixel>, uint32_t version,
    const EmissionInterchangeLimits &);
EmissionInterchangeImage decode_emission_exr_owned(std::span<const uint8_t>,
 const EmissionInterchangeLimits & = {},EmissionExrMemory={});
size_t emission_exr_owned_decoded_bytes(std::span<const uint8_t>,
 const EmissionInterchangeLimits &,EmissionExrMemory={});
EmissionExrDescription emission_exr_owned_description(std::span<const uint8_t>,
 const EmissionInterchangeLimits &,EmissionExrMemory={}, bool streaming=false);
}
