// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh.hpp"
#include "dome/normal-frames.hpp"
#include <functional>
#include <span>
namespace dome::detail {
// Detached structural result only. Runtime/codec admission must own and replay
// the actual before geometry, role-coordinate and attribute owners.
struct KeyMirrorLimits {
  size_t max_bytes=128*1024*1024,max_work=64*1024*1024;
  std::function<bool()> cancelled;
  // Owning refinement factories reserve later endpoint buffers while the
  // geometric kernel still builds its map; old callers retain zero cost.
  size_t reserved_output_corner_bytes=0;
};
// Explicit object-local X plane; direction vectors never receive its offset.
struct KeyMirrorPlane { double offset=0; };
struct KeyMirrorCorrespondence {
  std::vector<uint32_t> corners,vertices;
  double maximum_rest_error=0;
  float tolerance=0;
};
KeyMirrorCorrespondence reflected_key_correspondence(const Mesh &,KeyMirrorPlane,float tolerance,
    const KeyMirrorLimits & = {});
KeyMirrorCorrespondence reflected_key_correspondence(const Mesh &,float tolerance,
    const KeyMirrorLimits & = {});
// Mirror the complete displacement field, without choosing between conflicting
// seam values or requiring a reflected triangle partition. Coincident candidate
// vertices must agree exactly; there is no averaging or silent nearest fallback.
std::vector<Vec3> reflected_key_vertex_deltas(const Mesh &,std::span<const Vec3>,
    KeyMirrorPlane,float tolerance,const KeyMirrorLimits & = {});
// Explicit logical CORNER coordinates of this actual named role. An owning
// factory must derive them from the retained role source, never edited UV0.
std::vector<int8_t> reflected_key_role_signs(const NormalFrames &,
    std::span<const std::array<float,2>> corner_coordinates,
    const KeyMirrorCorrespondence &,const KeyMirrorLimits & = {});
} // namespace dome::detail
