// SPDX-License-Identifier: GPL-3.0-or-later
#include "cgltf.h"
#include "document-authority.hpp"
#include "dome/document.hpp"
#include "dome/rig-rest.hpp"
#include "geometry-weight-authoring.hpp"
#include <fstream>
#define main reference_field_main
#include "geometry-weight-reference.cpp"
#undef main
namespace {
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
nlohmann::json inspect(const Scene &scene) {
  const auto &target = scene.object(2);
  return {{"scene", scene.encode()},
          {"name", target.name},
          {"rig_source", bool(target.rig_source)},
          {"bound", bool(target.deformation && target.deformation->skin)}};
}
Scene ui_fixture() {
  auto scene = fixture();
  Mesh world;
  constexpr unsigned nx = 12, ny = 80;
  for (unsigned y = 0; y <= ny; ++y)
    for (unsigned x = 0; x <= nx; ++x) {
      world.positions.push_back({.18f + .04f * x / nx, float(y) / ny, 0});
      world.normals.push_back({0, 0, 1});
      world.uv.push_back({float(x) / nx, float(y) / ny});
    }
  for (unsigned y = 0; y < ny; ++y)
    for (unsigned x = 0; x < nx; ++x) {
      const auto a = y * (nx + 1) + x, b = a + 1, d = a + nx + 1, c = d + 1;
      world.indices.insert(world.indices.end(), {a, b, c, a, c, d});
    }
  const std::function<bool()> never = [] { return false; };
  detail::SkinGeometryWork work(50000000, never);
  auto mesh =
      detail::skin_world_mesh(world, scene.world_matrix(2).inverse(), work);
  scene.object(2).painted = std::make_shared<PaintedMesh>(mesh, 64);
  DeformationState morph(mesh);
  morph.morphs = MorphSet(mesh);
  auto altered = mesh.positions;
  altered.front().z += .01f;
  morph.morphs->set_target(mesh, "Retained UI detail", altered);
  morph.morph_weights = {.35f};
  scene.set_deformation(2, morph);
  return scene;
}
} // namespace
int main(int argc, char **argv) try {
  if (argc == 3 && std::string_view(argv[1]) == "--ui-fixture") {
    ui_fixture().save(argv[2]);
    return 0;
  }
  if (argc == 3 && std::string_view(argv[1]) == "--inspect") {
    auto scene = Scene::load(argv[2]);
    std::cout << inspect(scene).dump() << '\n';
    return 0;
  }
  if ((argc == 3 || argc == 4) && std::string_view(argv[1]) == "--expected") {
    auto scene = Scene::load(argv[2]);
    SkinAuthoringOptions opts;
    opts.mode = SkinAuthoringMode::NativeVisible;
    opts.reference = 1;
    author_native_visible_skin(scene, std::array<uint64_t, 1>{2}, opts);
    if (argc == 4)
      scene.save(argv[3]);
    else
      std::cout << inspect(scene).dump() << '\n';
    return 0;
  }
  if (argc == 3 && std::string_view(argv[1]) == "--restore") {
    auto scene = Scene::load(argv[2]);
    restore_rig_source(scene, 2, false);
    std::cout << inspect(scene).dump() << '\n';
    return 0;
  }
  ensure(argc == 2, "Choose native authoring output directory");
  const std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out);
  std::filesystem::create_directories(out / "state");
  std::filesystem::create_directories(out / "config");
  setenv("XDG_STATE_HOME", (out / "state").c_str(), 1);
  setenv("XDG_CONFIG_HOME", (out / "config").c_str(), 1);
  auto source = fixture();
  const auto before = source.encode();
  SkinAuthoringOptions options;
  options.mode = SkinAuthoringMode::NativeVisible;
  options.reference = 1;
  auto bound = source.snapshot();
  const auto target_paint = bound.object(2).painted;
  const auto protected_before = bound.snapshot();
  NativeVisibleSkinReport report;
  ensure(author_native_visible_skin(bound, std::array<uint64_t, 1>{2}, options,
                                    {}, &report),
         "Public native-handle producer changes a genuinely unskinned target");
  ensure(bound.object(2).painted == target_paint &&
             bound.object(2).deformation->morphs->encode() ==
                 source.object(2).deformation->morphs->encode() &&
             bound.object(2).deformation->morph_weights ==
                 source.object(2).deformation->morph_weights &&
             bound.object(2).deformation->pose.empty() &&
             source.object(2).deformation->pose.empty(),
         "Target paint/geometry/authored morph/current values and original "
         "pose retained");
  std::ofstream(out / "source-before.json") << before["objects"][0].dump(2);
  std::ofstream(out / "source-after.json")
      << bound.encode()["objects"][0].dump(2);
  detail::require_isolated_scene(protected_before, bound, 2);
  auto source_context = bound;
  source_context.object(1) = source.object(1);
  ensure(source_context.encode() == bound.encode(),
         "Complete source rig/pose/material/paint remains exact in same owning "
         "Scene context");
  const auto accepted = bound.encode();
  ensure(
      !author_native_visible_skin(bound, std::array<uint64_t, 1>{2}, options) &&
          bound.encode() == accepted,
      "Exact canonical rig replay permits same-field no-op without "
      "overwriting first source");
  bound.save(out / "bound.dsculpt");
  auto reopened = Scene::load(out / "bound.dsculpt");
  ensure(reopened.encode() == accepted,
         "New unskinned result and original rig source reopen exactly");
  auto restored = reopened;
  restore_rig_source(restored, 2, false);
  ensure(restored.encode() == before,
         "Complete native Restore returns original unskinned "
         "morph/paint/affine state");
  double native_error = 0, export_error = 0;
  size_t corners = 0;
  for (int pose = 0; pose < 5; ++pose) {
    auto current = reopened;
    auto d = *current.object(2).deformation;
    const auto &rig = *source.object(1).deformation->skeleton;
    d.pose.clear();
    for (const auto &bone : rig.bones)
      d.pose.push_back(bone.rest);
    const double angle = .15 * pose;
    d.pose[2].rotation = {float(std::cos(angle / 2)), 0,
                          float(std::sin(angle / 2)), 0};
    current.set_deformation(2, d);
    const auto &mesh = current.object(2).painted->mesh();
    auto native = d.preview(mesh);
    const auto target_world = current.world_matrix(2),
               reference_world = source.world_matrix(1);
    const auto inverse_reference = reference_world.inverse();
    const auto joints = rig.world(d.pose), ibm = rig.inverse_bind();
    std::vector<P> expected;
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
      auto p = mesh.positions[i];
      const auto delta = d.morphs->targets[0].delta[i];
      p = p + delta * d.morph_weights[0];
      const auto physical = target_world.point({p.x, p.y, p.z});
      const auto local = inverse_reference.point(physical);
      const auto q = reference_world.point(
          point(joints[2].data(), point(ibm[2].data(), local)));
      expected.push_back(q);
      const auto actual =
          target_world.point({native.positions[i].x, native.positions[i].y,
                              native.positions[i].z});
      native_error =
          std::max(native_error, std::hypot(actual[0] - q[0], actual[1] - q[1],
                                            actual[2] - q[2]));
    }
    const auto file = out / ("pose-" + std::to_string(pose) + ".glb");
    auto selected_surfaces = current.export_surfaces();
    std::erase_if(selected_surfaces,
                  [](const auto &surface) { return surface.id != 2; });
    ensure(selected_surfaces.size() == 1,
           "Explicit selected-target export retains full native project "
           "reference separately");
    export_glb(selected_surfaces, file);
    cgltf_options opts{};
    cgltf_data *raw = nullptr;
    ensure(cgltf_parse_file(&opts, file.c_str(), &raw) == cgltf_result_success,
           "Independent native GLB parse");
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw, cgltf_free);
    ensure(cgltf_load_buffers(&opts, raw, file.c_str()) ==
                   cgltf_result_success &&
               cgltf_validate(raw) == cgltf_result_success,
           "Independent complete GLB validates");
    size_t found = 0;
    for (size_t n = 0; n < raw->nodes_count; ++n) {
      const auto &node = raw->nodes[n];
      if (!node.mesh || std::string(node.mesh->name ? node.mesh->name : "") !=
                            current.object(2).name)
        continue;
      ++found;
      size_t offset = 0;
      for (size_t pi = 0; pi < node.mesh->primitives_count; ++pi) {
        const auto &p = node.mesh->primitives[pi];
        auto position =
            cgltf_find_accessor(&p, cgltf_attribute_type_position, 0);
        const auto count = p.indices ? p.indices->count : position->count;
        ensure(offset + count <= mesh.indices.size(),
               "Exact current triangle corner bound");
        for (size_t c = 0; c < count; ++c) {
          const auto index =
              p.indices ? cgltf_accessor_read_index(p.indices, c) : c;
          auto actual = parsed(node, p, index);
          auto e = expected[mesh.indices[offset + c]];
          export_error = std::max(
              export_error,
              std::hypot(actual[0] - e[0], actual[1] - e[1], actual[2] - e[2]));
          ++corners;
        }
        offset += count;
      }
      ensure(offset == mesh.indices.size(),
             "All current source corners exported");
    }
    ensure(found == 1,
           "Actual unskinned target exists once in exported hierarchy");
  }
  ensure(native_error <= 3e-5 && export_error <= 3e-5,
         "Actual relative affine posed output and independent emitted LBS "
         "match reference matrices");
  Document doc;
  doc.replace(source);
  ensure(doc.wait_mesh(std::chrono::seconds(120)),
         "Native immutable input mesh ready");
  doc.configure_object_grab(true, true);
  doc.hold_object(2);
  ensure(doc.wait_mesh(std::chrono::seconds(120)),
         "Actual pinned target authority ready");
  const auto path = out / "native.dsculpt";
  doc.copy_scene().save(path);
  doc.bind_skin(options);
  auto save = doc.save_async(path);
  save.get();
  ensure(doc.wait_asset() && doc.wait_mesh(std::chrono::seconds(120)) &&
             doc.frame().state->error.empty(),
         "Actual typed native bind worker accepts producer plus ordered Save");
  ensure(
      doc.copy_scene().encode() == accepted &&
          Scene::load(path).encode() == accepted &&
          doc.frame().state->held_object == 2,
      "Actual pinned output/Save/held state exact and source ghost untouched");
  ensure(doc.undo() && doc.copy_scene().encode() == before && doc.redo() &&
             doc.copy_scene().encode() == accepted,
         "Native worker adds exactly one complete Undo/Redo");
  std::cout << "PASS geometry_authoring_checks=" << checks
            << " posed_corners=" << corners << " native_error=" << native_error
            << " export_error=" << export_error << " work=" << report.work
            << " workspace=" << report.planned_workspace
            << " native_public_worker=1 typed_admission=1\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
