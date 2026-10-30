// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "affine.hpp"
#include "creator-archive.hpp"
#include "groups.hpp"
#include <span>
namespace dome {
class Scene;
struct SculptObject;
// Model-wide original node authority. Native instance is provenance/cache
// partitioning data, not a requirement that a historical group still exists.
struct SourceNodePlacement {
  std::shared_ptr<const CreatorSourceModel> model;
  uint32_t source_node=0;
  uint64_t source_instance=0;
  void validate() const;
  AffineTransform local() const;
};
// Native parent-local affine prefix, independent of original source authority.
// The existing CanvasTransform remains a native similarity delta after it.
// Physical world gestures conjugate this prefix by the complete parent frame.
class NativeAffineEdit {
public:
  explicit NativeAffineEdit(AffineTransform);
  const AffineTransform &matrix() const noexcept{return matrix_;}
private:
  AffineTransform matrix_;
};
// Detached complete placement before a conversion. IDs are historical data;
// no live Scene lookup is used to validate or evaluate this capsule. Restoring
// original source content keeps current placement unless explicitly requested.
struct SourcePlacementCapsule {
  CanvasTransform object_transform{};
  std::shared_ptr<const NativeAffineEdit> object_edit;
  std::vector<SceneGroup> groups;
  void validate() const;
  AffineTransform world_matrix() const;
};
// This conversion validates finite positive similarity, not Scene's historical
// object-position/scale UI bounds; tracking/world gesture deltas have other units.
AffineTransform affine_similarity(CanvasTransform);
// Group local = E_native * TRS_native * A_original. Object local = E_native*TRS.
AffineTransform group_local_matrix(const SceneGroup &);
AffineTransform object_local_matrix(const SculptObject &);
bool has_affine_placement(const SculptObject &,std::span<const SceneGroup>);
std::shared_ptr<const SourcePlacementCapsule> capture_source_placement(const Scene &,uint64_t);
} // namespace dome
