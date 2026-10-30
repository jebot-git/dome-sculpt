// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh-processing.hpp"
#include "polygon.hpp"
#include <deque>
namespace dome {
struct PolygonDrawData {
  std::vector<float> move_weights;
  std::vector<uint32_t> wire_indices, selected_edges, selected_points;
  Vec3 selection_center{};
  bool has_selection = false;
};
struct PolygonView {
  std::shared_ptr<const PolygonMesh> topology;
  std::shared_ptr<const PolygonRender> surface;
  std::shared_ptr<const PolygonSelection> selection;
  std::shared_ptr<const PolygonDrawData> display;
  Vec3 move_delta{}; // GPU preview only; source triangles/BVH stay immutable.
  uint64_t revision = 1;
  bool changed = false, can_undo = false, can_redo = false;
  bool moving = false, busy = false, committing = false;
  std::string error;
};
// Worker-owned component workspace. All snapshots own immutable geometry and
// selection; frontends never build triangulations/BVHs or mutate this object.
class PolygonWorkspace {
public:
  explicit PolygonWorkspace(PolygonMesh, const PolygonLimits & = {});
  const PolygonView &view() const { return *current_.view; }
  void mode(PolygonSelectionMode, const PolygonLimits & = {});
  void select(PolygonId, bool additive = false, bool toggle = false,
              const PolygonLimits & = {});
  void select_all(const PolygonLimits & = {});
  void clear_selection(const PolygonLimits & = {});
  bool select_ray(Vec3 origin, Vec3 direction, float maximum_distance,
                  float component_radius, bool additive = false,
                  bool toggle = false, const PolygonLimits & = {});
  void apply(PolygonOperation, Vec3 delta = {}, float amount = 0,
             unsigned cuts = 1, const PolygonLimits & = {});
  void begin_move();
  // Total displacement from Begin, not a sum of lossy floating-point deltas.
  void move_to(Vec3 total_delta, const PolygonLimits & = {});
  void end_move(bool accept, const PolygonLimits & = {});
  bool undo();
  bool redo();
  PolygonId identity_cursor() const { return high_water_; }

private:
  struct Snapshot {
    std::shared_ptr<const PolygonView> view;
    std::shared_ptr<const SurfaceQuery> query;
  };
  Snapshot current_;
  std::shared_ptr<const PolygonMesh> original_;
  std::deque<Snapshot> undo_, redo_;
  std::optional<Snapshot> gesture_;
  PolygonId high_water_ = 1;
  size_t maximum_history_components_ = 8'000'000;
  Snapshot prepare(PolygonMesh, PolygonSelection, const PolygonLimits &) const;
  Snapshot decorate(Snapshot, bool can_undo, bool can_redo, bool moving) const;
  void selection(PolygonSelection, const PolygonLimits &);
  void commit(Snapshot, Snapshot before);
  void trim_history();
};
} // namespace dome
