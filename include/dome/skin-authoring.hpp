// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
namespace dome {
enum class SkinAuthoringMode { Automatic, Transfer, SourceVisible };
// Explicit source-assisted imported-rig method: original positive component
// membership, connected-surface entry-wall visibility, fixed4-influence output.
// This is not a volume PDE or general unskinned/native intent solver.
struct SourceVisibleSkinOptions {
  size_t maximum_work = 3'000'000'000ULL;
  size_t maximum_workspace = 256u * 1024u * 1024u;
  inline static constexpr WeightLimits output_limits{4, 1e-5f};
  void validate() const;
};
struct SourceVisibleSkinReport {
  size_t work = 0, planned_workspace = 0, group_parts = 0, changed_parts = 0,
         rows = 0, controls = 0;
};
struct SkinAuthoringOptions {
  uint64_t reference = 0;
  SkinAuthoringMode mode = SkinAuthoringMode::Automatic;
  WeightLimits limits{};
  // Distances are world metres, independent of object/group scale.
  float seed_distance = .3f, seed_margin = .01f, falloff = 2;
  float transfer_distance = .1f;
  size_t maximum_work = 50'000'000;
  // Locks refer to the target's existing exact joint identities. No names or
  // nearest bones are used to guess a correspondence between different rigs.
  std::vector<uint8_t> locks;
  // Separate opt-in resources/policy; legacy maximum_work/limits remain
  // Automatic/Transfer settings and are never silently reset by mode changes.
  SourceVisibleSkinOptions source_visible;
  void validate() const;
};
// Zero-capable logarithmic slider over0..10000 world metres (displayed as mm).
float skin_distance_metres(float normalized);
float skin_distance_slider(float metres);
// Automatic binding accepts fitted skeletons without weights. Transfer requires
// an already skinned reference. Source and destination placements are converted
// explicitly; paint, original appearance, morphs and native source identities
// stay intact. An existing different rig is rejected rather than discarded.
bool author_skin(Scene &, uint64_t target, const SkinAuthoringOptions &,
                 const std::function<bool()> &cancelled = {});
// Compute one source group field and atomically emit only the explicit targets.
// Group members supply immutable read-only support; no target is silently
// added. The report publishes only after a complete successful return,
// including no-op. Geometry, paint and source capsules are retained;
// unsupported owners refuse.
bool author_source_visible_skin(Scene &, std::span<const uint64_t> targets,
                                const SkinAuthoringOptions &,
                                const std::function<bool()> &cancelled = {},
                                SourceVisibleSkinReport *report = nullptr);
} // namespace dome
