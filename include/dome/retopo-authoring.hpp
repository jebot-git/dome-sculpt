// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "retopo.hpp"
#include "scene.hpp"
namespace dome {
// Persistent editable topology. The source reference may outlive its object;
// exported display triangles do not replace these stable records.
struct RetopoPatch {
  uint64_t source = 0;
  RetopoMesh topology;
};
enum class RetopoAction {
  BeginGesture, Sample, EndGesture, CancelGesture,
  Mode, Face, Wire, Delete, Pin, Seam, Relax, Snap, Clear, Undo, Redo
};
struct RetopoView {
  std::shared_ptr<const RetopoMesh> topology;
  std::shared_ptr<const SurfaceQuery> source;
  CanvasTransform transform{};
  std::vector<TopologyID> selected;
  uint64_t revision = 1;
  bool moving = false, can_undo = false, can_redo = false, changed = false;
  bool busy = false, committing = false;
  float snap_mm = 50;
  std::string error;
};
// Worker-owned workspace. Frontends publish only immutable views, enqueue
// bounded samples and never perform topology/projection/triangulation work.
class RetopoWorkspace {
public:
  explicit RetopoWorkspace(const Scene &, const std::function<bool()> & = {});
  const RetopoView &view() const { return view_; }
  uint64_t source_object() const { return source_object_; }
  void edit(RetopoAction, Vec3 = {}, float snap_mm = 50,
            const std::function<bool()> & = {});
private:
  RetopoView view_;
  uint64_t source_object_ = 0;
  std::shared_ptr<const RetopoMesh> original_;
  std::vector<RetopoView> undo_, redo_;
  std::optional<RetopoView> gesture_;
  TopologyID moving_vertex_ = 0;
  bool sampled_ = false;
  void remember(RetopoView);
  void finish_gesture(bool cancel);
};
// Creates a separate atlas/paint-ready object. Existing assets remain retained
// and hidden; each Apply is one document transaction, including reopen/edit.
uint64_t publish_retopology(Scene &, const RetopoMesh &, uint64_t source,
                            CanvasTransform, const std::function<bool()> & = {});
} // namespace dome
