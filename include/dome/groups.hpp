// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "navigation.hpp"
#include <memory>
#include <cstdint>
#include <string>
namespace dome {
struct HumanoidControls;
struct SceneGroup {
  uint64_t id = 0, parent = 0;
  std::string name;
  CanvasTransform transform{};
  std::shared_ptr<const HumanoidControls> humanoid{};
};
} // namespace dome
