// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/import.hpp"
#include "dome/humanoid-creator.hpp"
namespace dome::detail {
struct EmissionSourceInfo {std::array<float,3> factor{};float strength=1;};
inline std::optional<EmissionSourceInfo> emission_source_info(const SculptObject &object) {
  const auto &source=object.appearance;
  if(!source || !source->asset || source->material<0)return {};
  const auto &material=source->asset->materials.at(size_t(source->material));
  return EmissionSourceInfo{material.emissive,material.emissive_strength};
}
}
