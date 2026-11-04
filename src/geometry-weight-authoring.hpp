// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "anatomical-continuation.hpp"
#include "document-authority.hpp"
#include "polygon-conversion.hpp"
namespace dome::detail {
struct GeometryWeightBindOptions {
  WeightLimits limits{};
  std::vector<uint8_t> locks;
  size_t maximum_work = 500'000'000, maximum_workspace = 256u * 1024u * 1024u;
};
struct GeometryWeightBindReport {
  size_t work = 0, workspace = 0, changed = 0;
  ContinuationReport field;
  size_t measure_zero_origins = 0;
};
namespace geometry_weight_authoring {
using Work = weight_surface::Budget;
template <class A, class B>
bool same_owner(const std::shared_ptr<A> &a, const std::shared_ptr<B> &b) {
  return a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a);
}
struct Bytes {
  size_t used = 0, maximum;
  void add(size_t count, size_t unit = 1) {
    if (unit && count > (maximum - used) / unit)
      throw std::length_error(
          "Geometry bind workspace exceeded before staging");
    used += count * unit;
  }
  void text(const std::string &s) { add(s.size() + 1); }
};
inline void deformation_copy_bytes(const DeformationState &d, Bytes &b,
                                   Work &w) {
  if (d.pose.size() > 256 || d.morph_weights.size() > 128 ||
      (d.skeleton && d.skeleton->bones.size() > 256) ||
      (d.skin && d.skin->vertices.size() > 2'000'000) ||
      (d.morphs && d.morphs->targets.size() > 128))
    throw std::length_error(
        "Geometry bind deformation copy dimensions exceeded");
  b.add(sizeof(d));
  b.add(d.pose.size(), sizeof(JointTransform));
  b.add(d.morph_weights.size(), sizeof(float));
  w.step(d.pose.size() + d.morph_weights.size() + 1);
  if (d.skeleton) {
    const auto &s = *d.skeleton;
    b.add(s.bones.size(), sizeof(Bone));
    w.step(s.bones.size() + 1);
    for (const auto &bone : s.bones)
      b.text(bone.name);
    if (s.inverse_bind_override)
      b.add(s.inverse_bind_override->size(), sizeof(RigMatrix));
  }
  if (d.skin) {
    b.add(d.skin->vertices.size(), sizeof(VertexWeights));
    w.step(d.skin->vertices.size() + 1);
    for (const auto &row : d.skin->vertices) {
      if (row.size() > 32)
        throw std::length_error("Geometry bind row copy exceeded");
      w.step(row.size() + 1);
      b.add(row.size(), sizeof(Influence) + 64);
    }
  }
  if (d.morphs) {
    b.add(d.morphs->targets.size(), sizeof(MorphTarget));
    w.step(d.morphs->targets.size() + 1);
    for (const auto &target : d.morphs->targets) {
      w.step(target.delta.size() + 1);
      b.text(target.name);
      b.add(target.delta.size(), sizeof(Vec3));
    }
  }
  if (d.humanoid)
    for (const auto &[name, joint] : d.humanoid->joints) {
      (void)joint;
      w.step();
      b.add(128);
      b.text(name);
    }
  if (d.landmarks)
    for (const auto &[name, point] : d.landmarks->points) {
      (void)point;
      w.step();
      b.add(128);
      b.text(name);
    }
  if (d.expressions)
    for (const auto &[name, bindings] : d.expressions->visemes) {
      w.step(bindings.size() + 1);
      b.add(128);
      b.text(name);
      b.add(bindings.size(), sizeof(VisemeBinding) + 64);
      for (const auto &binding : bindings)
        b.text(binding.shape);
    }
}
inline bool equal_row(const VertexWeights &a, const VertexWeights &b) {
  if (a.size() != b.size())
    return false;
  for (const auto &x : a)
    if (std::find(b.begin(), b.end(), x) == b.end())
      return false;
  return true;
}
} // namespace geometry_weight_authoring
// Private prototype producer. Explicit reference/geometry domain; no UI/public
// method or imported-source eligibility is silently redirected here.
inline bool
author_geometry_weight_skin(Scene &scene, std::span<const uint64_t> targets,
                            const DeclaredWeightSupport &support,
                            const GeometryWeightBindOptions &options,
                            const std::function<bool()> &cancelled = {},
                            GeometryWeightBindReport *report = nullptr) {
  using namespace geometry_weight_authoring;
  if (targets.empty() || targets.size() > 64 || !support.reference ||
      !support.world_metric ||
      support.rig_geometry != DeclaredRigGeometry::NativeBoneAxes ||
      !options.maximum_work || options.maximum_work > 3'000'000'000ULL ||
      options.maximum_workspace < 4096 ||
      options.maximum_workspace > 512u * 1024u * 1024u ||
      !options.limits.max_influences || options.limits.max_influences > 32 ||
      !std::isfinite(options.limits.prune) || options.limits.prune < 0 ||
      options.limits.prune >= 1 || options.locks.size() > 256 ||
      std::any_of(options.locks.begin(), options.locks.end(),
                  [](auto x) { return x > 1; }))
    throw std::invalid_argument(
        "Invalid explicit native geometry weight options/domain");
  Work work{0, options.maximum_work, cancelled};
  work.stop();
  Bytes bytes{0, options.maximum_workspace};
  bytes.add(65536);
  bytes.add(scene.objects().size(), sizeof(SculptObject));
  bytes.add(scene.groups().size(), sizeof(SceneGroup));
  for (const auto &o : scene.objects()) {
    work.step();
    bytes.text(o.name);
    bytes.text(o.material.name);
  }
  for (const auto &g : scene.groups()) {
    work.step();
    bytes.text(g.name);
  }
  const auto &source_view = scene.object(support.reference);
  bytes.add(sizeof(SculptObject));
  bytes.text(source_view.name);
  bytes.text(source_view.material.name);
  const auto source = source_view;
  if (!source.deformation || !source.deformation->skeleton || !source.painted ||
      !source.volume || source.volume->in_stroke())
    throw std::invalid_argument(
        "Choose the complete current native reference rig");
  const auto source_world = scene.world_matrix(source.id);
  const auto source_mesh_owner = source.painted->mesh_owner();
  const auto source_volume_owner = source.volume->geometry_owner();
  const auto source_paint_revision = source.painted->revision();
  const auto source_volume_revision = source.volume->revision();
  auto require_reference = [&] {
    work.step_items(source.name.size() + source.material.name.size() + 1, 4);
    work.step(128);
    const auto &now = scene.object(source.id);
    if (!now.volume || !now.painted || !same_object_authority(source, now) ||
        !same_owner(source.painted, now.painted) ||
        !same_owner(source.volume, now.volume) ||
        !same_owner(source_mesh_owner, now.painted->mesh_owner()) ||
        !same_owner(source_volume_owner, now.volume->geometry_owner()) ||
        now.painted->revision() != source_paint_revision ||
        now.volume->revision() != source_volume_revision ||
        !same_owner(source.deformation, now.deformation) ||
        !same_owner(source.appearance, now.appearance) ||
        !same_owner(source.material_bindings, now.material_bindings) ||
        !same_owner(source.polygon, now.polygon) ||
        !same_owner(source.retopology, now.retopology) ||
        !same_owner(source.shape_source, now.shape_source) ||
        !same_owner(source.rig_source, now.rig_source) ||
        !same_owner(source.affine_edit, now.affine_edit) ||
        scene.world_matrix(source.id).matrix() != source_world.matrix())
      throw std::runtime_error(
          "Native reference source authority changed before adoption");
  };
  std::set<uint64_t> unique;
  for (auto id : targets) {
    work.step();
    const auto &o = scene.object(id);
    if (!unique.insert(id).second || !support.find(id) || !o.mesh_active ||
        !o.painted || !o.volume || !o.visible || o.locked ||
        o.painted->in_stroke() || o.volume->in_stroke())
      throw std::invalid_argument(
          "Choose explicit editable native geometry targets");
    if (o.deformation) {
      weight_surface::validate_rest_deformation(*o.deformation,
                                                o.painted->mesh(), work);
      deformation_copy_bytes(*o.deformation, bytes, work);
    } else {
      bytes.add(sizeof(DeformationState));
      work.step_items(o.painted->mesh().positions.size() +
                          o.painted->mesh().indices.size() + 1,
                      32);
    }
    const auto count = source.deformation->skeleton->bones.size();
    if (!options.locks.empty() &&
        (!o.deformation || !o.deformation->skeleton || !o.deformation->skin ||
         options.locks.size() != count))
      throw std::invalid_argument(
          "Geometry bind locks need actual existing joint/row identity");
    bytes.add(o.painted->mesh().positions.size(),
              sizeof(VertexWeights) +
                  options.limits.max_influences * sizeof(Influence) + 32);
    if (!o.deformation || !o.deformation->skeleton) {
      const auto &rig = *source.deformation->skeleton;
      bytes.add(rig.bones.size(), sizeof(Bone) + 64);
      work.step(rig.bones.size() + 1);
      for (const auto &bone : rig.bones)
        bytes.text(bone.name);
      if (rig.inverse_bind_override)
        bytes.add(rig.inverse_bind_override->size(), sizeof(RigMatrix));
      if (!o.deformation || !o.deformation->humanoid)
        if (source.deformation->humanoid)
          for (const auto &[name, joint] :
               source.deformation->humanoid->joints) {
            (void)joint;
            work.step();
            bytes.add(128);
            bytes.text(name);
          }
    }
    if (!o.rig_source) {
      bytes.add(sizeof(PolygonOrigin) + sizeof(PaintedMesh) + sizeof(Volume) +
                4096);
      bytes.add(o.painted->storage_bytes());
      for (const auto &layer : o.painted->layers()) {
        work.step();
        bytes.add(sizeof(PaintLayer) + 64);
        bytes.text(layer.name);
      }
      if (o.appearance) {
        const auto &a = *o.appearance;
        bytes.add(sizeof(CreatorAppearance));
        bytes.add(a.source_vertices.size() + a.source_joint_nodes.size() +
                      a.source_morph_targets.size(),
                  sizeof(uint32_t));
        bytes.text(a.material_json);
        bytes.text(a.mtoon_json);
      }
      for (const auto &layer : o.volume->layers()) {
        work.step();
        bytes.add(sizeof(Volume::Layer) + 64);
        bytes.text(layer.name);
      }
      uint64_t group = o.group;
      size_t depth = 0;
      while (group) {
        work.step();
        if (++depth > 32)
          throw std::length_error(
              "Geometry source placement path exceeds capture bound");
        const auto &g = scene.group(group);
        bytes.add(sizeof(SceneGroup) + sizeof(HumanoidControls) + 256);
        bytes.text(g.name);
        if (g.humanoid)
          bytes.text(g.humanoid->baseline);
        group = g.parent;
      }
    }
  }
  if (bytes.used >= bytes.maximum || work.used >= work.maximum)
    throw std::length_error("Geometry bind staging exhausted resources");
  auto fields = anatomical_continuation(
      scene, 0, cancelled, BoneWallDomain::ConnectedSourceSurface,
      {work.maximum - work.used, bytes.maximum - bytes.used}, &support);
  work.step(fields.report.work);
  const size_t planned = bytes.used + fields.report.bytes;
  std::vector<std::pair<uint64_t, std::shared_ptr<const DeformationState>>>
      changed;
  changed.reserve(targets.size());
  for (auto id : targets) {
    const auto &o = scene.object(id);
    auto part =
        std::find_if(fields.parts.begin(), fields.parts.end(),
                     [&](const auto &p) { return p.source_lease.id == id; });
    if (part == fields.parts.end() || !part->matches(scene))
      throw std::runtime_error("Geometry target source lease changed");
    SkinWeights skin;
    skin.vertices.reserve(part->raw_to_row.size());
    bool differs = !o.deformation || !o.deformation->skin;
    for (size_t raw = 0; raw < part->raw_to_row.size(); ++raw) {
      work.step(256 + 32 * fields.joints.size() *
                          std::bit_width(fields.joints.size() + 1));
      VertexWeights values;
      const auto row = part->raw_to_row[raw];
      for (size_t j = 0; j < fields.joints.size(); ++j)
        if (fields.fields[row * fields.joints.size() + j] > 0 &&
            (options.locks.empty() || !options.locks[fields.joints[j]]))
          values.push_back(
              {fields.joints[j],
               float(fields.fields[row * fields.joints.size() + j])});
      if (!options.locks.empty())
        for (auto x : o.deformation->skin->vertices[raw])
          if (options.locks[x.joint])
            values.push_back(x);
      auto next = normalize_weights(std::move(values),
                                    source.deformation->skeleton->bones.size(),
                                    options.locks, options.limits);
      if (o.deformation && o.deformation->skin &&
          equal_row(next, o.deformation->skin->vertices[raw]))
        skin.vertices.push_back(o.deformation->skin->vertices[raw]);
      else {
        skin.vertices.push_back(std::move(next));
        differs = true;
      }
    }
    if (!differs)
      continue;
    auto d = o.deformation
                 ? std::make_shared<DeformationState>(*o.deformation)
                 : std::make_shared<DeformationState>(o.painted->mesh());
    if (!d->skeleton) {
      const auto change = scene.world_matrix(id).inverse() * source_world;
      SkinGeometryWork placement(work.maximum - work.used, cancelled);
      const auto before = placement.remaining();
      d->skeleton =
          skin_reframe(*source.deformation->skeleton, change, placement);
      work.step(before - placement.remaining());
      // Existing target morph/pose and expression data remain untouched.
      if (!d->humanoid && source.deformation->humanoid)
        d->humanoid = source.deformation->humanoid;
    }
    d->skin = std::move(skin);
    weight_surface::validate_rest_deformation(*d, o.painted->mesh(), work);
    changed.emplace_back(id, std::move(d));
  }
  work.stop();
  require_reference();
  if (changed.empty()) {
    if (report)
      *report = {work.used, planned, 0, fields.report,
                 fields.query_report.measure_zero_origins};
    return false;
  }
  Scene staged = scene;
  for (const auto &[id, deform] : changed) {
    work.stop();
    auto origin = scene.object(id).rig_source;
    if (!origin)
      origin = capture_polygon_origin(scene, id);
    staged.object(id).deformation = deform;
    staged.object(id).rig_source = std::move(origin);
  }
  staged.validate();
  work.stop();
  for (const auto &part : fields.parts)
    if (!part.matches(scene))
      throw std::runtime_error("Geometry field source changed before adoption");
  require_reference();
  scene = std::move(staged);
  if (report)
    *report = {work.used, planned, changed.size(), fields.report,
               fields.query_report.measure_zero_origins};
  return true;
}
} // namespace dome::detail
