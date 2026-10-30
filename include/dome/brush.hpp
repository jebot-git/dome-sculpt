// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "math.hpp"
#include <span>
#include <string_view>
namespace dome {
enum class Tool : int { Clay, Carve, Smooth, Flatten };
enum class BrushOperation : int { Sculpt, MaskAdd, MaskSubtract, MaskSmooth };
struct Brush {
  Vec3 from{}, to{}, normal{0, 1, 0};
  float radius = .1f, strength = 1, pressure = 1, blend = .025f, band = .1f;
  Tool tool = Tool::Clay;
  // Internal kernel controls: object-local symmetry and independent mask editing.
  BrushOperation operation = BrushOperation::Sculpt;
  unsigned symmetry_axes = 0;
  Vec3 symmetry_origin{};
};
struct Sample {
  Vec3 position;
  float distance, average;
  float influence = 1; // Protection scales delta, never pressure-dependent radius.
};
void apply_brush(std::span<Sample>, const Brush &);
std::string_view compute_backend();
} // namespace dome
