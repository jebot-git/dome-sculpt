// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <functional>

namespace dome {
class PaintedMesh;
namespace detail {
// Includes occupied sparse tiles after padding with the CPU compositor's
// layer order. The caller budgets images and geometry separately.
size_t native_grid_composite_work(const PaintedMesh &,size_t maximum,
    const std::function<bool()> &cancelled);
}
}
