// SPDX-License-Identifier: GPL-3.0-or-later
#include "brush_kernel.hpp"
namespace dome {
void apply_brush(std::span<Sample> samples, const Brush &b) {
  for (auto &s : samples)
    s.distance = brush_sample(s, b);
}
std::string_view compute_backend() { return "CPU"; }
} // namespace dome
