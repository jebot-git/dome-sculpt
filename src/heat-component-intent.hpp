// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "heat-seams.hpp"
#include "heat-source.hpp"
namespace dome::detail {
using HeatJointMask = std::array<uint64_t, 4>;
enum class HeatComponentIntentKind { UserDeclared, OriginalPositiveUsage };
struct HeatComponentDeclaration {
  uint32_t vertex = 0;
  std::vector<uint32_t> joints;
};
class HeatComponentIntent {
  uint64_t object_ = 0;
  std::shared_ptr<const PaintedMesh> paint_;
  std::shared_ptr<const Mesh> mesh_;
  std::shared_ptr<const DeformationState> deformation_;
  std::shared_ptr<const CreatorAppearance> appearance_;
  std::vector<HeatJointMask> masks_;
  HeatComponentIntentKind kind_ = HeatComponentIntentKind::UserDeclared;
  size_t work_ = 0, bytes_ = 0, components_ = 0;
  template <class T, class U>
  static bool same(const std::shared_ptr<T> &a, const std::shared_ptr<U> &b) {
    return a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a);
  }
  friend HeatComponentIntent
  capture_imported_component_intent(const SculptObject &, const HeatSeamGraph &,
                                    const HeatBindingOptions &,
                                    const std::function<bool()> &);
  friend HeatComponentIntent capture_user_component_intent(
      const SculptObject &, std::span<const HeatComponentDeclaration>,
      const HeatBindingOptions &, const std::function<bool()> &);

public:
  bool matches(const SculptObject &o) const {
    return object_ == o.id && same(paint_, o.painted) && o.painted &&
           same(mesh_, o.painted->mesh_owner()) &&
           same(deformation_, o.deformation) && same(appearance_, o.appearance);
  }
  std::span<const HeatJointMask> masks(const SculptObject &o) const {
    if (!matches(o))
      throw std::invalid_argument(
          "Component intent source/geometry/rig owner changed");
    return masks_;
  }
  HeatComponentIntentKind kind() const { return kind_; }
  size_t work() const { return work_; }
  size_t planned_bytes() const { return bytes_; }
  size_t components() const { return components_; }
};
// Factory-local graph: only explicit current edges and approved exact seams.
// No position-near welding and no source-weight numerical values in this graph.
inline std::vector<uint32_t>
heat_intent_components(const Mesh &m, std::span<const uint32_t> seams,
                       size_t &work, size_t maximum,
                       const std::function<bool()> &cancelled) {
  auto step = [&](size_t n = 1) {
    if (n > maximum - work)
      throw std::length_error("Component intent work cap");
    work += n;
    if ((work & 1023) < n && cancelled && cancelled())
      throw std::runtime_error("Component intent cancelled");
  };
  std::vector<uint32_t> parent(m.positions.size());
  std::iota(parent.begin(), parent.end(), 0);
  auto root = [&](uint32_t x) {
    while (x != parent[x]) {
      step();
      parent[x] = parent[parent[x]];
      x = parent[x];
    }
    return x;
  };
  if (!seams.empty()) {
    if (seams.size() != parent.size())
      throw std::invalid_argument("Intent seam dimensions changed");
    for (uint32_t i = 0; i < parent.size(); ++i) {
      step();
      if (seams[i] >= parent.size())
        throw std::invalid_argument("Intent seam out of bounds");
      parent[root(i)] = root(seams[i]);
    }
  }
  for (size_t c = 0; c < m.indices.size(); c += 3) {
    step(3);
    auto a = root(m.indices[c]), b = root(m.indices[c + 1]),
         d = root(m.indices[c + 2]);
    parent[a] = b;
    parent[root(b)] = d;
  }
  for (uint32_t i = 0; i < parent.size(); ++i) {
    step();
    parent[i] = root(i);
  }
  return parent;
}
inline HeatComponentIntent capture_user_component_intent(
    const SculptObject &o,
    std::span<const HeatComponentDeclaration> declarations,
    const HeatBindingOptions &options,
    const std::function<bool()> &cancelled = {}) {
  if (cancelled && cancelled())
    throw std::runtime_error("Component intent cancelled");
  if (!o.id || !o.painted || !o.deformation || !o.deformation->skeleton ||
      !options.maximum_work || options.maximum_work > 500'000'000 ||
      options.maximum_bytes < 4096 ||
      options.maximum_bytes > 512u * 1024u * 1024u)
    throw std::invalid_argument(
        "Invalid declared component-intent owners/resources");
  const auto &m = o.painted->mesh();
  const auto &rig = *o.deformation->skeleton;
  const size_t n = m.positions.size(), b = rig.bones.size(),
               bytes = n * 80 + m.indices.size() * 16 + b * b * 64 + 8192;
  if (!n || n > 2'000'000 || !b || b > 256 || declarations.empty() ||
      declarations.size() > n || bytes > options.maximum_bytes)
    throw std::length_error("Declared intent exceeds construction bounds");
  HeatComponentIntent result;
  result.object_ = o.id;
  result.paint_ = o.painted;
  result.mesh_ = o.painted->mesh_owner();
  result.deformation_ = o.deformation;
  result.appearance_ = o.appearance;
  result.bytes_ = bytes;
  auto step = [&](size_t x) {
    if (x > options.maximum_work - result.work_)
      throw std::length_error("Declared intent work cap");
    result.work_ += x;
    if (cancelled && cancelled())
      throw std::runtime_error("Declared intent cancelled");
  };
  step(n + m.indices.size() + b * b);
  m.validate();
  rig.validate();
  auto parent = heat_intent_components(m, {}, result.work_,
                                       options.maximum_work, cancelled);
  result.masks_.resize(n);
  for (const auto &declaration : declarations) {
    step(1);
    if (declaration.vertex >= n || declaration.joints.empty() ||
        declaration.joints.size() > b)
      throw std::invalid_argument("Invalid current component declaration");
    for (auto joint : declaration.joints) {
      step(1);
      if (joint >= b || !rig.bones[joint].deform)
        throw std::invalid_argument(
            "Declared source joint is outside current deform domain");
      result.masks_[parent[declaration.vertex]][joint / 64] |= uint64_t(1)
                                                               << (joint % 64);
    }
  }
  for (uint32_t i = 0; i < n; ++i) {
    step(1);
    if (parent[i] == i) {
      ++result.components_;
      if (result.masks_[i] == HeatJointMask{})
        throw std::invalid_argument(
            "User intent omitted a connected component");
    }
  }
  for (uint32_t i = 0; i < n; ++i) {
    step(1);
    result.masks_[i] = result.masks_[parent[i]];
  }
  return result;
}
inline HeatComponentIntent
capture_imported_component_intent(const SculptObject &o,
                                  const HeatSeamGraph &seams,
                                  const HeatBindingOptions &options,
                                  const std::function<bool()> &cancelled = {}) {
  if (cancelled && cancelled())
    throw std::runtime_error("Imported component intent cancelled");
  if (!seams.matches(o) || !o.appearance || !o.appearance->asset ||
      !o.deformation || !o.deformation->humanoid || !o.deformation->skeleton)
    throw std::invalid_argument(
        "Imported component intent lacks exact current source/rig owners");
  const auto &m = o.painted->mesh();
  const auto &a = *o.appearance;
  const auto &rig = *o.deformation->skeleton;
  const size_t n = m.positions.size(), b = rig.bones.size(),
               bytes = n * 96 + m.indices.size() * 160 + b * b * 64 + 8192;
  if (!n || n > 2'000'000 || !b || b > 256 || options.maximum_bytes < 4096 ||
      options.maximum_bytes > 512u * 1024u * 1024u || !options.maximum_work ||
      options.maximum_work > 500'000'000 || bytes >= options.maximum_bytes)
    throw std::length_error(
        "Imported intent exceeds bounded capture resources");
  HeatComponentIntent result;
  result.object_ = o.id;
  result.paint_ = o.painted;
  result.mesh_ = o.painted->mesh_owner();
  result.deformation_ = o.deformation;
  result.appearance_ = o.appearance;
  result.bytes_ = bytes;
  result.kind_ = HeatComponentIntentKind::OriginalPositiveUsage;
  auto remaining = options;
  remaining.maximum_bytes -= bytes;
  auto profile = imported_heat_profile(rig, a, remaining, cancelled,
                                       &*o.deformation->humanoid,
                                       ImportedHeatPolicy::OriginalPrimitive);
  result.work_ = profile.work;
  result.bytes_ += profile.planned_bytes;
  auto step = [&](size_t x = 1) {
    if (x > options.maximum_work - result.work_)
      throw std::length_error("Imported intent work cap");
    result.work_ += x;
    if ((result.work_ & 1023) < x && cancelled && cancelled())
      throw std::runtime_error("Imported intent cancelled");
  };
  if (a.source_vertices.size() != n)
    throw std::invalid_argument(
        "Imported intent lacks exact current source vertex map");
  const auto global = a.asset->meshes.at(size_t(a.source_mesh))
                          .primitives.at(size_t(a.source_primitive));
  const auto &p = a.asset->primitives.at(global);
  const auto &node = a.asset->nodes.at(size_t(a.source_node));
  const auto &skin = a.asset->skins.at(size_t(node.skin));
  if (p.mesh.indices.size() != m.indices.size())
    throw std::invalid_argument("Imported intent source face count changed");
  step(n + m.indices.size());
  m.validate();
  const size_t map_work =
      64 * std::bit_width(p.mesh.indices.size() / 3 + 1) + 32;
  std::map<std::array<uint32_t, 3>, size_t> triangles;
  for (size_t c = 0; c < p.mesh.indices.size(); c += 3) {
    step(map_work);
    std::array<uint32_t, 3> t{p.mesh.indices[c], p.mesh.indices[c + 1],
                              p.mesh.indices[c + 2]};
    std::sort(t.begin(), t.end());
    ++triangles[t];
  }
  for (size_t c = 0; c < m.indices.size(); c += 3) {
    step(map_work);
    std::array<uint32_t, 3> t;
    for (int k = 0; k < 3; ++k) {
      auto v = m.indices[c + k];
      if (v >= n || a.source_vertices[v] >= p.mesh.positions.size())
        throw std::invalid_argument(
            "Imported intent source corner map invalid");
      t[k] = a.source_vertices[v];
    }
    std::sort(t.begin(), t.end());
    auto at = triangles.find(t);
    if (at == triangles.end() || !at->second)
      throw std::invalid_argument(
          "Imported intent source face identity changed");
    --at->second;
  }
  for (const auto &[key, count] : triangles) {
    (void)key;
    step();
    if (count)
      throw std::invalid_argument("Imported intent dropped source triangles");
  }
  auto parent = heat_intent_components(m, seams.groups(o), result.work_,
                                       options.maximum_work, cancelled);
  result.masks_.resize(n);
  std::map<uint32_t, uint32_t> native;
  for (uint32_t i = 0; i < b; ++i) {
    step(16);
    if (!native.emplace(a.source_joint_nodes[i], i).second)
      throw std::invalid_argument("Imported intent joint identity ambiguous");
  }
  for (uint32_t i = 0; i < n; ++i) {
    auto source = a.source_vertices[i];
    for (const auto &set : p.influences) {
      step(1);
      if (set.weights.size() != p.mesh.positions.size() ||
          set.joints.size() != set.weights.size())
        throw std::invalid_argument(
            "Imported intent influence dimensions changed");
      for (int k = 0; k < 4; ++k) {
        step(16);
        const float value = set.weights.at(source)[k];
        // Only the positivity predicate exits source capture. No magnitude is
        // transferred into a boundary value, energy, radius, RHS or solution.
        if (!std::isfinite(value) || value < 0)
          throw std::invalid_argument("Invalid original usage predicate");
        if (!(value > 0))
          continue;
        auto source_node = skin.joints.at(set.joints.at(source)[k]);
        auto at = native.find(source_node);
        if (at == native.end())
          throw std::invalid_argument(
              "Imported component usage has foreign joint");
        const auto joint = at->second;
        if (rig.bones[joint].deform) {
          if (!profile.eligible[joint])
            throw std::invalid_argument(
                "Component usage exceeds authenticated primitive palette");
          result.masks_[parent[i]][joint / 64] |= uint64_t(1) << (joint % 64);
        }
      }
    }
  }
  for (uint32_t i = 0; i < n; ++i) {
    step();
    if (parent[i] == i) {
      ++result.components_;
      if (result.masks_[i] == HeatJointMask{})
        throw std::invalid_argument(
            "Original usage leaves a component unassigned");
    }
  }
  for (uint32_t i = 0; i < n; ++i) {
    step();
    result.masks_[i] = result.masks_[parent[i]];
  }
  return result;
}
} // namespace dome::detail
