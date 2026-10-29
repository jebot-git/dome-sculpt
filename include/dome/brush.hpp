// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "math.hpp"
#include <span>
#include <string_view>
namespace dome {
enum class Tool : int { Clay, Carve, Smooth, Flatten };
struct Brush {
  Vec3 from{}, to{}, normal{0, 1, 0};
  float radius = .1f, strength = 1, pressure = 1, blend = .025f, band = .1f;
  Tool tool = Tool::Clay;
};
struct Sample {
  Vec3 position;
  float distance, average;
};
void apply_brush(std::span<Sample>, const Brush &);
std::string_view compute_backend();
} // namespace dome
