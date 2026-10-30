// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon.hpp"
#include "scene.hpp"
namespace dome {
struct PolygonOrigin;
// Stable editable topology, not the triangulated display/paint derivative.
// An active authority with no faces intentionally displays no surface, even if
// the retained object Volume still contains an earlier sculpt.
struct PolygonAuthority {
  PolygonMesh topology;
  bool active = true;
  std::shared_ptr<const PolygonOrigin> origin;
};
// Detached immutable authoring snapshot. Ordinary topology edits retain the
// first origin. An explicit later conversion may retain a prior authority;
// native archives bound and validate that DAG rather than following live IDs.
struct PolygonOrigin {
  std::shared_ptr<const Volume> volume;
  std::shared_ptr<const PaintedMesh> paint;
  Material material;
  std::shared_ptr<const DeformationState> deformation;
  std::shared_ptr<const CreatorAppearance> appearance;
  std::shared_ptr<const SourceMaterialBindings> material_bindings;
  std::shared_ptr<const RetopoPatch> retopology;
  std::shared_ptr<const PolygonAuthority> polygon;
  bool mesh_active = false;
  CanvasTransform world_transform{};
  std::string name;
  // Provenance only; zero and deleted source IDs remain valid and restorable.
  uint64_t source_object = 0;
};
// Compare the active painted REST derivative only. Posed preview positions and
// unrelated source attributes must not replace the editable topology authority.
void validate_polygon_derivative(const PolygonAuthority &, const PaintedMesh *,
                                 bool mesh_active, const PolygonLimits & = {});
} // namespace dome
