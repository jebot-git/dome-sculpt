// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rig.hpp"
#include <optional>
namespace dome {
enum class WeightBrushMode { Add, Subtract, Replace, Smooth };
struct WeightBrush {
  Vec3 from{}, to{};
  float radius = .05f, strength = .2f, pressure = 1, value = 1;
  uint32_t joint = 0;
  WeightBrushMode mode = WeightBrushMode::Add;
};
struct WeightPaintSurface {
  uint64_t object = 0;
  const Mesh *rest = nullptr;
  const SkinWeights *weights = nullptr;
  // Explicit common brush-coordinate positions. Supply current skinned/morphed
  // world positions for posed editing. Empty means rest.positions already share
  // the brush coordinate frame; no object transform is guessed.
  std::span<const Vec3> positions;
};
struct WeightBrushSymmetry {
  float plane_x = 0;
  // Explicit involution; joint names and left/right semantics are never inferred.
  std::vector<uint32_t> joints;
};
struct WeightEditOptions {
  std::vector<uint8_t> locks;
  WeightLimits limits;
  std::optional<WeightBrushSymmetry> symmetry;
  size_t maximum_vertices = 2'000'000, maximum_work = 50'000'000;
};
struct WeightSurfaceEdit {
  uint64_t object = 0, topology = 0;
  SkinWeights weights;
  size_t affected = 0, changed = 0, constrained = 0;
};
struct WeightEditResult {
  std::vector<WeightSurfaceEdit> surfaces;
  size_t affected = 0, changed = 0, constrained = 0;
  // At most eight representative constraint diagnostics; counts remain exact.
  std::vector<std::string> diagnostics;
};
// Detached, all-input-validating capsule stroke. No topology/source identity or
// input data changes. Cancellation/resource errors throw before publishing any
// result. Impossible hard-lock assignments remain exact and report constraints.
WeightEditResult edit_weights(std::span<const WeightPaintSurface>, size_t bones,
                              const WeightBrush &, WeightEditOptions = {},
                              const std::function<bool()> &cancelled = {});
// One gesture uses the maximum capsule falloff, evaluated from original
// weights once. Repeated frame samples cannot accumulate weight merely because
// a device reports more frequently. Joint/mode/target must match across samples.
WeightEditResult edit_weights(std::span<const WeightPaintSurface>, size_t bones,
                              std::span<const WeightBrush>, WeightEditOptions = {},
                              const std::function<bool()> &cancelled = {});
} // namespace dome
