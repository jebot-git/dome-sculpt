// SPDX-License-Identifier: GPL-3.0-or-later
#include "cgltf.h"
#include "document-authority.hpp"
#include "dome/document.hpp"
#include "dome/rig-rest.hpp"
#include "geometry-weight-authoring.hpp"
#include <fstream>
#define main original_u_field_main
#include "native-weight-open-stress.cpp"
#undef main
namespace native_consumer {
size_t checks = 0;
void ensure(bool v, const char *why) {
  ++checks;
  if (!v)
    throw std::runtime_error(why);
}
using P = std::array<double, 3>;
P point(const float *m, P p) {
  return {m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12],
          m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
          m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]};
}
P parsed(const cgltf_node &node, const cgltf_primitive &p, size_t v) {
  auto positions = cgltf_find_accessor(&p, cgltf_attribute_type_position, 0);
  float base[3]{};
  ensure(positions && cgltf_accessor_read_float(positions, v, base, 3),
         "Independent POSITION read");
  P value{base[0], base[1], base[2]};
  for (size_t t = 0; t < p.targets_count; ++t) {
    float w = node.weights_count > t         ? node.weights[t]
              : node.mesh->weights_count > t ? node.mesh->weights[t]
                                             : 0;
    for (size_t a = 0; a < p.targets[t].attributes_count; ++a)
      if (p.targets[t].attributes[a].type == cgltf_attribute_type_position) {
        float delta[3]{};
        ensure(cgltf_accessor_read_float(p.targets[t].attributes[a].data, v,
                                         delta, 3),
               "Independent morph POSITION read");
        for (size_t c = 0; c < 3; ++c)
          value[c] += double(w) * delta[c];
      }
  }
  ensure(node.skin && node.skin->inverse_bind_matrices,
         "Actual skin/inverse-bind authority retained");
  P result{};
  size_t influences = 0;
  for (int set = 0; set < 8; ++set) {
    auto joints = cgltf_find_accessor(&p, cgltf_attribute_type_joints, set);
    auto weights = cgltf_find_accessor(&p, cgltf_attribute_type_weights, set);
    if (!joints && !weights)
      break;
    ensure(joints && weights, "Paired joint/weight sets");
    unsigned j[4]{};
    float w[4]{};
    ensure(cgltf_accessor_read_uint(joints, v, j, 4) &&
               cgltf_accessor_read_float(weights, v, w, 4),
           "Independent weight reads");
    for (size_t k = 0; k < 4; ++k)
      if (w[k]) {
        ensure(j[k] < node.skin->joints_count,
               "Independent joint index in range");
        float bind[16]{}, world[16]{};
        ensure(cgltf_accessor_read_float(node.skin->inverse_bind_matrices, j[k],
                                         bind, 16),
               "Independent inverse bind read");
        cgltf_node_transform_world(node.skin->joints[j[k]], world);
        const auto transformed = point(world, point(bind, value));
        for (size_t c = 0; c < 3; ++c)
          result[c] += double(w[k]) * transformed[c];
        ++influences;
      }
  }
  ensure(influences > 0 && influences <= 4,
         "Emitted source method keeps fixed4 nonzero influences");
  return result;
}
} // namespace native_consumer

