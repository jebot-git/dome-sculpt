// SPDX-License-Identifier: GPL-3.0-or-later
#include "anatomical-continuation.hpp"
#include "dome/skin-authoring.hpp"
#include "polygon-conversion.hpp"
#include <set>
namespace dome {
namespace {
using Work = detail::weight_surface::Budget;
struct Bytes {
  size_t used = 0, maximum;
  void add(size_t count, size_t unit = 1) {
    if (unit && count > (maximum - used) / unit)
      throw std::length_error(
          "Source-assisted bind workspace exceeded before staging");
    used += count * unit;
  }
  void text(const std::string &s) { add(s.size() + 1); }
};
void deformation_copy_bytes(const DeformationState &d, Bytes &b, Work &w) {
  if (d.pose.size() > 256 || d.morph_weights.size() > 128 ||
      (d.skeleton && d.skeleton->bones.size() > 256) ||
      (d.skin && d.skin->vertices.size() > 2'000'000) ||
      (d.morphs && d.morphs->targets.size() > 128))
    throw std::length_error(
        "Source-assisted deformation copy dimensions exceeded");
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
        throw std::length_error("Source-assisted row copy exceeded");
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
bool equal_row(const VertexWeights &a, const VertexWeights &b) {
  if (a.size() != b.size())
    return false;
  for (const auto &x : a)
    if (std::find(b.begin(), b.end(), x) == b.end())
      return false;
  return true;
}
} // namespace
void SourceVisibleSkinOptions::validate() const {
  if (!maximum_work || maximum_work > 3'000'000'000ULL ||
      maximum_workspace < 4096 || maximum_workspace > 512u * 1024u * 1024u)
    throw std::invalid_argument(
        "Invalid source-assisted bind work/workspace limits");
}
bool author_source_visible_skin(Scene &scene, std::span<const uint64_t> targets,
                                const SkinAuthoringOptions &options,
                                const std::function<bool()> &cancelled,
                                SourceVisibleSkinReport *report) {
  options.validate();
  if (options.mode != SkinAuthoringMode::SourceVisible || targets.empty() ||
      targets.size() > 64)
    throw std::invalid_argument("Choose explicit source-assisted bind targets");
  const auto &reference = scene.object(options.reference);
  if (!reference.group || !reference.deformation ||
      !reference.deformation->skeleton || !reference.appearance ||
      !reference.appearance->asset || !reference.appearance->original_bytes ||
      !reference.painted)
    throw std::invalid_argument(
        "Choose an authenticated imported source group/rig");
  Work work{0, options.source_visible.maximum_work, cancelled};
  work.stop();
  Bytes bytes{0, options.source_visible.maximum_workspace};
  bytes.add(
      65536); // bounded normalization scratch and container/control blocks
  bytes.add(scene.objects().size(), sizeof(SculptObject));
  for (const auto &o : scene.objects()) {
    work.step();
    bytes.text(o.name);
    bytes.text(o.material.name);
  }
  bytes.add(scene.groups().size(), sizeof(SceneGroup));
  for (const auto &g : scene.groups()) {
    work.step();
    bytes.text(g.name);
  }
  std::set<uint64_t> ids;
  std::vector<SculptObject> captured;
  bytes.add(targets.size(), sizeof(SculptObject));
  captured.reserve(targets.size());
  const bool all_joints_locked =
      !options.locks.empty() &&
      std::all_of(options.locks.begin(), options.locks.end(),
                  [](auto x) { return x != 0; });
  bool every_target_closed = !options.locks.empty();
  for (auto id : targets) {
    work.step();
    if (!ids.insert(id).second)
      throw std::invalid_argument("Duplicate source-assisted target");
    const auto &o = scene.object(id);
    if (o.group != reference.group || !o.visible || o.locked ||
        !o.mesh_active || !o.painted || !o.deformation ||
        !o.deformation->skeleton || !o.deformation->skin)
      throw std::invalid_argument(
          "Targets must be editable meshes in the exact imported source group");
    const auto &d = *o.deformation;
    auto same_owner = [](const auto &a, const auto &b) {
      return a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a);
    };
    if (!o.appearance ||
        !same_owner(o.appearance->asset, reference.appearance->asset) ||
        o.appearance->source_joint_nodes !=
            reference.appearance->source_joint_nodes)
      throw std::invalid_argument(
          "Targets require the exact same imported source/rig identity");
    bytes.text(o.name);
    bytes.text(o.material.name);
    if (!options.locks.empty() &&
        options.locks.size() != d.skeleton->bones.size())
      throw std::invalid_argument(
          "Locks require exact target joint identities");
    deformation_copy_bytes(d, bytes, work);
    const size_t raw = o.painted->mesh().positions.size();
    bytes.add(raw, sizeof(VertexWeights) + 4 * sizeof(Influence));
    // A first complete source capture copies paint values/metadata and retains
    // immutable geometry/assets. Count a conservative full group-placement
    // path.
    if (!o.rig_source) {
      bytes.add(sizeof(PolygonOrigin) + sizeof(Volume) +
                sizeof(CreatorAppearance) + 65536);
      bytes.text(o.name);
      bytes.text(o.material.name);
      bytes.add(o.painted->storage_bytes());
      if (o.volume)
        for (const auto &layer : o.volume->layers()) {
          work.step();
          bytes.add(sizeof(Volume::Layer) + 64);
          bytes.text(layer.name);
        }
      if (o.appearance) {
        const auto &a = *o.appearance;
        bytes.add(a.source_vertices.size() + a.source_joint_nodes.size() +
                      a.source_morph_targets.size(),
                  sizeof(uint32_t));
        bytes.text(a.material_json);
        bytes.text(a.mtoon_json);
      }
      bytes.add(scene.groups().size(), sizeof(SceneGroup) + 128);
      for (const auto &g : scene.groups())
        bytes.text(g.name);
    }
    detail::weight_surface::validate_rest_deformation(d, o.painted->mesh(),
                                                      work);
    if (!options.locks.empty())
      for (const auto &row : d.skin->vertices) {
        work.step(row.size() + 1);
        size_t count = 0;
        double sum = 0;
        bool has_free = false;
        for (auto x : row)
          if (options.locks.at(x.joint)) {
            sum += x.weight;
            if (x.weight > 0)
              ++count;
          } else if (x.weight > 0)
            has_free = true;
        if (count > 4 || sum > 1.000001)
          throw std::invalid_argument("Locked influences cannot satisfy the "
                                      "fixed4 source-assisted preset");
        every_target_closed &=
            !has_free && (all_joints_locked || sum >= 1 - 1e-6);
      }
    captured.push_back(o);
  }
  if (every_target_closed) {
    work.stop();
    if (report)
      *report = {work.used, bytes.used, 0, 0, 0, 0};
    return false;
  }
  work.step();
  if (work.used >= work.maximum || bytes.used >= bytes.maximum)
    throw std::length_error(
        "Source-assisted bind staging exhausted its bounds");
  auto fields = detail::anatomical_continuation(
      scene, reference.group, cancelled,
      detail::BoneWallDomain::ConnectedSourceSurface,
      {work.maximum - work.used, bytes.maximum - bytes.used});
  work.step(fields.report.work);
  const size_t planned = fields.report.bytes + bytes.used;
  std::vector<std::pair<uint64_t, std::shared_ptr<const DeformationState>>>
      changed;
  changed.reserve(captured.size());
  for (const auto &o : captured) {
    auto found =
        std::find_if(fields.parts.begin(), fields.parts.end(),
                     [&](const auto &p) { return p.object->id == o.id; });
    if (found == fields.parts.end() || !found->matches(scene))
      throw std::runtime_error("Source-assisted target lease changed");
    const auto bones = o.deformation->skeleton->bones.size();
    SkinWeights skin;
    skin.vertices.reserve(found->raw_to_row.size());
    bool differs = false;
    for (size_t raw = 0; raw < found->raw_to_row.size(); ++raw) {
      const size_t row = found->raw_to_row[raw], labels = fields.joints.size();
      // Conservative native sorting/set/normalization forecast, charged before
      // allocation/copies. No artist values are read except explicit locks.
      work.step(256 + 32 * labels * std::bit_width(labels) + bones);
      VertexWeights values;
      values.reserve(labels + options.locks.size());
      for (size_t j = 0; j < labels; ++j)
        if (fields.fields[row * labels + j] > 0 &&
            (options.locks.empty() || !options.locks[fields.joints[j]]))
          values.push_back(
              {fields.joints[j], float(fields.fields[row * labels + j])});
      if (!options.locks.empty())
        for (auto x : o.deformation->skin->vertices[raw])
          if (options.locks[x.joint])
            values.push_back(x);
      auto next = normalize_weights(std::move(values), bones, options.locks,
                                    SourceVisibleSkinOptions::output_limits);
      const auto &old = o.deformation->skin->vertices[raw];
      if (equal_row(next, old))
        skin.vertices.push_back(old);
      else {
        skin.vertices.push_back(std::move(next));
        differs = true;
      }
    }
    if (!differs)
      continue;
    auto next = std::make_shared<DeformationState>(*o.deformation);
    next->skin = std::move(skin);
    detail::weight_surface::validate_rest_deformation(*next, o.painted->mesh(),
                                                      work);
    changed.emplace_back(o.id, std::move(next));
  }
  work.stop();
  if (changed.empty()) {
    if (report)
      *report = {work.used,           planned,
                 fields.parts.size(), 0,
                 fields.report.rows,  fields.joints.size()};
    return false;
  }
  Scene staged = scene;
  for (const auto &[id, deform] : changed) {
    work.stop();
    auto origin = scene.object(id).rig_source;
    if (!origin)
      origin = capture_polygon_origin(scene, id);
    auto &target = staged.object(id);
    target.deformation = deform;
    target.rig_source = std::move(origin);
  }
  staged.validate();
  work.stop();
  SourceVisibleSkinReport done{work.used,           planned,
                               fields.parts.size(), changed.size(),
                               fields.report.rows,  fields.joints.size()};
  scene = std::move(staged);
  if (report)
    *report = done;
  return true;
}
} // namespace dome
