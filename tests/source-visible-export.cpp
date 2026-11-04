// SPDX-License-Identifier: GPL-3.0-or-later
#include "cgltf.h"
#include "dome/creation.hpp"
#include "dome/skin-authoring.hpp"
#include "dome/gltf-export.hpp"
#include "dome/rig-rest.hpp"
#include "source-visible-pose.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
void check(bool v, const char *why) {
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
  check(positions && cgltf_accessor_read_float(positions, v, base, 3),
        "Independent POSITION read");
  P value{base[0], base[1], base[2]};
  for (size_t t = 0; t < p.targets_count; ++t) {
    float w = node.weights_count > t         ? node.weights[t]
              : node.mesh->weights_count > t ? node.mesh->weights[t]
                                             : 0;
    for (size_t a = 0; a < p.targets[t].attributes_count; ++a)
      if (p.targets[t].attributes[a].type == cgltf_attribute_type_position) {
        float delta[3]{};
        check(cgltf_accessor_read_float(p.targets[t].attributes[a].data, v,
                                        delta, 3),
              "Independent morph POSITION read");
        for (size_t c = 0; c < 3; ++c)
          value[c] += double(w) * delta[c];
      }
  }
  check(node.skin && node.skin->inverse_bind_matrices,
        "Actual skin/inverse-bind authority retained");
  P result{};
  size_t influences = 0;
  for (int set = 0; set < 8; ++set) {
    auto joints = cgltf_find_accessor(&p, cgltf_attribute_type_joints, set);
    auto weights = cgltf_find_accessor(&p, cgltf_attribute_type_weights, set);
    if (!joints && !weights)
      break;
    check(joints && weights, "Paired joint/weight sets");
    unsigned j[4]{};
    float w[4]{};
    check(cgltf_accessor_read_uint(joints, v, j, 4) &&
              cgltf_accessor_read_float(weights, v, w, 4),
          "Independent weight reads");
    for (size_t k = 0; k < 4; ++k)
      if (w[k]) {
        check(j[k] < node.skin->joints_count,
              "Independent joint index in range");
        float bind[16]{}, world[16]{};
        check(cgltf_accessor_read_float(node.skin->inverse_bind_matrices, j[k],
                                        bind, 16),
              "Independent inverse bind read");
        cgltf_node_transform_world(node.skin->joints[j[k]], world);
        const auto transformed = point(world, point(bind, value));
        for (size_t c = 0; c < 3; ++c)
          result[c] += double(w[k]) * transformed[c];
        ++influences;
      }
  }
  check(influences > 0 && influences <= 4,
        "Emitted source method keeps fixed4 nonzero influences");
  return result;
}
} // namespace
int main(int argc, char **argv) try {
  check(argc == 3, "Choose gender and export directory");
  std::string gender = argv[1];
  check(gender == "female" || gender == "male", "Choose actual baseline");
  std::filesystem::path out = argv[2];
  std::filesystem::create_directories(out);
  Scene scene;
  append_humanoid(scene,
                  gender == "male"
                      ? humanoid_baseline_defaults("vroid-beta-male")
                      : HumanoidControls{},
                  "Exported " + gender, {}, 64);
  auto input = scene.encode();
  std::vector<uint64_t> targets;
  for (const auto &o : scene.objects())
    if (o.painted)
      targets.push_back(o.id);
  check(targets.size() == 19, "Actual19part source");
  SkinAuthoringOptions options;
  options.mode = SkinAuthoringMode::SourceVisible;
  options.reference = targets.front();
  check(author_source_visible_skin(scene, targets, options),
        "Real full-group source helper changes weights");
  scene.save(out / "bound.dsculpt");
  auto reopened = Scene::load(out / "bound.dsculpt");
  check(reopened.encode() == scene.encode(),
        "Full native saved source/field closure roundtrips");
  auto restored = reopened;
  for (auto id : targets)
    if (restored.object(id).rig_source)
      restore_rig_source(restored, id, false);
  check(restored.encode() == input,
        "Full original skin/paint/material/shape source restoration exact");
  size_t meshes = 0, corners = 0;
  double maximum = 0;
  for (int pose = 0; pose < 5; ++pose) {
    auto current = reopened;
    for (auto id : targets) {
      auto d = *current.object(id).deformation;
      source_visible_test::pose(d, pose);
      current.set_deformation(id, std::move(d));
    }
    auto path = out / ("pose-" + std::to_string(pose) + ".glb");
    export_glb(current.export_surfaces(), path);
    cgltf_options opts{};
    cgltf_data *raw = nullptr;
    check(cgltf_parse_file(&opts, path.c_str(), &raw) == cgltf_result_success,
          "Independent cgltf parses current emitted file");
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw, cgltf_free);
    check(cgltf_load_buffers(&opts, raw, path.c_str()) ==
                  cgltf_result_success &&
              cgltf_validate(raw) == cgltf_result_success,
          "Independent complete emitted hierarchy/accessors validate");
    size_t count = 0;
    double pose_error = 0;
    for (size_t n = 0; n < raw->nodes_count; ++n) {
      const auto &node = raw->nodes[n];
      if (!node.mesh)
        continue;
      const auto name = std::string(node.mesh->name ? node.mesh->name : "");
      auto found = std::find_if(
          current.objects().begin(), current.objects().end(),
          [&](const auto &o) { return o.painted && o.name == name; });
      check(found != current.objects().end(),
            "Emitted mesh names actual current source object");
      const auto &mesh = found->painted->mesh();
      auto native = source_visible_test::points(mesh, *found->deformation);
      const auto placement = current.world_matrix(found->id);
      size_t offset = 0;
      for (size_t pi = 0; pi < node.mesh->primitives_count; ++pi) {
        const auto &p = node.mesh->primitives[pi];
        auto positions =
            cgltf_find_accessor(&p, cgltf_attribute_type_position, 0);
        check(p.type == cgltf_primitive_type_triangles && positions,
              "Emitted native triangle primitive");
        const size_t indices = p.indices ? p.indices->count : positions->count;
        check(offset + indices <= mesh.indices.size(),
              "Exact ordered corner bound");
        for (size_t c = 0; c < indices; ++c) {
          const auto emitted =
              p.indices ? cgltf_accessor_read_index(p.indices, c) : c;
          const auto expected =
              placement.point(native.at(mesh.indices.at(offset + c)));
          const auto actual = parsed(node, p, emitted);
          const double error =
              std::hypot(actual[0] - expected[0], actual[1] - expected[1],
                         actual[2] - expected[2]);
          pose_error = std::max(pose_error, error);
          ++corners;
        }
        offset += indices;
      }
      check(offset == mesh.indices.size(),
            "Every actual source triangle/corner exported exactly once");
      ++count;
    }
    check(count == 19, "All19 actual skinned parts survive current export");
    meshes += count;
    maximum = std::max(maximum, pose_error);
    std::cout << "POSE gender=" << gender << " pose=" << pose
              << " parts=" << count << " current_export_error=" << pose_error
              << std::endl;
    check(
        pose_error < 3e-5,
        "Independent current exported skin/pose/morph positions equal native");
  }
  std::cout << "PASS gender=" << gender << " posed_meshes=" << meshes
            << " parsed_corners=" << corners
            << " max_position_error=" << maximum << " reopen=1 restore=1\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
