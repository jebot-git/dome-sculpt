// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/emission-interchange.hpp"
namespace dome::detail {
struct EmissionExrDescription {size_t decoded_bytes=0;uint32_t version=0,width=0,height=0;};
EmissionExrDescription emission_exr_description(std::span<const uint8_t>,
                                               const EmissionInterchangeLimits &);
// Bounded Core header inspection only. No pixel/chunk-output allocation.
size_t emission_exr_decoded_bytes(std::span<const uint8_t>,
                                  const EmissionInterchangeLimits &);
}
