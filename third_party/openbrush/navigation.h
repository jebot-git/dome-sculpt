// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "brush-math.h"
#include <array>
#include <cstdint>
#include <optional>
namespace ob {
struct TrackedHand {
  Transform grip, aim;
  bool valid = false, aimValid = false;
  float trigger = 0, squeeze = 0;
  uint32_t timeMs = 0;
};
class Navigation {
  int mask = 0;
  Transform start, base;
  Vec3 startVector;

public:
  std::optional<Transform> update(const std::array<TrackedHand, 2> &hands,
                                  Transform current);
  bool grabbing() const { return mask != 0; }
  void reset() { mask = 0; }
};
} // namespace ob
