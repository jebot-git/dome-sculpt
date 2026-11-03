// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bone-shadow-query.hpp"
#include "heat-component-intent.hpp"
#include "material-visibility-seams.hpp"
#include "screened-continuation-solve.hpp"
#include "screened-sparse-ldl.hpp"
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
namespace {
// This factory is included after the private native heat assembly TU; System
// and Budget below are the bounded research types, not public authoring ABI.
inline ContinuationFields anatomical_continuation(
    const Scene &scene, uint64_t group,
    const std::function<bool()> &cancelled = {},
    BoneWallDomain wall_domain = BoneWallDomain::WholeGroup) {
  HeatBindingOptions options;
  options.maximum_bytes = 512u * 1024u * 1024u;
  options.maximum_work = 500'000'000;
  Budget budget{0, options.maximum_work, cancelled};
  budget.stop();
  const char *stage = "source capture";
  try {
    ContinuationFields out;
    std::vector<const SculptObject *> objects;
    size_t raw_count = 0, corners = 0;
    const SculptObject *first = nullptr;
    std::set<std::array<int, 3>> source_instances;
    for (const auto &o : scene.objects()) {
      budget.step();
      if (o.group != group)
        continue;
      if (!o.painted || !o.mesh_active || source_is_dormant(o) ||
          o.painted->in_stroke() || !o.deformation ||
          !o.deformation->skeleton || !o.deformation->skin ||
          !o.deformation->humanoid || !o.appearance)
        throw std::invalid_argument("Continuation group has unsupported owner");
      if (o.deformation->skeleton->bones.empty() ||
          o.deformation->skeleton->bones.size() > 256 ||
          o.painted->mesh().positions.size() > 2'000'000 - raw_count ||
          o.painted->mesh().indices.size() > 12'000'000 - corners)
        throw std::length_error("Continuation aggregate source bound");
      if (!source_instances
               .insert({o.appearance->source_node, o.appearance->source_mesh,
                        o.appearance->source_primitive})
               .second)
        throw std::invalid_argument(
            "Continuation has duplicate original primitive instances");
      if (!first)
        first = &o;
      if (o.appearance->asset.get() != first->appearance->asset.get() ||
          o.appearance->asset.owner_before(first->appearance->asset) ||
          first->appearance->asset.owner_before(o.appearance->asset) ||
          o.appearance->source_joint_nodes !=
              first->appearance->source_joint_nodes ||
          o.deformation->skeleton->encode() !=
              first->deformation->skeleton->encode() ||
          o.deformation->humanoid->encode() !=
              first->deformation->humanoid->encode() ||
          !o.deformation->pose.empty() ||
          std::any_of(o.deformation->morph_weights.begin(),
                      o.deformation->morph_weights.end(),
                      [](float x) { return x != 0; }) ||
          scene.world_matrix(o.id).matrix() != AffineTransform{}.matrix())
        throw std::invalid_argument(
            "Continuation current source/rig/metric changed");
      budget.step(o.deformation->skeleton->bones.size() *
                  o.deformation->skeleton->bones.size());
      objects.push_back(&o);
      raw_count += o.painted->mesh().positions.size();
      corners += o.painted->mesh().indices.size();
    }
    if (!first || raw_count > 2'000'000 || corners > 12'000'000)
      throw std::length_error("Continuation source bounds");
    const size_t bones = first->deformation->skeleton->bones.size();
    // Simultaneous systems, maps, graph/BVH, dense RHS/fields and solve
    // scratch.
    size_t capture_bytes = 0;
    for (const auto *o : objects) {
      const size_t add = sizeof(SculptObject) + o->name.size() +
                         o->material.name.size() + o->painted->storage_bytes();
      if (add > options.maximum_bytes - capture_bytes)
        throw std::length_error("Continuation snapshot bytes");
      capture_bytes += add;
    }
    const size_t bytes =
        capture_bytes + raw_count * (768 + 16 * bones) + corners * 320 + 65536;
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
    auto profile = imported_heat_profile(*first->deformation->skeleton,
                                         *first->appearance, options, cancelled,
                                         &*first->deformation->humanoid);
    budget.step(profile.work);
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
      auto seams = capture_heat_seams(o, options, cancelled);
      auto intent =
          capture_imported_component_intent(o, seams, options, cancelled);
      budget.step(seams.work() + intent.work());
      auto part = assemble(o.painted->mesh(), budget,
                           HeatOperator::PositiveGraph, seams.groups(o),
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
      auto masks = intent.masks(o);
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
      for (auto index : o.painted->mesh().indices) {
        budget.step();
        mesh.indices.push_back(out.parts.back().raw_to_row[index]);
      }
      face_owner.insert(face_owner.end(), o.painted->mesh().indices.size() / 3,
                        owner);
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
    std::vector<double> low(n), high(n);
    for (const auto &source : profile.segments) {
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
      for (uint32_t i = 0; i < n; ++i) {
        budget.step(16);
        if (!(usage[i][source.bone / 64] & (uint64_t(1) << (source.bone % 64))))
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
        double lo = 0, hi = 1;
        if (physical) {
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
      if (o.group == group)
        ++current_count;
    if (current_count != out.parts.size())
      throw std::runtime_error("Captured source group membership changed");
    for (const auto &part : out.parts)
      if (!part.matches(scene))
        throw std::runtime_error("Captured weight source lease changed");
    out.report.work = budget.used;
    return out;
  } catch (const std::length_error &e) {
    throw std::length_error(std::string(stage) + ": " + e.what() +
                            " used=" + std::to_string(budget.used));
  }
}
} // namespace
} // namespace dome::detail
