// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "heat-component-intent.hpp"
#include "weight-nearest-support.hpp"
#include "weight-surface-system.hpp"
namespace dome::detail {
// Private worker input. Empty declarations select geometry-derived connected
// components with the current authored deform-bone set, not artist skin values.
// Optional declarations are explicit user intent, not a prerequisite for the
// eventual automatic/native route. No source/appearance identity is fabricated.
struct DeclaredWeightPart {
  uint64_t object = 0;
  std::vector<HeatComponentDeclaration> declarations;
};
enum class DeclaredSupportPolicy { AllDeformControls, NearestSourceUnion };
struct DeclaredWeightSupport {
  DeclaredSupportPolicy policy = DeclaredSupportPolicy::AllDeformControls;
  std::vector<DeclaredWeightPart> parts;
  const DeclaredWeightPart *find(uint64_t id) const {
    auto at = std::find_if(parts.begin(), parts.end(),
                           [&](const auto &p) { return p.object == id; });
    return at == parts.end() ? nullptr : &*at;
  }
};
inline ImportedHeatProfile
declared_weight_profile(const Skeleton &rig, weight_surface::Budget &work) {
  if (rig.bones.empty() || rig.bones.size() > 256)
    throw std::length_error("Declared weight control dimensions exceeded");
  work.step_items(rig.bones.size(), rig.bones.size() * 32 + 64);
  rig.validate();
  ImportedHeatProfile profile; // private data container; source remains null
  profile.eligible.resize(rig.bones.size());
  const auto world = rig.world();
  auto head = [&](size_t j) {
    return std::array<double, 3>{world[j][12], world[j][13], world[j][14]};
  };
  profile.segments.reserve(rig.bones.size() * 2);
  for (uint32_t i = 0; i < rig.bones.size(); ++i)
    if (rig.bones[i].deform) {
      profile.eligible[i] = 1;
      bool child = false;
      for (uint32_t j = 0; j < rig.bones.size(); ++j) {
        work.step();
        if (rig.bones[j].parent == int(i)) {
          profile.segments.push_back({head(i), head(j), i});
          child = true;
        }
      }
      if (!child)
        profile.segments.push_back({head(i), head(i), i});
    }
  if (profile.segments.empty())
    throw std::invalid_argument("Declared weight rig has no deform source");
  return profile;
}
inline HeatComponentIntent declared_weight_intent(
    const SculptObject &o, const DeclaredWeightPart &input,
    const ImportedHeatProfile &profile, const HeatBindingOptions &options,
    weight_surface::Budget &work, DeclaredSupportPolicy policy) {
  if (input.object != o.id || !o.painted || !o.deformation ||
      !o.deformation->skeleton)
    throw std::invalid_argument("Declared weight source lease is incomplete");
  std::vector<HeatComponentDeclaration> generated;
  auto declarations =
      std::span<const HeatComponentDeclaration>(input.declarations);
  if (declarations.empty()) {
    const auto &m = o.painted->mesh();
    if (m.positions.size() > 2'000'000 || m.indices.size() > 6'000'000 ||
        m.positions.size() > options.maximum_bytes / 32)
      throw std::length_error(
          "Default weight intent exceeds construction bounds");
    size_t local_work = 0;
    auto parents = heat_intent_components(
        m, {}, local_work,
        std::min(options.maximum_work, work.maximum - work.used), work.cancel);
    work.step(local_work);
    size_t components = 0, labels = 0;
    for (size_t i = 0; i < parents.size(); ++i) {
      work.step();
      components += parents[i] == i;
    }
    for (auto x : profile.eligible)
      labels += bool(x);
    const size_t unit =
        sizeof(HeatComponentDeclaration) + labels * sizeof(uint32_t);
    if (!labels || components > options.maximum_bytes / unit ||
        parents.size() * sizeof(uint32_t) >
            options.maximum_bytes - components * unit)
      throw std::length_error(
          "Default weight intent declaration budget exceeded");
    const size_t metadata =
        parents.size() * (sizeof(uint32_t) + sizeof(HeatJointMask)) +
        components * unit;
    if (metadata > options.maximum_bytes)
      throw std::length_error("Nearest component support metadata exceeded");
    const size_t intent_bytes =
        parents.size() * 80 + m.indices.size() * 16 +
        profile.eligible.size() * profile.eligible.size() * 64 + 8192;
    if (intent_bytes > options.maximum_bytes - metadata)
      throw std::length_error("Default intent simultaneous workspace exceeded");
    std::vector<HeatJointMask> masks(parents.size());
    if (policy == DeclaredSupportPolicy::NearestSourceUnion) {
      NearestWeightSources sources(profile.segments, work);
      for (size_t i = 0; i < parents.size(); ++i) {
        const auto p = m.positions[i];
        const auto selected = sources.nearest({p.x, p.y, p.z}, work);
        for (size_t k = 0; k < 4; ++k)
          masks[parents[i]][k] |= selected[k];
      }
    } else
      for (size_t i = 0; i < parents.size(); ++i)
        if (parents[i] == i)
          for (size_t j = 0; j < profile.eligible.size(); ++j)
            if (profile.eligible[j])
              masks[i][j / 64] |= uint64_t(1) << (j % 64);
    generated.reserve(components);
    for (uint32_t i = 0; i < parents.size(); ++i)
      if (parents[i] == i) {
        work.step(labels + 1);
        HeatComponentDeclaration d;
        d.vertex = i;
        d.joints.reserve(labels);
        for (uint32_t j = 0; j < profile.eligible.size(); ++j)
          if (masks[i][j / 64] & (uint64_t(1) << (j % 64)))
            d.joints.push_back(j);
        if (d.joints.empty())
          throw std::invalid_argument(
              "Default component has no nearest declared source");
        generated.push_back(std::move(d));
      }
    declarations = generated;
  }
  auto remaining = options;
  remaining.maximum_work =
      std::min(remaining.maximum_work, work.maximum - work.used);
  if (!remaining.maximum_work)
    throw std::length_error("Declared intent has no remaining work");
  auto result =
      capture_user_component_intent(o, declarations, remaining, work.cancel);
  work.step(result.work());
  return result;
}
} // namespace dome::detail
