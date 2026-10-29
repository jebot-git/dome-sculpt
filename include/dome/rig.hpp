// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <nlohmann/json_fwd.hpp>
#include <span>
namespace dome {
using RigMatrix = std::array<float, 16>;
struct JointTransform {
  Vec3 translation{};
  Quat rotation{};
  Vec3 scale{1, 1, 1};
};
struct Bone {
  std::string name;
  int parent = -1;
  JointTransform rest;
  float length = .1f;
  bool deform = true;
};
struct Skeleton {
  std::vector<Bone> bones;
  void validate() const;
  std::vector<RigMatrix>
  world(std::span<const JointTransform> local = {}) const;
  std::vector<std::array<Vec3, 2>>
  segments(std::span<const JointTransform> local = {}) const;
  std::vector<RigMatrix> inverse_bind() const;
  std::vector<RigMatrix>
  skin_matrices(std::span<const JointTransform> local) const;
  nlohmann::json encode() const;
  static Skeleton decode(const nlohmann::json &);
};
struct Influence {
  uint32_t joint = 0;
  float weight = 0;
  auto operator<=>(const Influence &) const = default;
};
using VertexWeights = std::vector<Influence>;
struct WeightLimits {
  uint32_t max_influences = 4;
  float prune = 1e-5f;
};
// Hard-locked positive influences are preserved exactly. Impossible sums/caps
// fail rather than dropping a lock or silently assigning an arbitrary joint.
VertexWeights normalize_weights(VertexWeights, size_t bone_count,
                                std::span<const uint8_t> locks = {},
                                WeightLimits = {});
// Set one unlocked influence to an exact value and redistribute the remainder
// among existing unlocked groups while preserving all hard locks.
VertexWeights paint_weight(VertexWeights, uint32_t joint, float value,
                           size_t bone_count,
                           std::span<const uint8_t> locks = {},
                           WeightLimits = {});
struct SkinWeights {
  std::vector<VertexWeights> vertices;
  void validate(size_t vertex_count, size_t bone_count) const;
  nlohmann::json encode() const;
  static SkinWeights decode(const nlohmann::json &, size_t vertex_count,
                            size_t bone_count);
};
struct BindOptions {
  WeightLimits limits;
  float maximum_seed_distance = .3f;
  float seed_margin = .01f;
  float falloff = 2;
  size_t maximum_work = 50'000'000;
};
// Multi-source bone distances propagate on welded triangle edges. Disconnected
// components without a nearby deform-bone seed fail with a component
// diagnostic.
SkinWeights automatic_bind(const Mesh &, const Skeleton &, BindOptions = {},
                           const std::function<bool()> &cancelled = {});
SkinWeights transfer_weights(const Mesh &source, const SkinWeights &,
                             const Mesh &destination, size_t bone_count,
                             float maximum_distance, WeightLimits = {},
                             const std::function<bool()> &cancelled = {});
// Account BVH construction/comparison, node and triangle visits; retain the
// original overload for existing callers while authoring can choose a budget.
SkinWeights transfer_weights(const Mesh &source, const SkinWeights &,
                             const Mesh &destination, size_t bone_count,
                             float maximum_distance, WeightLimits,
                             const std::function<bool()> &cancelled,
                             size_t maximum_work);
std::vector<uint32_t>
mirror_vertices(const Mesh &, float tolerance = 1e-4f,
                const std::function<bool()> &cancelled = {});
SkinWeights mirror_weights(const Mesh &, const SkinWeights &,
                           std::span<const uint32_t> bone_mirror,
                           float tolerance = 1e-4f);
Mesh deform_mesh(const Mesh &, const SkinWeights &, const Skeleton &,
                 std::span<const JointTransform> local);
uint64_t morph_topology_id(const Mesh &);
struct MorphTarget {
  std::string name;
  std::vector<Vec3> delta;
};
struct MorphSet {
  uint64_t topology = 0;
  std::vector<MorphTarget> targets;
  explicit MorphSet(const Mesh &);
  void validate(const Mesh &) const;
  void set_target(const Mesh &base, std::string name,
                  std::span<const Vec3> edited_positions);
  void mirror_target(const Mesh &, size_t target, std::string name,
                     float tolerance = 1e-4f);
  Mesh evaluate(const Mesh &, std::span<const float> weights) const;
  nlohmann::json encode() const;
  static MorphSet decode(const nlohmann::json &, const Mesh &);
};
} // namespace dome
