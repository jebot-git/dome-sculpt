// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "retopo.hpp"
#include "bake.hpp"
namespace dome {
struct CutSettings {
  float yaw = 0, pitch = 0, offset_mm = 1;
  bool cap = true, flipped = false;
  Material cap_material{"Cut cap", {.8f,.12f,.04f,1}};
  void validate() const;
};
struct CutPlane {
  Vec3 normal{1,0,0};
  float offset = 0;
};
struct CutPreview {
  CutSettings settings;
  CutPlane plane;
  std::shared_ptr<const PlaneCutResult> result;
  Mesh cap_mesh;
  bool changed = false;
};
struct CutView {
  uint64_t session = 0, request = 0, preview_request = 0;
  CutSettings settings;
  Vec3 center{};
  float radius = .5f, offset_limit_mm = 1000;
  std::shared_ptr<const CutPreview> preview;
  bool ready = false, busy = true, committing = false, can_undo = false, can_redo = false;
  std::string error;
};
CutPlane cut_plane(const CutSettings &, Vec3 center);
std::pair<Vec3,float> cut_bounds(const Mesh &);
CutPreview preview_cut(const Mesh &, const CutSettings &, const std::function<bool()> & = {});
struct CutPublishReport { uint64_t shell=0, cap=0, projected=0; bool changed=false; };
// World-space immutable source/provenance. Side paint is resampled by exact
// corner mappings rather than nearest rays. The original object stays retained.
CutPublishReport publish_cut(Scene &, const CutPreview &, const BakeSource &,
                             uint32_t resolution = 512,
                             const std::function<bool()> & = {},
                             const std::function<void(float)> & = {});
} // namespace dome
