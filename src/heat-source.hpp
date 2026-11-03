// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/avatar.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/import.hpp"
#include "heat-binding.hpp"
#include <map>
namespace dome::detail {
// Private imported-source producer. No rig axis, topology or source ID is
// inferred from a display name. Current joint heads retain the actual metric;
// the unchanged original parent graph authenticates the endpoint identities.
inline std::vector<HeatSourceSegment>
imported_heat_segments(const Skeleton &rig, const CreatorAppearance &source,
                       const std::function<bool()> &cancelled = {}) {
  if (!source.asset || source.source_joint_nodes.size() != rig.bones.size())
    throw std::invalid_argument(
        "Imported bone heat needs complete exact source joint identities");
  rig.validate();
  const auto &nodes = source.asset->nodes;
  std::map<uint32_t, uint32_t> native;
  for (uint32_t i = 0; i < rig.bones.size(); ++i) {
    if (cancelled && cancelled())
      throw std::runtime_error("Bone heat source capture cancelled");
    auto node = source.source_joint_nodes[i];
    if (node >= nodes.size() || !native.emplace(node, i).second)
      throw std::invalid_argument(
          "Imported bone heat joint map is invalid or duplicated");
  }
  for (uint32_t i = 0; i < rig.bones.size(); ++i) {
    int parent = nodes[source.source_joint_nodes[i]].parent, actual = -1;
    size_t depth = 0;
    while (parent >= 0) {
      if (cancelled && cancelled())
        throw std::runtime_error("Bone heat source capture cancelled");
      if (size_t(parent) >= nodes.size() || ++depth > nodes.size())
        throw std::invalid_argument(
            "Imported bone heat source parent graph is invalid");
      auto at = native.find(uint32_t(parent));
      if (at != native.end()) {
        actual = int(at->second);
        break;
      }
      parent = nodes[size_t(parent)].parent;
    }
    if (actual != rig.bones[i].parent)
      throw std::invalid_argument(
          "Imported bone heat requires an authenticated current/source parent "
          "correspondence");
  }
  auto world = rig.world();
  auto head = [&](size_t i) {
    return std::array<double, 3>{world[i][12], world[i][13], world[i][14]};
  };
  std::vector<HeatSourceSegment> out;
  out.reserve(rig.bones.size() * 2);
  for (uint32_t i = 0; i < rig.bones.size(); ++i)
    if (rig.bones[i].deform) {
      bool child = false;
      for (uint32_t j = 0; j < rig.bones.size(); ++j) {
        if (cancelled && cancelled())
          throw std::runtime_error("Bone heat source capture cancelled");
        if (rig.bones[j].parent == int(i)) {
          out.push_back({head(i), head(j), i});
          child = true;
        }
      }
      if (!child)
        out.push_back({head(i), head(i), i});
    }
  return out;
}
} // namespace dome::detail

