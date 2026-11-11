// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/avatar.hpp"
#include <algorithm>
namespace dome::detail {
// Only declared anatomical continuations choose another landmark as the tail.
// A hand follows the middle metacarpal, never whichever finger sorts first.
inline std::string humanoid_fit_continuation(const HumanRole &r,
                                             const HumanLandmarks &l) {
  auto parent = [&](const HumanRole &child) {
    std::string p = child.parent;
    while (!p.empty() && !l.points.contains(p)) {
      const auto &roles = human_roles();
      auto found = std::find_if(roles.begin(), roles.end(),
                               [&](const auto &role) { return role.name == p; });
      if (found == roles.end()) return std::string{};
      p = found->parent;
    }
    return p;
  };
  if (r.name == "hips") return "spine";
  if (r.name == "spine" || r.name == "chest" || r.name == "upperChest" ||
      r.name == "neck") {
    for (std::string candidate : {"chest", "upperChest", "neck", "head"})
      if (l.points.contains(candidate))
        for (const auto &child : human_roles())
          if (child.name == candidate && parent(child) == r.name)
            return candidate;
    return {};
  }
  if (r.name.ends_with("Hand")) {
    auto middle = r.name.substr(0, r.name.size() - 4) + "MiddleProximal";
    return l.points.contains(middle) ? middle : std::string{};
  }
  if (r.name.ends_with("Foot")) {
    auto toes = r.name.substr(0, r.name.size() - 4) + "Toes";
    return l.points.contains(toes) ? toes : std::string{};
  }
  if (r.name != "head")
    for (const auto &child : human_roles())
      if (l.points.contains(child.name) && parent(child) == r.name)
        return child.name;
  return {};
}
inline Vec3 humanoid_fit_terminal_axis(const HumanRole &r,
                                       const HumanLandmarks &l,
                                       const std::string &parent) {
  bool finger = false;
  for (auto name : {"Thumb", "Index", "Middle", "Ring", "Little"})
    finger |= r.name.find(name) != std::string::npos;
  if ((finger || r.name.ends_with("Hand")) && !parent.empty()) {
    const auto delta = l.points.at(r.name) - l.points.at(parent);
    // A collapsed parent is refused by fit_humanoid, not assigned a new axis.
    if (length(delta) > 0) return normalized(delta);
  }
  // Foot without Toes retains its existing explicit body-up convention. A
  // shin continuation would point it down; no unplaced toe is manufactured.
  return l.up;
}
} // namespace dome::detail
