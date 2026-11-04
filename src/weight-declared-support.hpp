// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "heat-component-intent.hpp"
#include "weight-nearest-support.hpp"
#include "weight-surface-system.hpp"
#include <bit>
namespace dome::detail {
// Private worker input. Empty declarations select geometry-derived connected
// components with the current authored deform-bone set, not artist skin values.
// Optional declarations are explicit user intent, not a prerequisite for the
// eventual automatic/native route. No source/appearance identity is fabricated.
struct DeclaredWeightPart {
  uint64_t object = 0;
  std::vector<HeatComponentDeclaration> declarations;
};
enum class DeclaredSupportPolicy {
  AllDeformControls,
  NearestSourceUnion,
  // Native geometry evidence only. A boundary tie is not a component owning
  // witness for a foreign control. An all-tied component refuses; no artist
  // values, geometry weld or hidden fallback supplies the missing intent.
  UniqueSourceWitnesses
};
enum class DeclaredRigGeometry { NativeBoneAxes, ChildHeadControl };
// Separate screened-energy construction, not a fallback after a failed solve.
// Component masks still own permitted controls; only the closest permitted
// finite source(s) anchor each row. Exact ties retain all joint identities.
enum class DeclaredRowAnchors { AllVisible, NearestPermitted };
enum class DeclaredSourceRequirements { AllEligible, GeometryEvidence };
enum class DeclaredSourceSpan { ComponentProjection, AuthoredFinite };
enum class DeclaredOriginBoundary {
  StrictOwnership,
  NativeOpenSegment,
  NativeFiniteLineIntegral
};
struct DeclaredWeightSupport {
  DeclaredSupportPolicy policy = DeclaredSupportPolicy::AllDeformControls;
  bool world_metric = false;
  std::vector<DeclaredWeightPart> parts;
  uint64_t reference = 0;
  DeclaredRigGeometry rig_geometry = DeclaredRigGeometry::NativeBoneAxes;
  DeclaredRowAnchors row_anchors = DeclaredRowAnchors::AllVisible;
  DeclaredSourceRequirements source_requirements =
      DeclaredSourceRequirements::AllEligible;
  DeclaredSourceSpan source_span = DeclaredSourceSpan::ComponentProjection;
  DeclaredOriginBoundary origin_boundary =
      DeclaredOriginBoundary::StrictOwnership;
  const DeclaredWeightPart *find(uint64_t id) const {
    auto at = std::find_if(parts.begin(), parts.end(),
                           [&](const auto &p) { return p.object == id; });
    return at == parts.end() ? nullptr : &*at;
  }
};
inline ImportedHeatProfile
declared_weight_profile(const Skeleton &rig, weight_surface::Budget &work,
                        DeclaredRigGeometry geometry) {
  if (rig.bones.empty() || rig.bones.size() > 256)
    throw std::length_error("Declared weight control dimensions exceeded");
  work.step_items(rig.bones.size(), rig.bones.size() * 32 + 64);
  rig.validate();
  ImportedHeatProfile profile; // private data container; source remains null
  profile.eligible.resize(rig.bones.size());
  if (geometry == DeclaredRigGeometry::NativeBoneAxes) {
    const auto actual = rig.segments();
    profile.segments.reserve(actual.size());
    for (uint32_t i = 0; i < actual.size(); ++i) {
      work.step(32);
      if (!rig.bones[i].deform)
        continue;
      profile.eligible[i] = 1;
      const auto &a = actual[i][0], &b = actual[i][1];
      profile.segments.push_back({{a.x, a.y, a.z}, {b.x, b.y, b.z}, i});
    }
    if (profile.segments.empty())
      throw std::invalid_argument("Native rig has no authored deform handles");
    return profile;
  }
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
// New geometry-only intent type: it never fabricates a target skin/skeleton to
// satisfy the imported-source HeatComponentIntent factory. The genuine source
// rig owner is retained independently from the target's optional deformation.
class DeclaredWeightIntent {
  uint64_t object_ = 0, revision_ = 0;
  std::shared_ptr<const PaintedMesh> paint_;
  std::shared_ptr<const Mesh> mesh_;
  std::shared_ptr<const DeformationState> target_, reference_;
  std::vector<HeatJointMask> masks_;
  template <class A, class B>
  static bool same(const std::shared_ptr<A> &a, const std::shared_ptr<B> &b) {
    return a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a);
  }
  friend DeclaredWeightIntent
  declared_weight_intent(const SculptObject &, const DeclaredWeightPart &,
                         const ImportedHeatProfile &,
                         const HeatBindingOptions &, weight_surface::Budget &,
                         DeclaredSupportPolicy, const Mesh &,
                         std::shared_ptr<const DeformationState>);

public:
  std::span<const HeatJointMask> masks(const SculptObject &o) const {
    if (o.id != object_ || !same(paint_, o.painted) || !o.painted ||
        !same(mesh_, o.painted->mesh_owner()) ||
        o.painted->revision() != revision_ || !same(target_, o.deformation) ||
        !reference_ || !reference_->skeleton)
      throw std::logic_error(
          "Declared geometry/reference intent lease changed");
    return masks_;
  }
};
inline DeclaredWeightIntent
declared_weight_intent(const SculptObject &o, const DeclaredWeightPart &input,
                       const ImportedHeatProfile &profile,
                       const HeatBindingOptions &options,
                       weight_surface::Budget &work,
                       DeclaredSupportPolicy policy, const Mesh &metric_mesh,
                       std::shared_ptr<const DeformationState> reference) {
  if (input.object != o.id || !o.painted || !reference || !reference->skeleton)
    throw std::invalid_argument(
        "Declared geometry needs its genuine reference rig owner");
  const auto &m = metric_mesh;
  const auto &rig = *reference->skeleton;
  const size_t n = m.positions.size(), b = rig.bones.size();
  if (!n || n > 2'000'000 || m.indices.size() > 6'000'000 || !b || b > 256 ||
      profile.eligible.size() != b || input.declarations.size() > n ||
      n > options.maximum_bytes / 80)
    throw std::length_error(
        "Declared geometry intent dimensions/workspace exceeded");
  if (m.positions.size() != o.painted->mesh().positions.size() ||
      m.indices.size() != o.painted->mesh().indices.size())
    throw std::logic_error(
        "Worker metric vertex/corner correspondence changed");
  const size_t source_bytes =
      profile.segments.size() * sizeof(HeatSourceSegment) + 4096;
  if (source_bytes > options.maximum_bytes ||
      n > (options.maximum_bytes - source_bytes) / 80)
    throw std::length_error(
        "Declared geometry simultaneous mask/source workspace exceeded");
  size_t local_work = 0;
  auto parents = heat_intent_components(
      m, {}, local_work,
      std::min(options.maximum_work, work.maximum - work.used), work.cancel);
  work.step(local_work);
  std::vector<HeatJointMask> masks(n);
  if (input.declarations.empty()) {
    if (policy == DeclaredSupportPolicy::NearestSourceUnion ||
        policy == DeclaredSupportPolicy::UniqueSourceWitnesses) {
      NearestWeightSources sources(profile.segments, work);
      for (size_t i = 0; i < n; ++i) {
        const auto p = m.positions[i];
        auto selected = sources.nearest({p.x, p.y, p.z}, work);
        if (policy == DeclaredSupportPolicy::UniqueSourceWitnesses) {
          work.step(4);
          size_t count = 0;
          for (auto word : selected)
            count += std::popcount(word);
          if (count != 1)
            continue;
        }
        for (size_t k = 0; k < 4; ++k)
          masks[parents[i]][k] |= selected[k];
      }
    } else
      for (size_t i = 0; i < n; ++i)
        if (parents[i] == i)
          for (size_t j = 0; j < b; ++j) {
            work.step();
            if (profile.eligible[j])
              masks[i][j / 64] |= uint64_t(1) << (j % 64);
          }
  } else
    for (const auto &d : input.declarations) {
      work.step();
      if (d.vertex >= n || d.joints.empty() || d.joints.size() > b)
        throw std::invalid_argument(
            "Invalid explicit geometry component declaration");
      for (auto joint : d.joints) {
        work.step();
        if (joint >= b || !rig.bones[joint].deform || !profile.eligible[joint])
          throw std::invalid_argument(
              "Explicit geometry joint is outside authored deform domain");
        masks[parents[d.vertex]][joint / 64] |= uint64_t(1) << (joint % 64);
      }
    }
  for (size_t i = 0; i < n; ++i) {
    work.step();
    if (parents[i] == i && masks[i] == HeatJointMask{})
      throw std::invalid_argument(
          "Explicit geometry intent omitted a connected component");
  }
  DeclaredWeightIntent out;
  out.object_ = o.id;
  out.paint_ = o.painted;
  out.mesh_ = o.painted->mesh_owner();
  out.revision_ = o.painted->revision();
  out.target_ = o.deformation;
  out.reference_ = std::move(reference);
  out.masks_.resize(n);
  for (size_t i = 0; i < n; ++i) {
    work.step();
    out.masks_[i] = masks[parents[i]];
  }
  return out;
}
} // namespace dome::detail
