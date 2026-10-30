// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
namespace dome {
enum class ShapeKeyAction { Create, CaptureMix, Duplicate, Mirror, Replace, Rename, Restore };
struct ShapeKeyEdit {
  ShapeKeyAction action=ShapeKeyAction::Create;
  size_t target=0;
  std::string name;
  std::vector<Vec3> positions; // Replace: complete current REST-vertex positions.
  float mirror_tolerance=.0001f; // Object-local metres, not an inferred unit.
};
struct ShapeKeyResult {size_t target=0;bool changed=false;};
// Detached worker operation. REST topology/UVs, skin, pose, paint and other keys
// stay exact; a complete immutable pre-authoring source survives save/reopen.
ShapeKeyResult author_shapekey(Scene &,uint64_t object,const ShapeKeyEdit &,
    const std::function<bool()> &cancelled={});
} // namespace dome
