// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bone-shadow-query.hpp"
#include "geometry-rig-correspondence.hpp"
#include "heat-component-intent.hpp"
#include "material-visibility-seams.hpp"
#include "native-unused-source.hpp"
#include "screened-continuation-solve.hpp"
#include "screened-sparse-ldl.hpp"
#include "skin-affine.hpp"
#include "weight-declared-support.hpp"
#include "weight-surface-system.hpp"
#include <map>
#include <nlohmann/json.hpp>
namespace dome::detail {
struct ContinuationPart {
  std::shared_ptr<const SculptObject> object;
  SculptObject source_lease;
  std::shared_ptr<const Mesh> source_mesh;
  uint64_t paint_revision = 0, volume_revision = 0;
  std::array<double, 16> world{};
  bool matches(const Scene &scene) const {
    const SculptObject *current = nullptr;
    for (const auto &o : scene.objects())
      if (o.id == source_lease.id) {
        current = &o;
        break;
      }
    if (!current || !current->painted || !current->volume)
      return false;
    const auto &a = source_lease, &b = *current;
    auto same = [](const auto &x, const auto &y) {
      return x.get() == y.get() && !x.owner_before(y) && !y.owner_before(x);
    };
    return a.id == b.id && a.group == b.group && a.name == b.name &&
           a.visible == b.visible && a.locked == b.locked &&
           a.mesh_active == b.mesh_active && same(a.painted, b.painted) &&
           same(source_mesh, b.painted->mesh_owner()) &&
           b.painted->revision() == paint_revision &&
           same(a.volume, b.volume) &&
           b.volume->revision() == volume_revision &&
           same(a.deformation, b.deformation) &&
           same(a.appearance, b.appearance) &&
           same(a.material_bindings, b.material_bindings) &&
           same(a.polygon, b.polygon) && same(a.retopology, b.retopology) &&
           same(a.shape_source, b.shape_source) &&
           same(a.rig_source, b.rig_source) &&
           same(a.affine_edit, b.affine_edit) &&
           scene.world_matrix(b.id).matrix() == world;
  }
  std::vector<uint32_t> raw_to_row;
};
struct ContinuationFields {
  std::vector<ContinuationPart> parts;
  std::vector<double> fields; // row-major, full certified field before limits
  std::vector<uint32_t> joints;
  ContinuationReport report;
  BoneQueryReport query_report;
  size_t visibility_seams = 0;
};
struct ContinuationLimits {
  size_t maximum_work = 500'000'000;
  size_t maximum_bytes = 512u * 1024u * 1024u;
};
namespace {
using namespace weight_surface;
// Default remains the original500M research guard. The separate opt-in native
// source-assisted method supplies its documented bounded limits explicitly.
inline ContinuationFields
anatomical_continuation(const Scene &scene, uint64_t group,
                        const std::function<bool()> &cancelled = {},
                        BoneWallDomain wall_domain = BoneWallDomain::WholeGroup,
                        ContinuationLimits limits = {},
                        const DeclaredWeightSupport *declared = nullptr) {
  if (!limits.maximum_work || limits.maximum_work > 3'000'000'000ULL ||
      limits.maximum_bytes < 4096 ||
      limits.maximum_bytes > 512u * 1024u * 1024u)
    throw std::invalid_argument("Invalid source-visible bind resource limits");
  HeatBindingOptions options;
  options.maximum_bytes = limits.maximum_bytes;
  options.maximum_work = std::min(limits.maximum_work, size_t(500'000'000));
  Budget budget{0, limits.maximum_work, cancelled};
  budget.stop();
  const char *stage = "source capture";
  try {
    ContinuationFields out;
    if (declared) {
      if ((declared->policy != DeclaredSupportPolicy::AllDeformControls &&
           declared->policy != DeclaredSupportPolicy::NearestSourceUnion &&
           declared->policy != DeclaredSupportPolicy::UniqueSourceWitnesses) ||
          (declared->rig_geometry != DeclaredRigGeometry::NativeBoneAxes &&
           declared->rig_geometry != DeclaredRigGeometry::ChildHeadControl) ||
          (declared->row_anchors != DeclaredRowAnchors::AllVisible &&
           declared->row_anchors != DeclaredRowAnchors::NearestPermitted) ||
          (declared->source_requirements !=
               DeclaredSourceRequirements::AllEligible &&
           declared->source_requirements !=
               DeclaredSourceRequirements::GeometryEvidence) ||
          (declared->source_span != DeclaredSourceSpan::ComponentProjection &&
           declared->source_span != DeclaredSourceSpan::AuthoredFinite) ||
          (declared->source_requirements ==
               DeclaredSourceRequirements::GeometryEvidence &&
           (declared->rig_geometry != DeclaredRigGeometry::NativeBoneAxes ||
            declared->row_anchors != DeclaredRowAnchors::NearestPermitted)))
        throw std::invalid_argument("Unknown declared geometry weight policy");
      if (declared->parts.empty() || declared->parts.size() > 64)
        throw std::length_error("Declared weight part dimensions exceeded");
      std::set<uint64_t> ids;
      for (const auto &p : declared->parts) {
        budget.step(p.declarations.size() + 1);
        if (!p.object || !ids.insert(p.object).second)
          throw std::invalid_argument("Duplicate declared weight object");
        (void)scene.object(p.object);
      }
    }
    size_t authority_workspace = 0;
    std::optional<ContinuationPart> reference_lease;
    if (declared && declared->reference) {
      if (!declared->world_metric)
        throw std::invalid_argument(
            "Explicit reference needs full actual world metric");
      const auto &r = scene.object(declared->reference);
      if (!r.painted || !r.volume || !r.deformation ||
          !r.deformation->skeleton || r.painted->in_stroke())
        throw std::invalid_argument(
            "Explicit geometry source needs a prepared genuine rig owner");
      validate_rest_deformation(*r.deformation, r.painted->mesh(), budget);
      const size_t owner_bytes =
          2 * (sizeof(SculptObject) + r.name.size() + r.material.name.size()) +
          4096;
      if (owner_bytes > limits.maximum_bytes)
        throw std::length_error(
            "Reference owner copy exceeds workspace before capture");
      budget.step_items(r.name.size() + r.material.name.size() + 1, 2);
      ContinuationPart lease;
      lease.object = std::make_shared<const SculptObject>(r);
      lease.source_lease = r;
      lease.source_mesh = r.painted->mesh_owner();
      lease.paint_revision = r.painted->revision();
      lease.volume_revision = r.volume->revision();
      lease.world = scene.world_matrix(r.id).matrix();
      reference_lease = std::move(lease);
    }
    std::vector<const SculptObject *> objects;
    size_t raw_count = 0, corners = 0;
    const SculptObject *first = nullptr;
    std::set<std::array<int, 3>> source_instances;
    for (const auto &o : scene.objects()) {
      budget.step();
      if (declared ? !declared->find(o.id) : o.group != group)
        continue;
      if (!o.painted || !o.volume || !o.mesh_active || source_is_dormant(o) ||
          o.painted->in_stroke() ||
          (!reference_lease && (!o.deformation || !o.deformation->skeleton)) ||
          (!declared &&
           (!o.deformation->skin || !o.deformation->humanoid || !o.appearance)))
        throw std::invalid_argument("Continuation group has unsupported owner");
      const auto &current_rig =
          reference_lease ? *reference_lease->object->deformation->skeleton
                          : *o.deformation->skeleton;
      if (current_rig.bones.empty() || current_rig.bones.size() > 256 ||
          o.painted->mesh().positions.size() > 2'000'000 - raw_count ||
          o.painted->mesh().indices.size() > 12'000'000 - corners)
        throw std::length_error("Continuation aggregate source bound");
      if (!declared &&
          !source_instances
               .insert({o.appearance->source_node, o.appearance->source_mesh,
                        o.appearance->source_primitive})
               .second)
        throw std::invalid_argument(
            "Continuation has duplicate original primitive instances");
      if (!first)
        first = &o;
      if (reference_lease) {
        const auto &r = *reference_lease->object;
        if (o.deformation && o.deformation->skeleton && o.id != r.id) {
          const auto current_world = scene.world_matrix(o.id);
          const auto change =
              current_world.inverse() * scene.world_matrix(r.id);
          if (!geometry_reference_rig_matches(
                  *r.deformation->skeleton, *o.deformation->skeleton, change,
                  current_world.matrix() == reference_lease->world, budget,
                  limits.maximum_bytes, &authority_workspace))
            throw std::invalid_argument("Existing target rig requires explicit "
                                        "exact reference correspondence");
          if (o.appearance && o.appearance->asset &&
              (!r.appearance || o.appearance->asset != r.appearance->asset ||
               o.appearance->asset.owner_before(r.appearance->asset) ||
               r.appearance->asset.owner_before(o.appearance->asset) ||
               o.appearance->source_joint_nodes !=
                   r.appearance->source_joint_nodes))
            throw std::invalid_argument(
                "Existing imported rig cannot borrow unrelated native "
                "reference identities");
        }
      } else {
        if ((!declared &&
             (o.appearance->asset.get() != first->appearance->asset.get() ||
              o.appearance->asset.owner_before(first->appearance->asset) ||
              first->appearance->asset.owner_before(o.appearance->asset) ||
              o.appearance->source_joint_nodes !=
                  first->appearance->source_joint_nodes ||
              o.deformation->humanoid->encode() !=
                  first->deformation->humanoid->encode())) ||
            o.deformation->skeleton->encode() !=
                first->deformation->skeleton->encode() ||
            (!(declared && declared->world_metric) &&
             scene.has_affine_placement(o.id)) ||
            scene.world_matrix(o.id).matrix() !=
                scene.world_matrix(first->id).matrix())
          throw std::invalid_argument(
              "Continuation current source/rig/metric changed");
      }
      // Solve in the common source REST frame. A shared native positive
      // similarity leaves angles, the mass/(distance^2+mass) kernel and
      // normalized output invariant. Pose/morph are retained display state,
      // never substituted for REST geometry. The separate explicit reference
      // path stages each genuine REST mesh in its actual world metric.
      if (!(declared && declared->world_metric) &&
          !(scene.world_transform(o.id).scale > 0))
        throw std::invalid_argument(
            "Continuation requires positive similarity placement");
      if (o.deformation)
        validate_rest_deformation(*o.deformation, o.painted->mesh(), budget);
      else {
        budget.step_items(o.painted->mesh().positions.size() +
                              o.painted->mesh().indices.size() + 1,
                          32);
        o.painted->mesh().validate();
      }
      budget.step(current_rig.bones.size() * current_rig.bones.size());
      objects.push_back(&o);
      raw_count += o.painted->mesh().positions.size();
      corners += o.painted->mesh().indices.size();
    }
    if (!first || raw_count > 2'000'000 || corners > 12'000'000)
      throw std::length_error("Continuation source bounds");
    const auto *rig_source =
        reference_lease ? reference_lease->object.get() : first;
    const size_t bones = rig_source->deformation->skeleton->bones.size();
    const bool world_metric = declared && declared->world_metric;
    AffineTransform metric, metric_shift;
    size_t metric_bytes = 0;
    if (world_metric) {
      const auto world = scene.world_matrix(rig_source->id);
      AffineInteractionFrame checked(world);
      (void)checked;
      auto origin = AffineTransform{}.matrix();
      for (size_t k = 0; k < 3; ++k)
        origin[12 + k] = -world.matrix()[12 + k];
      metric_shift = AffineTransform(origin);
      metric = metric_shift * world;
      for (const auto *o : objects) {
        const auto &m = o->painted->mesh();
        metric_bytes += sizeof(Mesh) +
                        (m.positions.size() + m.normals.size()) * sizeof(Vec3) +
                        m.uv.size() * sizeof(std::array<float, 2>) +
                        m.indices.size() * sizeof(uint32_t) + m.uv_pins.size() +
                        m.uv_seams.size();
      }
    }
    // Simultaneous systems, maps, graph/BVH, dense RHS/fields and solve
    // scratch.
    size_t capture_bytes = 0;
    for (const auto *o : objects) {
      const size_t add = 3 * (sizeof(SculptObject) + o->name.size() +
                              o->material.name.size()) +
                         o->painted->storage_bytes();
      if (add > options.maximum_bytes - capture_bytes)
        throw std::length_error("Continuation snapshot bytes");
      capture_bytes += add;
    }
    if (reference_lease)
      capture_bytes +=
          2 * (sizeof(SculptObject) + reference_lease->object->name.size() +
               reference_lease->object->material.name.size()) +
          4096;
    const bool nearest_rows =
        declared &&
        declared->row_anchors == DeclaredRowAnchors::NearestPermitted;
    const size_t bytes =
        capture_bytes +
        raw_count * (768 + 16 * bones + (nearest_rows ? 32 : 0)) +
        corners * 320 + 65536 + metric_bytes;
    if (bytes > options.maximum_bytes)
      throw std::length_error("Continuation memory bound before construction");
    out.report.bytes = bytes;
    // Own detached paint objects and immutable REST geometry before any
    // cancellation point can invalidate the caller's object vector. Material,
    // source capsules and const rig owners remain strong, never raw lifetimes.
    std::vector<SculptObject> leased_objects;
    leased_objects.reserve(objects.size());
    for (const auto *o : objects)
      leased_objects.push_back(*o);
    objects.clear();
    for (const auto &o : leased_objects)
      objects.push_back(&o);
    std::vector<std::shared_ptr<const SculptObject>> captured;
    captured.reserve(objects.size());
    for (const auto *o : objects) {
      budget.step(o->painted->layers().size() + 1);
      auto copy = std::make_shared<SculptObject>(*o);
      copy->painted = o->painted->snapshot();
      captured.push_back(std::move(copy));
    }
    first = captured.front().get();
    rig_source = reference_lease ? reference_lease->object.get() : first;
    budget.step();
    options.maximum_work =
        std::min(size_t(500'000'000), budget.maximum - budget.used);
    if (!options.maximum_work)
      throw std::length_error("Source-visible work exhausted before capture");
    if (declared &&
        declared->rig_geometry == DeclaredRigGeometry::NativeBoneAxes &&
        rig_source->appearance) {
      const auto &appearance = *rig_source->appearance;
      if (!appearance.source_joint_nodes.empty())
        throw std::invalid_argument(
            "Imported rig needs source-authenticated handle geometry; raw bone "
            "axes are not native handles");
      if (appearance.asset) {
        if (appearance.source_node < 0 ||
            size_t(appearance.source_node) >= appearance.asset->nodes.size())
          throw std::invalid_argument(
              "Native handle reference has invalid original node authority");
        if (appearance.asset->nodes[size_t(appearance.source_node)].skin >= 0)
          throw std::invalid_argument(
              "Original skinned source cannot be silently reinterpreted as "
              "native bone axes");
      }
    }
    auto profile =
        declared ? declared_weight_profile(*rig_source->deformation->skeleton,
                                           budget, declared->rig_geometry)
                 : imported_heat_profile(*first->deformation->skeleton,
                                         *first->appearance, options, cancelled,
                                         &*first->deformation->humanoid);
    budget.step(profile.work);
    if (world_metric) {
      budget.step_items(profile.segments.size(), 128);
      for (auto &source : profile.segments) {
        auto transform = [&](std::array<double, 3> p) {
          const auto q = skin_affine_point(
              metric, {float(p[0]), float(p[1]), float(p[2])});
          return std::array<double, 3>{q.x, q.y, q.z};
        };
        source.head = transform(source.head);
        source.tail = transform(source.tail);
      }
    }
    for (uint32_t j = 0; j < bones; ++j)
      if (profile.eligible[j])
        out.joints.push_back(j);
    const size_t labels = out.joints.size();
    std::vector<size_t> joint_label(bones, SIZE_MAX);
    for (size_t j = 0; j < labels; ++j)
      joint_label[out.joints[j]] = j;
    std::vector<HeatJointMask> usage;
    std::vector<uint32_t> row_owner, face_owner, support_root;
    struct EdgeLess {
      Budget *budget;
      bool operator()(const std::array<uint32_t, 2> &a,
                      const std::array<uint32_t, 2> &b) const {
        budget->step();
        return a < b;
      }
    };
    std::map<std::array<uint32_t, 2>, double, EdgeLess> edges(
        EdgeLess{&budget});
    System s;
    Mesh mesh;
    stage = "part geometry assembly";
    for (size_t owner = 0; owner < objects.size(); ++owner) {
      const auto &o = *captured[owner];
      budget.step();
      options.maximum_work =
          std::min(size_t(500'000'000), budget.maximum - budget.used);
      if (!options.maximum_work)
        throw std::length_error("Source-visible work exhausted before capture");
      std::optional<Mesh> staged_metric;
      if (world_metric) {
        const auto &m = o.painted->mesh();
        budget.step_items(m.positions.size() + m.indices.size() + m.uv.size() +
                              m.uv_pins.size() + m.uv_seams.size() + 1,
                          64);
        SkinGeometryWork geometry(budget.maximum - budget.used, cancelled);
        const auto remaining = geometry.remaining();
        staged_metric = skin_world_mesh(
            m, metric_shift * scene.world_matrix(o.id), geometry);
        budget.step(remaining - geometry.remaining());
      }
      const auto &solve_mesh =
          staged_metric ? *staged_metric : o.painted->mesh();
      std::optional<HeatSeamGraph> seams;
      std::optional<HeatComponentIntent> intent;
      std::optional<DeclaredWeightIntent> declared_intent;
      std::span<const uint32_t> row_groups;
      if (declared) {
        declared_intent = declared_weight_intent(
            o, *declared->find(o.id), profile, options, budget,
            declared->policy, solve_mesh, rig_source->deformation);
      } else {
        seams = capture_heat_seams(o, options, cancelled);
        budget.step(seams->work());
        options.maximum_work =
            std::min(size_t(500'000'000), budget.maximum - budget.used);
        if (!options.maximum_work)
          throw std::length_error(
              "Source-visible work exhausted before capture");
        intent =
            capture_imported_component_intent(o, *seams, options, cancelled);
        budget.step(intent->work());
        row_groups = seams->groups(o);
      }
      auto part =
          assemble(solve_mesh, budget, HeatOperator::PositiveGraph, row_groups,
                   HeatNormals::UnitFaces, HeatMass::MixedVoronoi);
      const uint32_t offset = s.positions.size();
      ContinuationPart p;
      p.object = captured[owner];
      p.source_lease = *objects[owner];
      p.source_mesh = objects[owner]->painted->mesh_owner();
      p.paint_revision = objects[owner]->painted->revision();
      p.volume_revision = objects[owner]->volume->revision();
      p.world = scene.world_matrix(o.id).matrix();
      p.raw_to_row = part.raw_to_row;
      auto masks = declared ? declared_intent->masks(o) : intent->masks(o);
      usage.resize(offset + part.positions.size());
      for (size_t raw = 0; raw < p.raw_to_row.size(); ++raw) {
        budget.step(4);
        p.raw_to_row[raw] += offset;
        usage[p.raw_to_row[raw]] = masks[raw];
      }
      out.parts.push_back(std::move(p));
      for (uint32_t i = 0; i < part.positions.size(); ++i) {
        budget.step(4);
        s.positions.push_back(part.positions[i]);
        s.normals.push_back(part.normals[i]);
        s.mass.push_back(part.mass[i]);
        row_owner.push_back(owner);
        support_root.push_back(offset + part.surface_parent[i]);
        mesh.positions.push_back(vec(part.positions[i]));
        mesh.normals.push_back(vec(part.normals[i]));
        for (size_t at = part.starts[i]; at < part.starts[i + 1]; ++at) {
          budget.step();
          if (part.columns[at] > i)
            edges[{offset + i, offset + part.columns[at]}] = -part.values[at];
        }
      }
      for (auto index : solve_mesh.indices) {
        budget.step();
        mesh.indices.push_back(out.parts.back().raw_to_row[index]);
      }
      face_owner.insert(face_owner.end(), solve_mesh.indices.size() / 3, owner);
    }
    const size_t n = s.positions.size();
    out.report.rows = n;
    double length = 0;
    for (const auto &[edge, w] : edges) {
      budget.step(16);
      (void)w;
      length += norm(sub(s.positions[edge[0]], s.positions[edge[1]]));
    }
    const double h = length / edges.size();
    if (!(h > 0) || !std::isfinite(h))
      throw std::runtime_error("Continuation invalid scale");
    std::vector<D> face_normals(mesh.indices.size() / 3);
    for (size_t face = 0; face < face_normals.size(); ++face) {
      budget.step(16);
      auto a = s.positions[mesh.indices[face * 3]],
           b = s.positions[mesh.indices[face * 3 + 1]],
           c = s.positions[mesh.indices[face * 3 + 2]];
      face_normals[face] = cross(sub(b, a), sub(c, a));
    }
    SurfaceQuery query(mesh, [&] {
      budget.step();
      return false;
    });
    // Explicit local co-oriented cross-part support edges. These are geometric
    // contact springs across distinct immutable owners, not topology welding.
    stage = "contact support construction";
    for (uint32_t i = 0; i < n; ++i) {
      budget.stop();
      SurfaceQueryOptions qo;
      qo.closest_maximum = float(h);
      qo.cancelled = [&] {
        budget.step();
        return false;
      };
      qo.triangle_filter = [&](uint32_t face) {
        budget.step(8);
        if (face_owner[face] == row_owner[i])
          return false;
        size_t c = size_t(face) * 3;
        bool common = false;
        for (size_t word = 0; word < 4; ++word)
          common |= (usage[i][word] & usage[mesh.indices[c]][word]) != 0;
        if (!common)
          return false;
        auto a = s.positions[mesh.indices[c]],
             b = s.positions[mesh.indices[c + 1]],
             d = s.positions[mesh.indices[c + 2]];
        budget.step(24);
        return dot(cross(sub(b, a), sub(d, a)), s.normals[i]) > 0;
      };
      auto hit = query.nearest(vec(s.positions[i]), qo);
      if (!hit)
        continue;
      double k =
          s.mass[i] / (double(hit->distance) * hit->distance + s.mass[i]);
      for (size_t c = 0; c < 3; ++c) {
        budget.step(8);
        auto j = mesh.indices[size_t(hit->triangle) * 3 + c];
        double w = k * hit->barycentric[c];
        if (w > 0) {
          edges[{std::min(i, j), std::max(i, j)}] += w;
          ++out.report.contacts;
        }
      }
    }
    std::vector<double> rhs(n * labels), screen(n);
    stage = "owning analytic shadow query";
    std::vector<bone_shadow::Point> query_points;
    query_points.reserve(n);
    for (auto p : s.positions) {
      budget.step(3);
      query_points.push_back({float(p[0]), float(p[1]), float(p[2])});
    }
    MaterialVisibilitySeams incidence;
    // A local-source wall domain never queries other source components. Do
    // not build a cross-material incidence certificate that it cannot use.
    // Whole-group control retains the complete canonical-owner proof.
    if (wall_domain == BoneWallDomain::WholeGroup)
      incidence = material_visibility_seams(
          std::span<const ContinuationPart>(out.parts),
          std::span<const bone_shadow::Point>(query_points),
          std::span<const uint32_t>(mesh.indices),
          std::span<const uint32_t>(row_owner), options.maximum_bytes - bytes,
          budget);
    out.visibility_seams = incidence.joined_edges;
    BoneShadowQuery shadow_query(query_points, mesh.indices,
                                 options.maximum_bytes - bytes -
                                     incidence.planned_bytes,
                                 budget, incidence.groups);
    BoneShadowQuery::Scratch shadow_scratch(shadow_query);
    const size_t source_bytes =
        bytes + incidence.planned_bytes + shadow_query.planned_bytes;
    out.report.bytes = source_bytes;
    stage = "analytic visible source fields";
    const size_t source_start_work = budget.used;
    std::vector<HeatJointMask> row_anchors;
    if (nearest_rows) {
      // Captured metric REST rows, not posed positions. Component/user intent
      // remains the allowed set; unrelated closest controls cannot override it.
      // Selection precedes visibility. An occluded nearest control does not
      // silently promote a farther source, and an unsupported source refuses.
      NearestWeightSources sources(profile.segments, budget);
      row_anchors.reserve(n);
      for (size_t i = 0; i < n; ++i) {
        budget.step(4);
        row_anchors.push_back(
            sources.nearest(query_points[i], usage[i], budget));
      }
    }
    std::vector<double> low(n), high(n);
    HeatJointMask required{};
    const bool geometry_requirements =
        declared && declared->source_requirements ==
                        DeclaredSourceRequirements::GeometryEvidence;
    if (geometry_requirements)
      for (auto mask : usage) {
        budget.step(4);
        for (size_t k = 0; k < 4; ++k)
          required[k] |= mask[k];
      }
    for (const auto &source : profile.segments) {
      if (geometry_requirements &&
          !(required[source.bone / 64] & (uint64_t(1) << (source.bone % 64)))) {
        // A permitted native rig control is not mandatory on every part.
        // Retain its exact zero field and the full Skeleton only after a
        // whole-facet certificate, never vertex-only absence.
        if (!certify_unused_native_source(source, profile.segments, mesh, usage,
                                          budget))
          throw std::runtime_error(
              "Unused native source lacks whole-facet dominance bone=" +
              std::to_string(source.bone));
        continue;
      }
      bone_shadow::Point head{}, tail{};
      for (size_t k = 0; k < 3; ++k) {
        head[k] = float(source.head[k]);
        tail[k] = float(source.tail[k]);
        if (double(head[k]) != source.head[k] ||
            double(tail[k]) != source.tail[k])
          throw std::invalid_argument(
              "Source requires exact captured binary32 positions");
      }
      const auto delta = sub(source.tail, source.head);
      const double physical = norm(delta);
      auto axis = delta;
      if (physical)
        for (double &v : axis)
          v /= physical;
      std::fill(low.begin(), low.end(), INFINITY);
      std::fill(high.begin(), high.end(), -INFINITY);
      const bool authored_span =
          declared &&
          declared->source_span == DeclaredSourceSpan::AuthoredFinite;
      if (!authored_span)
        for (uint32_t i = 0; i < n; ++i) {
          budget.step(16);
          if (!(usage[i][source.bone / 64] &
                (uint64_t(1) << (source.bone % 64))))
            continue;
          double t = dot(sub(s.positions[i], source.head), axis);
          low[support_root[i]] = std::min(low[support_root[i]], t);
          high[support_root[i]] = std::max(high[support_root[i]], t);
        }
      bool supported = false;
      for (uint32_t i = 0; i < n; ++i) {
        budget.step(4);
        if (!(usage[i][source.bone / 64] & (uint64_t(1) << (source.bone % 64))))
          continue;
        if (nearest_rows && !(row_anchors[i][source.bone / 64] &
                              (uint64_t(1) << (source.bone % 64))))
          continue;
        double lo = 0, hi = 1;
        if (physical && !authored_span) {
          lo = std::clamp(low[support_root[i]] / physical, 0., 1.);
          hi = std::clamp(high[support_root[i]] / physical, 0., 1.);
        }
        if (lo >= hi)
          continue;
        double conductance = 0;
        try {
          conductance = shadow_query.integrate(i, head, tail, s.mass[i], lo, hi,
                                               shadow_scratch, budget,
                                               out.query_report, wall_domain);
        } catch (const std::length_error &e) {
          throw std::length_error(
              std::string(e.what()) +
              " base_work=" + std::to_string(source_start_work) + " source=" +
              std::to_string(source.bone) + " row=" + std::to_string(i) +
              " completed_integrals=" + std::to_string(out.report.samples) +
              " candidates=" + std::to_string(out.query_report.candidates) +
              " exact=" + std::to_string(out.query_report.exact_predicates) +
              " row_hits=" + std::to_string(out.query_report.row_hits) +
              " source_hits=" + std::to_string(out.query_report.source_hits) +
              " search_work=" + std::to_string(out.query_report.search_work) +
              " union_work=" + std::to_string(out.query_report.union_work) +
              " integration_work=" +
              std::to_string(out.query_report.integration_work) +
              " predicate_work=" +
              std::to_string(out.query_report.predicate_work));
        } catch (const BoneQueryAmbiguity &e) {
          throw std::runtime_error(
              std::string(e.what()) +
              " base_work=" + std::to_string(source_start_work) +
              " source=" + std::to_string(source.bone) +
              " row=" + std::to_string(i) + " face=" + std::to_string(e.face) +
              " kind=" + std::to_string(int(e.kind)));
        }
        if (!std::isfinite(conductance) || conductance < 0)
          throw std::runtime_error("Invalid analytic source conductance");
        screen[i] += conductance;
        rhs[i * labels + joint_label[source.bone]] += conductance;
        supported |= conductance > 0;
        ++out.report.samples;
      }
      if (!supported)
        throw std::runtime_error(
            "Analytic source has no declared group support bone=" +
            std::to_string(source.bone));
      ++out.report.supported_controls;
    }
    stage = "screened matrix assembly";
    s.diagonal = screen;
    s.starts.assign(n + 1, 0);
    s.parent.resize(n);
    std::iota(s.parent.begin(), s.parent.end(), 0);
    auto root = [&](uint32_t i) {
      while (s.parent[i] != i) {
        budget.step();
        s.parent[i] = s.parent[s.parent[i]];
        i = s.parent[i];
      }
      return i;
    };
    for (auto [edge, w] : edges) {
      budget.step(8);
      if (!(w > 0) || !std::isfinite(w))
        throw std::runtime_error("Continuation nonpositive conductance");
      ++s.starts[edge[0] + 1];
      ++s.starts[edge[1] + 1];
      s.diagonal[edge[0]] += w;
      s.diagonal[edge[1]] += w;
      s.parent[root(edge[0])] = root(edge[1]);
    }
    std::vector<double> anchored(n);
    for (uint32_t i = 0; i < n; ++i) {
      budget.step(2);
      anchored[root(i)] += screen[i];
      s.starts[i + 1] += s.starts[i];
    }
    for (uint32_t i = 0; i < n; ++i) {
      budget.step();
      if (!(anchored[root(i)] > 0))
        throw std::runtime_error(
            "Continuation has an unseeded connected support domain");
    }
    s.columns.resize(s.starts.back());
    s.values.resize(s.starts.back());
    auto cursor = s.starts;
    for (auto [edge, w] : edges) {
      budget.step(4);
      auto a = cursor[edge[0]]++, b = cursor[edge[1]]++;
      s.columns[a] = edge[1];
      s.values[a] = -w;
      s.columns[b] = edge[0];
      s.values[b] = -w;
    }
    // Lexicographic edge iteration yields sorted unique adjacency rows.
    stage = "fixed screened factorization";
    ScreenedSparseLDL factor(s, options.maximum_bytes - source_bytes, budget);
    out.report.bytes = source_bytes + factor.planned_bytes;
    std::vector<uint8_t> free(n);
    struct FixedInverse {
      ScreenedSparseLDL &factor;
      std::vector<uint8_t> &free;
      void apply(std::span<const double> r, std::span<double> z, Budget &work) {
        factor.apply(r, z, free, work);
      }
    } ic{factor, free};
    out.fields.resize(n * labels);
    std::vector<double> b(n), x(n), sum(n);
    stage = "certified field solve";
    for (size_t label = 0; label < labels; ++label) {
      for (size_t i = 0; i < n; ++i) {
        budget.step();
        b[i] = rhs[i * labels + label];
      }
      if (geometry_requirements &&
          !(required[out.joints[label] / 64] &
            (uint64_t(1) << (out.joints[label] % 64)))) {
        // Explicit homogeneous system, certified unused above. Do not weaken
        // the required-control PCG guard or solve then normalize a fallback.
        budget.step(2 * n);
        for (double v : b)
          if (v != 0)
            throw std::logic_error("Unused source has a nonzero RHS");
        std::fill(x.begin(), x.end(), 0.);
        s.multiply(x, b, budget);
        for (double v : b)
          if (v != 0)
            throw std::logic_error("Unused source failed exact zero residual");
      } else
        continuation_solve(s, b, x, ic, budget, out.report);
      for (size_t i = 0; i < n; ++i) {
        budget.step(2);
        out.fields[i * labels + label] = x[i];
        sum[i] += x[i];
      }
    }
    for (double v : sum) {
      budget.step();
      out.report.partition = std::max(out.report.partition, std::abs(v - 1));
    }
    if (out.report.partition > 1e-9)
      throw std::runtime_error(
          "Continuation failed independent partition certificate");
    size_t current_count = 0;
    for (const auto &o : scene.objects())
      if (declared ? declared->find(o.id) != nullptr : o.group == group)
        ++current_count;
    if (current_count != out.parts.size())
      throw std::runtime_error("Captured source group membership changed");
    for (const auto &part : out.parts)
      if (!part.matches(scene))
        throw std::runtime_error("Captured weight source lease changed");
    if (reference_lease && !reference_lease->matches(scene))
      throw std::runtime_error(
          "Captured actual reference rig/world/REST owner changed");
    out.report.bytes = std::max(out.report.bytes, authority_workspace);
    out.report.work = budget.used;
    return out;
  } catch (const std::length_error &e) {
    throw std::length_error(std::string(stage) + ": " + e.what() +
                            " used=" + std::to_string(budget.used));
  }
}
} // namespace
} // namespace dome::detail
