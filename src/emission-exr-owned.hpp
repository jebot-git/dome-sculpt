// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/emission-interchange.hpp"
namespace dome::detail {
// Per-call raw storage provider, including deterministic null-return tests.
// The codec owns every successful allocation until its paired release.
struct EmissionExrMemory {
 void *state=nullptr;
 void *(*allocate)(void *,size_t) noexcept=nullptr;
 void (*release)(void *,void *) noexcept=nullptr;
};
std::vector<uint8_t> encode_emission_exr_owned(const EmissionInterchangeImage &,
 const EmissionInterchangeLimits & = {},EmissionExrMemory={});
EmissionInterchangeImage decode_emission_exr_owned(std::span<const uint8_t>,
 const EmissionInterchangeLimits & = {},EmissionExrMemory={});
size_t emission_exr_owned_decoded_bytes(std::span<const uint8_t>,
 const EmissionInterchangeLimits &,EmissionExrMemory={});
}
