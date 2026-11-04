// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "geometry-weight-authoring.hpp"
namespace dome::detail {
// Original source identity is not a live asset lookup or source-instance token.
// Membership requires the actual current group and strong immutable owner.
struct NativeNodeContext {
  std::array<uint64_t, 64> ids{};
  size_t count = 0;
  void add(uint64_t id) {
    if (std::find(ids.begin(), ids.begin() + count, id) != ids.begin() + count)
      return;
    if (count == ids.size())
      throw std::length_error("Native support context exceeds64 parts");
    ids[count++] = id;
  }
};
inline NativeNodeContext
native_node_context(const Scene &scene, uint64_t reference,
                    std::span<const uint64_t> targets) {
  if (scene.objects().size() > 256 || scene.groups().size() > 128 ||
      targets.empty() || targets.size() > 64)
    throw std::length_error("Native support context dimensions exceeded");
  const auto &r = scene.object(reference);
  if (!r.group || !r.appearance || !r.appearance->asset ||
      r.appearance->source_node < 0 || r.appearance->source_mesh < 0)
    throw std::invalid_argument(
        "Choose a complete original reference mesh node");
  const auto &a = *r.appearance;
  const auto &model = *a.asset;
  if (size_t(a.source_node) >= model.nodes.size() ||
      size_t(a.source_mesh) >= model.meshes.size() ||
      model.nodes[a.source_node].mesh != a.source_mesh ||
      model.nodes[a.source_node].skin >= 0)
    throw std::invalid_argument(
        "Native support original node association changed");
  const auto &raw = model.meshes[a.source_mesh].primitives;
  if (raw.empty() || raw.size() > 64)
    throw std::length_error(
        "Native support original primitive count exceeds64");
  std::array<int, 64> expected{};
  std::array<uint64_t, 64> members{};
  size_t count = 0;
  for (auto index : raw) {
    if (index >= model.primitives.size())
      throw std::invalid_argument("Invalid original native primitive table");
    const auto &p = model.primitives[index];
    if (p.source_mesh != a.source_mesh || p.source_primitive < 0 ||
        !p.influences.empty() ||
        std::find(expected.begin(), expected.begin() + count,
                  p.source_primitive) != expected.begin() + count)
      throw std::invalid_argument(
          "Native support needs unique unskinned primitives");
    expected[count++] = p.source_primitive;
  }
  for (const auto &o : scene.objects()) {
    if (o.group != r.group || !o.appearance ||
        !geometry_weight_authoring::same_owner(o.appearance->asset, a.asset) ||
        o.appearance->source_node != a.source_node)
      continue;
    if (o.appearance->source_mesh != a.source_mesh || !o.mesh_active ||
        !o.painted || !o.volume || o.painted->in_stroke() ||
        o.volume->in_stroke())
      throw std::invalid_argument(
          "Native support current primitive is unavailable");
    const auto end = expected.begin() + count;
    const auto found =
        std::find(expected.begin(), end, o.appearance->source_primitive);
    if (found == end || members[size_t(found - expected.begin())])
      throw std::invalid_argument(
          "Native support primitive is foreign or duplicated");
    members[size_t(found - expected.begin())] = o.id;
  }
  NativeNodeContext result;
  for (size_t i = 0; i < count; ++i) {
    if (!members[i])
      throw std::invalid_argument("Native support original node is incomplete");
    result.add(members[i]);
  }
  for (auto id : targets) {
    (void)scene.object(id);
    result.add(id);
  }
  return result;
}
// Snapshot only metadata/COW owners; no geometry/texture clone or silent
// repair. All original scene inputs stay untouched until the final no-throw
// move.
inline bool author_native_context_skin(Scene &scene,
                                       std::span<const uint64_t> targets,
                                       const SkinAuthoringOptions &options,
                                       const std::function<bool()> &cancelled,
                                       NativeVisibleSkinReport *report) {
  using namespace geometry_weight_authoring;
  auto context = native_node_context(scene, options.reference, targets);
  std::array<uint64_t, 64> output_ids{};
  std::copy(targets.begin(), targets.end(), output_ids.begin());
  const auto outputs = std::span(output_ids.data(), targets.size());
  struct CowLease {
    std::shared_ptr<const void> mesh, volume;
    uint64_t paint_revision = 0, volume_revision = 0;
  };
  std::array<CowLease, 256> leases{};
  Bytes bytes{0, options.native_visible.maximum_workspace};
  bytes.add(sizeof(leases) + sizeof(NativeNodeContext) * 2 +
            sizeof(output_ids) + sizeof(SkinAuthoringOptions) +
            sizeof(DeclaredWeightSupport) + sizeof(GeometryWeightBindOptions) +
            2 * sizeof(Scene) + 4096);
  bytes.add(options.locks.size(), 2);
  bytes.add(context.count, sizeof(DeclaredWeightPart));
  bytes.add(scene.objects().size(), 2 * sizeof(SculptObject));
  bytes.add(scene.groups().size(), 2 * sizeof(SceneGroup));
  size_t wrapper_work = 4096 + scene.objects().size() * 1024 +
                        scene.groups().size() * 512 + context.count * 256 +
                        targets.size() * 16;
  for (const auto &o : scene.objects()) {
    if (!o.volume)
      throw std::invalid_argument("Native context input has no volume owner");
    bytes.text(o.name);
    bytes.text(o.name);
    bytes.text(o.material.name);
    bytes.text(o.material.name);
    if (o.name.size() > 1024 || o.material.name.size() > 1024)
      throw std::length_error("Native context metadata dimensions exceeded");
    wrapper_work += 4 * (o.name.size() + o.material.name.size());
  }
  for (const auto &g : scene.groups()) {
    bytes.text(g.name);
    bytes.text(g.name);
    if (g.name.size() > 1024)
      throw std::length_error(
          "Native context group metadata dimensions exceeded");
    wrapper_work += 4 * g.name.size();
    if (g.humanoid) {
      bytes.add(sizeof(HumanoidControls) + 64);
      bytes.text(g.humanoid->baseline);
      if (g.humanoid->baseline.size() > 1024)
        throw std::length_error("Native context creator metadata exceeded");
      wrapper_work += 4 * g.humanoid->baseline.size();
    }
  }
  if (wrapper_work >= options.native_visible.maximum_work ||
      bytes.used >= bytes.maximum || bytes.maximum - bytes.used < 4096)
    throw std::length_error(
        "Native support staging exceeds resources before copies");
  const SkinAuthoringOptions request = options;
  Scene before = scene;
  for (const auto &g : scene.groups())
    if (g.humanoid)
      before.group(g.id).humanoid =
          std::make_shared<const HumanoidControls>(*g.humanoid);
  for (size_t i = 0; i < scene.objects().size(); ++i) {
    const auto &o = scene.objects()[i];
    auto &l = leases[i];
    l.volume = o.volume->geometry_owner();
    l.volume_revision = o.volume->revision();
    if (o.painted) {
      l.mesh = o.painted->mesh_owner();
      l.paint_revision = o.painted->revision();
    }
  }
  const auto unchanged = [&] {
    if (before.active().id != scene.active().id)
      throw std::runtime_error("Native support active input changed");
    if (before.objects().size() != scene.objects().size() ||
        before.groups().size() != scene.groups().size())
      throw std::runtime_error("Native support scene membership changed");
    for (const auto &o : scene.objects())
      if (!o.volume)
        throw std::runtime_error("Native support volume owner was removed");
    require_isolated_scene(before, scene, 0);
    for (size_t i = 0; i < scene.groups().size(); ++i) {
      const auto &a = before.groups()[i], &b = scene.groups()[i];
      if (!same_owner(a.source_placement, b.source_placement) ||
          !same_owner(a.affine_edit, b.affine_edit))
        throw std::runtime_error(
            "Native support group placement owner changed");
    }
    for (size_t i = 0; i < scene.objects().size(); ++i) {
      const auto &a = before.objects()[i], &b = scene.objects()[i];
      const auto &l = leases[i];
      if (!same_owner(a.deformation, b.deformation) ||
          !same_owner(a.appearance, b.appearance) ||
          !same_owner(a.material_bindings, b.material_bindings) ||
          !same_owner(a.polygon, b.polygon) ||
          !same_owner(a.retopology, b.retopology) ||
          !same_owner(a.shape_source, b.shape_source) ||
          !same_owner(a.rig_source, b.rig_source) ||
          !same_owner(a.affine_edit, b.affine_edit) ||
          !same_owner(a.volume, b.volume) ||
          !same_owner(a.painted, b.painted) ||
          !same_owner(l.volume, b.volume->geometry_owner()) ||
          b.volume->revision() != l.volume_revision ||
          (b.painted && (!same_owner(l.mesh, b.painted->mesh_owner()) ||
                         b.painted->revision() != l.paint_revision)))
        throw std::runtime_error("Native support COW input changed");
    }
    const auto current = native_node_context(scene, request.reference, outputs);
    if (current.count != context.count || current.ids != context.ids)
      throw std::runtime_error("Native support primitive membership changed");
  };
  Scene staged = scene;
  DeclaredWeightSupport support;
  support.policy = DeclaredSupportPolicy::UniqueSourceWitnesses;
  support.world_metric = true;
  support.reference = request.reference;
  support.row_anchors = DeclaredRowAnchors::NearestPermitted;
  support.source_requirements = DeclaredSourceRequirements::GeometryEvidence;
  support.source_span = DeclaredSourceSpan::AuthoredFinite;
  support.origin_boundary =
      request.native_visible.contact_handling ==
              NativeContactHandling::FiniteLineIntegral
          ? DeclaredOriginBoundary::NativeFiniteLineIntegral
          : DeclaredOriginBoundary::NativeOpenSegment;
  support.parts.reserve(context.count);
  for (size_t i = 0; i < context.count; ++i)
    support.parts.push_back({context.ids[i], {}});
  GeometryWeightBindOptions bind;
  bind.limits = request.limits;
  bind.locks = request.locks;
  bind.maximum_work = request.native_visible.maximum_work - wrapper_work;
  bind.maximum_workspace =
      request.native_visible.maximum_workspace - bytes.used;
  GeometryWeightBindReport result;
  const bool changed = author_geometry_weight_skin(staged, outputs, support,
                                                   bind, cancelled, &result);
  weight_surface::Budget{0, 1, cancelled}.stop();
  unchanged();
  if (changed)
    scene = std::move(staged);
  if (report)
    *report = {
        result.work + wrapper_work,
        result.workspace + bytes.used,
        outputs.size(),
        result.changed,
        result.field.rows,
        size_t(std::count_if(
            before.object(request.reference)
                .deformation->skeleton->bones.begin(),
            before.object(request.reference).deformation->skeleton->bones.end(),
            [](const auto &b) { return b.deform; })),
        result.field.supported_controls,
        result.measure_zero_origins};
  return changed;
}
} // namespace dome::detail
