// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
#include <span>
namespace dome {
enum class ObjectSelection { Replace, Extend, Toggle };
struct ObjectHierarchyRow {
  uint64_t id=0;
  unsigned depth=0;
  std::string name;
  bool group=false, expanded=false, selected=false, partial=false, active=false;
  bool visible=true, locked=false;
};
// Shared by controller hit testing and bitmap rasterization. Normalized panel
// coordinates, nine comfortable rows on the existing 60cm spatial palette.
namespace object_outliner {
inline constexpr float first=.24f, row_height=.065f;
inline constexpr size_t page_rows=9;
}
std::vector<ObjectHierarchyRow> object_hierarchy(const Scene &,
    std::span<const uint64_t> selected, std::span<const uint64_t> collapsed);
} // namespace dome
