// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
#include "humanoid-creator.hpp"
namespace dome {
// Detached scene operation; caller publishes on its guarded asset worker.
// Every part shares one persistent creator group/transform and keeps authored
// UV/rest/skin/morph identities. Existing objects and paint remain untouched.
uint64_t append_humanoid(Scene &, const HumanoidControls &,
                         std::string name = "Humanoid",
                         CanvasTransform transform = {}, uint32_t resolution = 256,
                         const std::function<bool()> &cancelled = {});
} // namespace dome
