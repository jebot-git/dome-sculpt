// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/emission-interchange.hpp"
namespace dome::detail {
// Bounded Core header inspection only. No pixel/chunk-output allocation.
size_t emission_exr_decoded_bytes(std::span<const uint8_t>,
                                  const EmissionInterchangeLimits &);
}
