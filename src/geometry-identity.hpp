// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include <bit>
namespace dome::geometry_detail {
// Exact dependency identity, never a fingerprint of packed mesh contents. All
// source owners are held strongly while the opaque identity is visible to a
// renderer, so allocation-address reuse cannot impersonate older geometry.
struct Dependency {
  uint64_t object = 0;
  Volume::GeometryOwner volume;
  float voxel = 0, band = 0;
  std::shared_ptr<const Mesh> paint;
  std::shared_ptr<const PaintSamplingAttachment> paint_sampling_attachment;
  std::shared_ptr<const std::vector<std::array<float,2>>> source_uv1;
  std::map<uint32_t,std::shared_ptr<const std::vector<std::array<float,2>>>> source_extra_uv;
  std::shared_ptr<const DeformationState> deformation;
  std::shared_ptr<const CreatorAppearance> appearance;
  std::shared_ptr<const SourceMaterialBindings> material_bindings;
  std::shared_ptr<const PolygonAuthority> polygon;
  std::shared_ptr<const detail::FramePositionDomain> frame_positions;
  CanvasTransform world;
  std::optional<AffineTransform> affine_world;
  bool canonical = false;
  uint8_t frame_role = 0;
};
template <class T>
bool same_owner(const std::shared_ptr<const T> &a,
                const std::shared_ptr<const T> &b) {
  return a == b && !a.owner_before(b) && !b.owner_before(a);
}
inline bool same(float a, float b) {
  return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b);
}
inline bool same(CanvasTransform a, CanvasTransform b) {
  return same(a.position.x, b.position.x) && same(a.position.y, b.position.y) &&
         same(a.position.z, b.position.z) &&
         same(a.orientation.x, b.orientation.x) &&
         same(a.orientation.y, b.orientation.y) &&
         same(a.orientation.z, b.orientation.z) &&
         same(a.orientation.w, b.orientation.w) && same(a.scale, b.scale);
}
struct Identity {
  uint64_t generation = 0;
  std::vector<Dependency> objects;
  bool matches(const Identity &other) const {
    if (generation != other.generation ||
        objects.size() != other.objects.size())
      return false;
    for (size_t i = 0; i < objects.size(); ++i) {
      const auto &a = objects[i], &b = other.objects[i];
      if(a.source_extra_uv.size()!=b.source_extra_uv.size())return false;
      for(const auto &[set,owner]:a.source_extra_uv){auto found=b.source_extra_uv.find(set);if(found==b.source_extra_uv.end()||!same_owner(owner,found->second))return false;}
      if (a.object != b.object || !same_owner(a.volume, b.volume) ||
          !same(a.voxel, b.voxel) || !same(a.band, b.band) ||
          !same_owner(a.paint, b.paint) || !same_owner(a.paint_sampling_attachment,b.paint_sampling_attachment) || !same_owner(a.source_uv1,b.source_uv1) ||
          !same_owner(a.deformation, b.deformation) ||
          !same_owner(a.appearance, b.appearance) ||
          !same_owner(a.material_bindings, b.material_bindings) || !same_owner(a.frame_positions,b.frame_positions) ||
          !same_owner(a.polygon, b.polygon) ||
          a.canonical != b.canonical || a.frame_role != b.frame_role || !same(a.world, b.world) ||
          bool(a.affine_world)!=bool(b.affine_world) || (a.affine_world && a.affine_world->matrix()!=b.affine_world->matrix()))
        return false;
    }
    return true;
  }
};
} // namespace dome::geometry_detail
