// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mtoon.hpp"

namespace dome::detail {
// Batch callers validate their immutable material once before mapping samples.
std::array<float, 2> mtoon_uv_validated(const MToonMaterial &, MToonSlot,
                                       std::array<float, 2>, double seconds, float mask);
}
