// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/material-bindings.hpp"
#include "dome/deformation.hpp"
namespace dome::detail {
// Detached candidate for an EXPLICIT authoring conversion. Creating this value
// neither changes a material nor establishes normal-texture conversion quality.
// The caller must retain its before-state and qualify the resolved texture and
// the declared format-specific evaluation before adopting these attributes.
struct NormalBakeFrameCandidate {
  std::shared_ptr<const NormalFrameSet> frames;
  bool authored_native_normals=false,authored_native_tangents=false;
  bool shared_target_attributes=false;
};
struct NormalBakeFrameOptions {
  size_t max_work=128*1024*1024,max_bytes=128*1024*1024;
  std::function<bool()> cancelled;
};
NormalBakeFrameCandidate normal_bake_frame_candidate(
    const Mesh &,const DeformationState *,const SourceMaterialBindings *,
    const NormalBakeFrameOptions & = {});
// Author ONE new native key against an explicitly chosen immutable base frame.
// Existing artist keys are never modified by this helper. The owner assigns the
// returned stable ID in its registry and retains/restores any replaced key.
// POSITION deltas remain current native vertex aligned; N/T are ordered corners.
struct NativeNormalBakeTarget {
  NormalFrameTarget attributes;
  std::vector<Vec3> positions;
};
NativeNormalBakeTarget normal_bake_author_native_target(const Mesh &,
    const NormalFrames &,std::span<const Vec3> edited_positions,uint64_t target_id,
    const NormalBakeFrameOptions & = {});
// Current UV copies may have split vertices. Map their newly authored POSITION
// deltas back to the captured native origin ONLY by exact ordered corners.
// Contradictory copies, changed rest geometry, or an unreferenced origin vertex
// are not silently averaged/zeroed; the caller must explicitly reauthor origin.
std::vector<Vec3> normal_bake_target_origin_positions(const Mesh &current,
    const Mesh &origin,std::span<const Vec3> current_deltas,
    const NormalBakeFrameOptions & = {});
} // namespace dome::detail
