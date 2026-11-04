// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/skin-authoring.hpp"
#include "geometry-weight-authoring.hpp"
#include "native-weight-reference.hpp"
namespace dome {
void NativeVisibleSkinOptions::validate() const {
  if (!maximum_work || maximum_work > 500'000'000 || maximum_workspace < 4096 ||
      maximum_workspace > 512u * 1024u * 1024u)
    throw std::invalid_argument(
        "Invalid native visible-source work/workspace bounds");
}
bool author_native_visible_skin(Scene &scene, std::span<const uint64_t> targets,
                                const SkinAuthoringOptions &options,
                                const std::function<bool()> &cancelled,
                                NativeVisibleSkinReport *report) {
  options.validate();
  if (options.mode != SkinAuthoringMode::NativeVisible || targets.empty() ||
      targets.size() > 64 ||
      !detail::native_weight_reference(scene.object(options.reference)))
    throw std::invalid_argument(
        "Choose editable targets and a genuine native fitted reference rig");
  const auto &rig = *scene.object(options.reference).deformation->skeleton;
  const size_t permitted =
      std::count_if(rig.bones.begin(), rig.bones.end(),
                    [](const auto &b) { return b.deform; });
  const size_t wrapper_work =
      256 + targets.size() * 16 + options.locks.size() + rig.bones.size() * 4;
  const size_t wrapper_bytes =
      sizeof(detail::DeclaredWeightSupport) +
      sizeof(detail::GeometryWeightBindOptions) +
      targets.size() * sizeof(detail::DeclaredWeightPart) +
      options.locks.size() + 128;
  if (wrapper_work >= options.native_visible.maximum_work ||
      wrapper_bytes >= options.native_visible.maximum_workspace ||
      options.native_visible.maximum_workspace - wrapper_bytes < 4096)
    throw std::length_error(
        "Native bind input staging exceeds work/workspace before copies");
  detail::DeclaredWeightSupport support;
  support.policy = detail::DeclaredSupportPolicy::UniqueSourceWitnesses;
  support.world_metric = true;
  support.reference = options.reference;
  support.row_anchors = detail::DeclaredRowAnchors::NearestPermitted;
  support.source_requirements =
      detail::DeclaredSourceRequirements::GeometryEvidence;
  support.source_span = detail::DeclaredSourceSpan::AuthoredFinite;
  support.parts.reserve(targets.size());
  for (auto id : targets)
    support.parts.push_back({id, {}});
  detail::GeometryWeightBindOptions bind;
  bind.limits = options.limits;
  bind.locks = options.locks;
  bind.maximum_work = options.native_visible.maximum_work - wrapper_work;
  bind.maximum_workspace =
      options.native_visible.maximum_workspace - wrapper_bytes;
  detail::GeometryWeightBindReport result;
  const bool changed = detail::author_geometry_weight_skin(
      scene, targets, support, bind, cancelled, &result);
  if (report)
    *report = {result.work + wrapper_work,
               result.workspace + wrapper_bytes,
               targets.size(),
               result.changed,
               result.field.rows,
               permitted,
               result.field.supported_controls};
  return changed;
}
} // namespace dome
