// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <memory>
#include <optional>
#include <nlohmann/json_fwd.hpp>
namespace dome {
// Attribute transport is independent of the material's TBN interpolation policy.
enum class NormalFrameTransport { FixedAttributes, CanonicalMikk };
struct NormalFrameTarget {
  uint64_t id=0;
  std::vector<Vec3> normals,tangents;
};
struct NormalFrames {
  NormalFrameTransport transport=NormalFrameTransport::FixedAttributes;
  bool generated_tangents=false;
  std::optional<uint32_t> generated_uv;
  uint32_t origin_vertex_count=0,source_vertex_count=0,source_corner_count=0;
  // Exact ordered current-paint corners. UV-only remapping preserves order.
  std::vector<uint32_t> origin_vertices,source_vertices,source_corners;
  std::vector<Vec3> normals;
  std::vector<std::array<float,4>> tangents; // Empty is a genuine N-only frame.
  std::vector<NormalFrameTarget> targets;
  void validate(bool deep=true) const;
  size_t storage_bytes() const;
  nlohmann::json encode() const;
  static NormalFrames decode(const nlohmann::json &);
};
struct FrameTargetIdentity {
  uint64_t id=0;
  std::optional<uint32_t> origin_slot,source_target;
  // Empty: captured original native slot is authority. Nonempty: explicitly
  // reauthored native POSITION delta, aligned to retained origin vertices.
  std::vector<Vec3> positions;
};
// Explicit native attribute skin convention. Shader TBN interpolation remains
// material/version specific and is not encoded in this transport policy.
enum class NormalFrameSkinPolicy { WeightedJointInverseTranspose = 1 };
enum class NativeCanonicalNormalEvaluation { RawInterpolatedVertexTbn = 1 };
struct NormalFrameSet {
  NormalFrameSkinPolicy skin_policy=NormalFrameSkinPolicy::WeightedJointInverseTranspose;
  NativeCanonicalNormalEvaluation canonical_evaluation=NativeCanonicalNormalEvaluation::RawInterpolatedVertexTbn;
  std::shared_ptr<const NormalFrames> pbr,mtoon,canonical;
  std::vector<FrameTargetIdentity> targets;
  std::vector<uint64_t> current_targets; // Current native slot -> stable frame ID.
  uint64_t next_target=1;
  void validate(bool deep=true) const;
  size_t storage_bytes() const;
};
class NormalFrameArchive {
public:
  uint32_t add(std::shared_ptr<const NormalFrameSet>,bool deep=true);
  const std::shared_ptr<const NormalFrameSet> &at(uint32_t) const;
  nlohmann::json encode() const;
  size_t storage_bytes() const;
  static NormalFrameArchive decode(const nlohmann::json &);
  const auto &sets() const { return sets_; }
private:
  std::vector<std::shared_ptr<const NormalFrameSet>> sets_;
};
} // namespace dome
