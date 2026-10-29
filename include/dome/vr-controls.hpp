// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "navigation.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>
namespace dome {
enum class VrParameter {
  None,
  BrushRadius,
  Flow,
  Metallic,
  Roughness,
  Opacity,
  LayerOpacity,
  InkColor,
  InkEmission,
  InkMetallic,
  InkRoughness,
  Mask,
  LayerOrder,
  BaseColor,
  UvEdit
};
struct VrControlState {
  VrParameter parameter = VrParameter::None;
  std::string title;
  std::array<float, 4> value{};
  float minimum = 0, maximum = 1;
  int components = 1, selected = 0;
  bool logarithmic = false;
};
enum class VrControlEventType {
  Begin,
  Delta,
  Absolute,
  Select,
  Confirm,
  Cancel,
  UvPick,
  UvMove,
  UvCommand
};
struct VrControlEvent {
  VrControlEventType type;
  VrParameter parameter;
  int component = 0;
  float value = 0, second = 0;
};
struct PanelRayHit {
  ob::Vec3 point;
  float u, v;
};
inline std::optional<PanelRayHit> panel_ray(CanvasTransform panel,
                                            CanvasTransform aim) {
  if (!ob::finite(panel) || !ob::finite(aim) || panel.scale <= 0)
    return {};
  auto inverse = ob::inverse(panel);
  auto origin = ob::transform(inverse, aim.position);
  auto direction =
      ob::rotate(inverse.orientation, ob::rotate(aim.orientation, {0, 0, -1})) *
      inverse.scale;
  if (std::abs(direction.z) < 1e-5f)
    return {};
  float t = -origin.z / direction.z;
  if (t <= 0)
    return {};
  auto p = origin + direction * t;
  float u = p.x / .6f + .5f, v = .5f - p.y / .6f;
  if (u < 0 || u > 1 || v < 0 || v > 1)
    return {};
  return PanelRayHit{ob::transform(panel, p), u, v};
}
std::optional<VrParameter> spatial_parameter(int page, int sector);
} // namespace dome