int main(int argc, char **argv) try {
  using native_consumer::ensure;
  ensure(argc == 2, "Choose actual native row workflow output directory");
  const std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out / "state");
  std::filesystem::create_directories(out / "config");
  setenv("XDG_STATE_HOME", (out / "state").c_str(), 1);
  setenv("XDG_CONFIG_HOME", (out / "config").c_str(), 1);
  auto source = fixture(1, false);
  auto reference = *source.active().deformation;
  for (auto b : reference.skeleton->bones)
    reference.pose.push_back(b.rest);
  reference.pose[0].rotation = {.99500417f, 0, 0, .09983342f};
  auto &target = source.active();
  const auto &m = target.painted->mesh();
  DeformationState original(m);
  original.morphs = MorphSet(m);
  auto altered = m.positions;
  for (auto &p : altered)
    if (p.y > .75f)
      p.z += .003f;
  original.morphs->set_target(m, "Original unskinned authored tip", altered);
  original.morph_weights = {.35f};
  source.set_deformation(1, original);
  target.material.name = "Original actual editable paint";
  target.material.metallic = .27f;
  target.material.roughness = .63f;
  PaintSettings settings;
  settings.color = {.2f, .7f, .1f, .6f};
  Brush brush;
  brush.from = brush.to = {-.2f, .8f, 0};
  brush.normal = {0, 0, 1};
  brush.radius = .1f;
  target.painted->begin_stroke();
  target.painted->stamp(brush, settings);
  ensure(target.painted->end_stroke(),
         "Real original RGBA paint sample created");
  target.volume->add_layer("Retained original hidden authoring");
  target.volume->layer_properties(1, "Retained original hidden authoring",
                                  false, false, .6f);
  target.volume->fill_mask(.3f);
  target.shape_source = capture_polygon_origin(source, 1);
  const auto reference_id =
      source.add("Explicit native reference rig", Volume{});
  auto &r = source.object(reference_id);
  r.mesh_active = true;
  r.painted = source.object(1).painted->snapshot();
  r.visible = false;
  source.set_deformation(reference_id, reference);
  source.select(1);
  source.validate();
  const auto before = source.encode();
  detail::DeclaredWeightSupport domain{
      detail::DeclaredSupportPolicy::NearestSourceUnion,
      true,
      {{1, {}}},
      reference_id};
  domain.row_anchors = detail::DeclaredRowAnchors::NearestPermitted;
  detail::GeometryWeightBindOptions options;
  detail::GeometryWeightBindReport report;
  auto bound = source.snapshot();
  const auto paint = bound.object(1).painted;
  ensure(detail::author_geometry_weight_skin(bound, std::array<uint64_t, 1>{1},
                                             domain, options, {}, &report),
         "Actual unskinned open-U owning producer changes native skin");
  ensure(bound.object(1).painted == paint &&
             bound.object(1).deformation->morphs->encode() ==
                 original.morphs->encode() &&
             bound.object(1).deformation->morph_weights ==
                 original.morph_weights &&
             bound.object(1).deformation->pose.empty(),
         "Source REST/current morph values and paint stay authoritative");
  detail::require_isolated_scene(source, bound, 1);
  const auto accepted = bound.encode();
  auto restored = bound.snapshot();
  restore_rig_source(restored, 1, false);
  ensure(restored.encode() == before,
         "Complete original unskinned source restores exactly");
  auto noop = bound.snapshot();
  ensure(!detail::author_geometry_weight_skin(noop, std::array<uint64_t, 1>{1},
                                              domain, options) &&
             noop.encode() == accepted,
         "Second canonical row bind is a true no-op");
  auto cancelled = source.snapshot();
  auto held = cancelled.object(reference_id).deformation;
  const auto baseline = held.use_count();
  size_t entered = 0;
  bool refused = false;
  try {
    detail::author_geometry_weight_skin(cancelled, std::array<uint64_t, 1>{1},
                                        domain, options, [&] {
                                          if (held.use_count() >= baseline + 2)
                                            return ++entered == 200;
                                          return false;
                                        });
  } catch (const std::runtime_error &) {
    refused = true;
  }
  ensure(refused && entered == 200 && cancelled.encode() == before,
         "Actual mid-helper cancellation leaves every original owner/state "
         "unchanged");
  auto stale = source.snapshot();
  held = stale.object(reference_id).deformation;
  const auto stale_count = held.use_count();
  bool changed = false;
  refused = false;
  try {
    detail::author_geometry_weight_skin(
        stale, std::array<uint64_t, 1>{1}, domain, options, [&] {
          if (!changed && held.use_count() >= stale_count + 2) {
            auto d = *stale.object(reference_id).deformation;
            d.pose[0].translation.z += .01f;
            stale.set_deformation(reference_id, d);
            changed = true;
          }
          return false;
        });
  } catch (const std::runtime_error &) {
    refused = true;
  }
  auto expected_stale = source.snapshot();
  expected_stale.object(reference_id).deformation =
      stale.object(reference_id).deformation;
  ensure(
      changed && refused && stale.encode() == expected_stale.encode(),
      "Actual changed reference refuses stale field without publishing target");
  double max_quality = 0, mean_quality = 0, max_export = 0;
  size_t exported_corners = 0;
  for (size_t pose = 0; pose < 3; ++pose) {
    auto current = bound.snapshot();
    auto d = *current.object(1).deformation;
    for (auto b : d.skeleton->bones)
      d.pose.push_back(b.rest);
    if (pose == 0) {
      d.pose[0].translation.x -= .1f;
      d.pose[1].translation.x += .1f;
    } else
      for (size_t j = 0; j < 2; ++j) {
        float a = (j ? 1 : -1) * .15f;
        d.pose[j].rotation =
            pose == 1
                ? Quat{float(std::cos(a / 2)), 0, 0, float(std::sin(a / 2))}
                : Quat{float(std::cos(a / 2)), float(std::sin(a / 2)), 0, 0};
      }
    current.set_deformation(1, d);
    auto native = d.preview(m);
    double sum = 0, maximum = 0;
    size_t count = 0;
    std::vector<native_consumer::P> truth;
    for (size_t i = 0; i < m.positions.size(); ++i) {
      auto p =
          m.positions[i] + d.morphs->targets[0].delta[i] * d.morph_weights[0];
      const size_t own = p.x < 0 ? 0 : 1;
      const auto &head = d.skeleton->bones[own].rest.translation;
      Vec3 ideal;
      if (pose == 0) {
        ideal = p;
        ideal.x += own ? .1f : -.1f;
      } else {
        const double a = (own ? 1 : -1) * .15;
        auto local = p - head;
        ideal = p;
        if (pose == 1) {
          ideal.x =
              head.x + float(std::cos(a) * local.x - std::sin(a) * local.y);
          ideal.y =
              head.y + float(std::sin(a) * local.x + std::cos(a) * local.y);
        } else {
          ideal.y =
              head.y + float(std::cos(a) * local.y - std::sin(a) * local.z);
          ideal.z =
              head.z + float(std::sin(a) * local.y + std::cos(a) * local.z);
        }
      }
      if (p.y >= .5f) {
        const double e = length(native.positions[i] - ideal);
        maximum = std::max(maximum, e);
        sum += e;
        ++count;
      }
      truth.push_back({native.positions[i].x, native.positions[i].y,
                       native.positions[i].z});
    }
    ensure(maximum <= .006 && sum / count <= .002,
           "Original actual pose6mm/2mm quality limits hold");
    max_quality = std::max(max_quality, maximum);
    mean_quality = std::max(mean_quality, sum / count);
    auto surfaces = current.export_surfaces();
    std::erase_if(surfaces, [](const auto &s) { return s.id != 1; });
    ensure(surfaces.size() == 1, "Selected target output preserves reference "
                                 "separately in native project");
    auto file = out / ("pose-" + std::to_string(pose) + ".glb");
    export_glb(surfaces, file);
    cgltf_options opt{};
    cgltf_data *raw = nullptr;
    ensure(cgltf_parse_file(&opt, file.c_str(), &raw) == cgltf_result_success,
           "Independent GLB parse");
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw, cgltf_free);
    ensure(cgltf_load_buffers(&opt, raw, file.c_str()) ==
                   cgltf_result_success &&
               cgltf_validate(raw) == cgltf_result_success,
           "Independent complete emitted GLB structure validates");
    size_t found = 0;
    for (size_t n = 0; n < raw->nodes_count; ++n) {
      const auto &node = raw->nodes[n];
      if (!node.mesh)
        continue;
      ++found;
      size_t offset = 0;
      for (size_t pi = 0; pi < node.mesh->primitives_count; ++pi) {
        const auto &prim = node.mesh->primitives[pi];
        const auto pos =
            cgltf_find_accessor(&prim, cgltf_attribute_type_position, 0);
        const auto corners = prim.indices ? prim.indices->count : pos->count;
        ensure(offset + corners <= m.indices.size(),
               "Exact ordered current corner bound");
        for (size_t c = 0; c < corners; ++c) {
          const auto v =
              prim.indices ? cgltf_accessor_read_index(prim.indices, c) : c;
          auto parsed = native_consumer::parsed(node, prim, v),
               q = truth[m.indices[offset + c]];
          max_export = std::max(
              max_export,
              std::hypot(parsed[0] - q[0], parsed[1] - q[1], parsed[2] - q[2]));
          ++exported_corners;
        }
        offset += corners;
      }
      ensure(offset == m.indices.size(), "All original target corners emitted");
    }
    ensure(found == 1,
           "Actual target emitted once without invented source geometry");
  }
  ensure(max_export <= 3e-5,
         "Independent actual exported LBS matches current native morph/poses");
  Document doc;
  doc.replace(source);
  ensure(doc.wait_mesh(std::chrono::seconds(120)),
         "Initial immutable native mesh ready");
  doc.configure_object_grab(true, true);
  doc.hold_object(1);
  ensure(doc.wait_mesh(std::chrono::seconds(120)),
         "Native held output authority ready");
  const auto file = out / "native.dsculpt";
  doc.copy_scene().save(file);
  doc.edit_scene("Experimental nearest-row native bind",
                 [domain, options](Scene &s, const auto &cancel) {
                   (void)detail::author_geometry_weight_skin(
                       s, std::array<uint64_t, 1>{1}, domain, options, cancel);
                 });
  auto save = doc.save_async(file);
  save.get();
  ensure(doc.wait_asset() && doc.wait_mesh(std::chrono::seconds(120)) &&
             doc.frame().state->error.empty(),
         "Actual worker and immediate dependent Save succeed");
  ensure(doc.copy_scene().encode() == accepted &&
             Scene::load(file).encode() == accepted &&
             doc.frame().state->held_object == 1,
         "Held target/readonly reference/current result and saved accepted "
         "snapshot exact");
  ensure(doc.undo() && doc.copy_scene().encode() == before && doc.redo() &&
             doc.copy_scene().encode() == accepted,
         "Actual worker adds exactly one Undo/Redo and keeps original source");
  std::cout << "PASS row_native_checks=" << native_consumer::checks
            << " work=" << report.work << " bytes=" << report.workspace
            << " pose_max_m=" << max_quality << " pose_mean_m=" << mean_quality
            << " parsed_corners=" << exported_corners
            << " export_error=" << max_export << " public_menu=0\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