namespace dome::detail {
enum class ImportedHeatPolicy { WholeSkin, OriginalPrimitive };
struct ImportedHeatProfile {
  std::shared_ptr<const ImportedAsset> source;
  std::vector<uint8_t> eligible;
  std::vector<HeatSourceSegment> segments;
  size_t work = 0, planned_bytes = 0;
  std::vector<uint32_t> point_forks;
};
// Explicit ORIGINAL-SKIN-USAGE policy. Does not silently reinterpret native
// fitted rigs or user-added joint identities as historical imported joints.
inline ImportedHeatProfile imported_heat_profile(
    const Skeleton &rig, const CreatorAppearance &appearance,
    const HeatBindingOptions &options,
    const std::function<bool()> &cancelled = {},
    const HumanoidMapping *humanoid = nullptr,
    ImportedHeatPolicy policy = ImportedHeatPolicy::WholeSkin) {
  if (!options.maximum_work || options.maximum_work > 500'000'000 ||
      options.maximum_bytes < 4096 ||
      options.maximum_bytes > 512u * 1024u * 1024u)
    throw std::invalid_argument(
        "Invalid imported heat capture resource bounds");
  size_t work = 0;
  auto step = [&](size_t n = 1) {
    if (n > options.maximum_work - work)
      throw std::length_error("Imported heat capture exceeds its work budget");
    work += n;
    if ((work & 1023) < n && cancelled && cancelled())
      throw std::runtime_error("Imported heat capture cancelled");
  };
  if (cancelled && cancelled())
    throw std::runtime_error("Imported heat capture cancelled");
  if (!appearance.asset ||
      appearance.source_joint_nodes.size() != rig.bones.size() ||
      rig.bones.empty() || rig.bones.size() > 256)
    throw std::invalid_argument(
        "Original-skin heat requires complete unchanged imported joint "
        "identities; native/added joints need explicit role authority");
  const auto &asset = *appearance.asset;
  const auto &nodes = asset.nodes;
  if (appearance.source_node < 0 ||
      size_t(appearance.source_node) >= nodes.size() ||
      nodes[size_t(appearance.source_node)].skin < 0)
    throw std::invalid_argument(
        "Original-skin heat lacks an exact source node/skin");
  const size_t b = rig.bones.size();
  if (asset.skins.size() > options.maximum_bytes ||
      b * b * 64 + b * 512 + asset.skins.size() > options.maximum_bytes)
    throw std::length_error("Imported heat capture exceeds its memory budget");
  ImportedHeatProfile result;
  result.source = appearance.asset;
  result.planned_bytes = b * b * 64 + b * 512 + asset.skins.size();
  step(b * b);
  rig.validate();
  std::map<uint32_t, uint32_t> native;
  for (uint32_t i = 0; i < b; ++i) {
    step(16);
    auto node = appearance.source_joint_nodes[i];
    if (node >= nodes.size() || !native.emplace(node, i).second)
      throw std::invalid_argument(
          "Original-skin heat source joint map is invalid or duplicated");
  }
  for (uint32_t i = 0; i < b; ++i) {
    int parent = nodes[appearance.source_joint_nodes[i]].parent, actual = -1;
    size_t depth = 0;
    while (parent >= 0) {
      step(16);
      if (size_t(parent) >= nodes.size() || ++depth > nodes.size())
        throw std::invalid_argument(
            "Original-skin heat source parent closure is invalid");
      auto at = native.find(uint32_t(parent));
      if (at != native.end()) {
        actual = int(at->second);
        break;
      }
      parent = nodes[size_t(parent)].parent;
    }
    if (actual != rig.bones[i].parent)
      throw std::invalid_argument(
          "Original-skin heat current/source parent correspondence changed");
  }
  std::vector<uint8_t> skins(asset.skins.size());
  for (size_t skin = 0; skin < asset.skins.size(); ++skin) {
    step();
    size_t matches = 0;
    for (auto node : asset.skins[skin].joints) {
      step(16);
      matches += native.contains(node);
    }
    if (matches && matches != asset.skins[skin].joints.size())
      throw std::invalid_argument(
          "Original-skin heat cannot mix partial foreign joint mappings across "
          "original skins");
    skins[skin] = matches > 0;
  }
  const auto primary = size_t(nodes[size_t(appearance.source_node)].skin);
  if (primary >= skins.size() || !skins[primary])
    throw std::invalid_argument("Original-skin heat primary skin does not "
                                "match current exact joint identities");
  if (policy != ImportedHeatPolicy::WholeSkin &&
      policy != ImportedHeatPolicy::OriginalPrimitive)
    throw std::invalid_argument("Unknown imported heat joint-palette policy");
  uint32_t selected_primitive = UINT32_MAX;
  if (policy == ImportedHeatPolicy::OriginalPrimitive) {
    if (appearance.source_mesh < 0 ||
        size_t(appearance.source_mesh) >= asset.meshes.size() ||
        nodes[size_t(appearance.source_node)].mesh != appearance.source_mesh ||
        appearance.source_primitive < 0 ||
        size_t(appearance.source_primitive) >=
            asset.meshes[size_t(appearance.source_mesh)].primitives.size())
      throw std::invalid_argument("Original-primitive heat lacks exact source "
                                  "mesh/primitive/node correspondence");
    selected_primitive = asset.meshes[size_t(appearance.source_mesh)]
                             .primitives[size_t(appearance.source_primitive)];
  }
  result.eligible.assign(b, 0);
  for (const auto &node : nodes) {
    step();
    if (policy == ImportedHeatPolicy::OriginalPrimitive &&
        &node != &nodes[size_t(appearance.source_node)])
      continue;
    if (node.skin < 0 || node.mesh < 0)
      continue;
    if (size_t(node.skin) >= skins.size() ||
        size_t(node.mesh) >= asset.meshes.size())
      throw std::invalid_argument(
          "Original-skin heat source mesh/skin index is invalid");
    if (!skins[size_t(node.skin)])
      continue;
    const auto &skin = asset.skins[size_t(node.skin)];
    for (auto primitive : asset.meshes[size_t(node.mesh)].primitives) {
      step();
      if (policy == ImportedHeatPolicy::OriginalPrimitive &&
          primitive != selected_primitive)
        continue;
      if (primitive >= asset.primitives.size())
        throw std::invalid_argument(
            "Original-skin heat source primitive index is invalid");
      const auto &p = asset.primitives[primitive];
      for (const auto &set : p.influences) {
        step();
        if (set.joints.size() != set.weights.size() ||
            set.weights.size() != p.mesh.positions.size())
          throw std::invalid_argument(
              "Original-skin heat influence dimensions differ");
        // Accessors may be shared by distinct material primitives. Only the
        // actual indexed draw witnesses primitive joint usage; unused rows do
        // not authorize sources. Duplicate indices are deliberately revisited
        // and charged, avoiding an unbounded deduplication allocation.
        for (auto v : p.mesh.indices) {
          step();
          if (v >= set.weights.size())
            throw std::invalid_argument(
                "Original-skin heat drawn vertex index is invalid");
          for (int c = 0; c < 4; ++c) {
            step(16);
            float weight = set.weights[v][c];
            if (!std::isfinite(weight) || weight < 0)
              throw std::invalid_argument(
                  "Original-skin heat has invalid source weights");
            if (weight == 0)
              continue;
            auto slot = set.joints[v][c];
            if (slot >= skin.joints.size())
              throw std::invalid_argument(
                  "Original-skin heat source joint slot is invalid");
            auto at = native.find(skin.joints[slot]);
            if (at == native.end())
              throw std::invalid_argument("Original-skin heat positive source "
                                          "weight has no exact native joint");
            if (rig.bones[at->second].deform)
              result.eligible[at->second] = 1;
          }
        }
      }
    }
  }
  // Primary source chains are semantic humanoid metadata, not node labels.
  // Eligibility controls emission only; authentic unweighted end nodes remain
  // physical geometry for an otherwise eligible distal/secondary source.
  if (humanoid) {
    step(b * b);
    humanoid->validate(rig, true);
  }
  auto world = rig.world();
  auto head = [&](size_t i) {
    return std::array<double, 3>{world[i][12], world[i][13], world[i][14]};
  };
  result.segments.reserve(b * 2);
  std::map<uint32_t, std::string> role_by_joint;
  if (humanoid)
    for (const auto &[name, joint] : humanoid->joints) {
      step();
      role_by_joint.emplace(joint, name);
    }
  auto preferred = [&](uint32_t joint) -> std::optional<uint32_t> {
    if (!humanoid)
      return {};
    auto at = role_by_joint.find(joint);
    if (at == role_by_joint.end())
      return {};
    const auto &role = at->second;
    std::vector<std::string> candidates;
    if (role == "hips")
      candidates = {"spine"};
    else if (role == "spine")
      candidates = {"chest", "upperChest", "neck", "head"};
    else if (role == "chest")
      candidates = {"upperChest", "neck", "head"};
    else if (role == "upperChest")
      candidates = {"neck", "head"};
    else if (role == "neck")
      candidates = {"head"};
    for (const std::string side : {"left", "right"}) {
      if (role == side + "UpperLeg")
        candidates = {side + "LowerLeg"};
      else if (role == side + "LowerLeg")
        candidates = {side + "Foot"};
      else if (role == side + "Foot")
        candidates = {side + "Toes"};
      else if (role == side + "Shoulder")
        candidates = {side + "UpperArm"};
      else if (role == side + "UpperArm")
        candidates = {side + "LowerArm"};
      else if (role == side + "LowerArm")
        candidates = {side + "Hand"};
      else if (role == side + "ThumbMetacarpal")
        candidates = {side + "ThumbProximal"};
      else if (role == side + "ThumbProximal")
        candidates = {side + "ThumbDistal"};
      for (const std::string finger : {"Index", "Middle", "Ring", "Little"}) {
        if (role == side + finger + "Proximal")
          candidates = {side + finger + "Intermediate"};
        else if (role == side + finger + "Intermediate")
          candidates = {side + finger + "Distal"};
      }
    }
    for (const auto &name : candidates) {
      step();
      auto target = humanoid->joints.find(name);
      if (target != humanoid->joints.end())
        return target->second;
    }
    return {};
  };
  for (uint32_t i = 0; i < b; ++i)
    if (result.eligible[i]) {
      step();
      auto target = preferred(i);
      std::vector<uint32_t> children;
      if (target) {
        uint32_t child = *target;
        while (rig.bones[child].parent != int(i)) {
          step();
          int parent = rig.bones[child].parent;
          if (parent < 0)
            throw std::invalid_argument(
                "Humanoid heat primary chain is outside its authenticated "
                "source ancestry");
          child = uint32_t(parent);
        }
        children.push_back(child);
      } else {
        for (uint32_t j = 0; j < b; ++j) {
          step();
          if (rig.bones[j].parent == int(i))
            children.push_back(j);
        }
      }
      if (children.size() == 1)
        result.segments.push_back({head(i), head(children[0]), i});
      else {
        if (children.size() > 1)
          result.point_forks.push_back(i);
        result.segments.push_back({head(i), head(i), i});
      }
    }
  if (result.segments.empty())
    throw std::invalid_argument(
        "Original-skin heat has no eligible source joints");
  result.work = work;
  return result;
}
} // namespace dome::detail
