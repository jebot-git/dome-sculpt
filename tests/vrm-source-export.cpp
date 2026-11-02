// SPDX-License-Identifier: GPL-3.0-or-later
#include "cgltf.h"
#include "dome/humanoid-creator.hpp"
#include "dome/import-authoring.hpp"
#include "dome/source-placement.hpp"
#include "dome/vrm.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <set>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
  ++checks;
}
struct Parsed {
  cgltf_data *data = nullptr;
  Parsed(const std::filesystem::path &p) {
    cgltf_options o{};
    if (cgltf_parse_file(&o, p.c_str(), &data) != cgltf_result_success ||
        cgltf_load_buffers(&o, data, p.c_str()) != cgltf_result_success)
      throw std::runtime_error("Cannot independently read emitted VRM");
  }
  ~Parsed() { cgltf_free(data); }
};
using Matrix = std::array<float, 16>;
const cgltf_accessor *attribute(const cgltf_attribute *a, size_t count,
                                cgltf_attribute_type type, int set = 0) {
  for (size_t i = 0; i < count; ++i)
    if (a[i].type == type && a[i].index == set)
      return a[i].data;
  return nullptr;
}
Vec3 point(const Matrix &m, Vec3 p) {
  return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
          m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
          m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
}
Matrix multiply(const Matrix &a, const Matrix &b) {
  Matrix result{};
  for (size_t c = 0; c < 4; ++c)
    for (size_t r = 0; r < 4; ++r)
      for (size_t k = 0; k < 4; ++k)
        result[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
  return result;
}
Vec3 vector(const cgltf_accessor *a, size_t index) {
  float value[3]{};
  if (!cgltf_accessor_read_float(a, index, value, 3))
    throw std::runtime_error("Independent emitted POSITION/morph read failed");
  return {value[0], value[1], value[2]};
}
std::vector<Vec3> evaluated(const cgltf_node &node) {
  const auto &p = node.mesh->primitives[0];
  auto position = attribute(p.attributes, p.attributes_count,
                            cgltf_attribute_type_position);
  std::vector<Matrix> skin;
  for (size_t joint = 0; joint < node.skin->joints_count; ++joint) {
    Matrix world{}, bind{};
    cgltf_node_transform_world(node.skin->joints[joint], world.data());
    if (!cgltf_accessor_read_float(node.skin->inverse_bind_matrices, joint,
                                   bind.data(), 16))
      throw std::runtime_error("Independent inverse bind read failed");
    skin.push_back(multiply(world, bind));
  }
  std::vector<Vec3> values;
  for (size_t v = 0; v < position->count; ++v) {
    auto P = vector(position, v);
    for (size_t target = 0; target < p.targets_count; ++target) {
      float weight = node.weights_count ? node.weights[target]
                                        : node.mesh->weights[target];
      auto a = attribute(p.targets[target].attributes,
                         p.targets[target].attributes_count,
                         cgltf_attribute_type_position);
      if (a && weight != 0)
        P += vector(a, v) * weight;
    }
    Vec3 world{};
    for (int set = 0;; ++set) {
      auto joints = attribute(p.attributes, p.attributes_count,
                              cgltf_attribute_type_joints, set),
           weights = attribute(p.attributes, p.attributes_count,
                               cgltf_attribute_type_weights, set);
      if (!joints)
        break;
      float ji[4]{}, w[4]{};
      if (!weights || !cgltf_accessor_read_float(joints, v, ji, 4) ||
          !cgltf_accessor_read_float(weights, v, w, 4))
        throw std::runtime_error("Independent influence read failed");
      for (size_t k = 0; k < 4; ++k)
        if (w[k] != 0)
          world += point(skin.at(size_t(ji[k])), P) * w[k];
    }
    values.push_back(world);
  }
  return values;
}
void exact_original_binds(const ObjectSurface &native,
                          const cgltf_node &emitted,
                          const cgltf_data &original) {
  const auto &a = *native.appearance;
  const auto &input = *original.nodes[size_t(a.source_node)].skin;
  Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  for (size_t bone = 0; bone < a.source_joint_nodes.size(); ++bone) {
    Matrix expected = identity, actual{};
    auto source = a.source_joint_nodes[bone];
    for (size_t slot = 0; slot < input.joints_count; ++slot)
      if (input.joints[slot] == original.nodes + source &&
          input.inverse_bind_matrices)
        check(cgltf_accessor_read_float(input.inverse_bind_matrices, slot,
                                        expected.data(), 16),
              "Independent original authored inverse bind decodes");
    check(cgltf_accessor_read_float(emitted.skin->inverse_bind_matrices, bone,
                                    actual.data(), 16),
          "Independent emitted bone-aligned inverse bind decodes");
    for (size_t k = 0; k < 16; ++k)
      check(std::bit_cast<uint32_t>(actual[k]) ==
                std::bit_cast<uint32_t>(expected[k]),
            "Each source skin keeps every authored IBM float bit; unweighted "
            "closure uses explicit identity");
  }
}
void with_jaw_source(const std::filesystem::path &input,
                     const std::filesystem::path &output) {
  Parsed parsed(input);
  std::ifstream file(input, std::ios::binary);
  std::vector<uint8_t> raw{std::istreambuf_iterator<char>(file), {}};
  auto u32 = [&](size_t at) {
    return uint32_t(raw.at(at)) | uint32_t(raw.at(at + 1)) << 8 |
           uint32_t(raw.at(at + 2)) << 16 | uint32_t(raw.at(at + 3)) << 24;
  };
  auto json_size = u32(12);
  auto json =
      nlohmann::json::parse(raw.begin() + 20, raw.begin() + 20 + json_size);
  auto bin_start = size_t(28) + json_size;
  std::vector<uint8_t> binary(raw.begin() + bin_start, raw.end());
  while (binary.size() % 4)
    binary.push_back(0);
  auto start = binary.size();
  auto put = [](std::vector<uint8_t> &bytes, uint32_t value) {
    for (int k = 0; k < 4; ++k)
      bytes.push_back(uint8_t(value >> (k * 8)));
  };
  const auto &skin = parsed.data->skins[0];
  for (size_t slot = 0; slot < skin.joints_count; ++slot) {
    float matrix[16]{};
    if (!cgltf_accessor_read_float(skin.inverse_bind_matrices, slot, matrix,
                                   16))
      throw std::runtime_error("Fixture inverse bind read failed");
    for (float value : matrix)
      put(binary, std::bit_cast<uint32_t>(value));
  }
  const float identity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  for (float value : identity)
    put(binary, std::bit_cast<uint32_t>(value));
  size_t head = 0;
  for (const auto &bone : json["extensions"]["VRM"]["humanoid"]["humanBones"])
    if (bone["bone"].get<std::string>() == "head")
      head = bone["node"].get<size_t>();
  auto joint = json["nodes"].size();
  json["nodes"][head]["children"].push_back(joint);
  json["nodes"].push_back(
      {{"name", "Explicit source jaw"}, {"translation", {0.f, -.03f, .02f}}});
  json["skins"][0]["joints"].push_back(joint);
  auto view = json["bufferViews"].size();
  json["bufferViews"].push_back({{"buffer", 0},
                                 {"byteOffset", start},
                                 {"byteLength", binary.size() - start}});
  auto accessor = json["accessors"].size();
  json["accessors"].push_back({{"bufferView", view},
                               {"componentType", 5126},
                               {"type", "MAT4"},
                               {"count", skin.joints_count + 1}});
  json["skins"][0]["inverseBindMatrices"] = accessor;
  json["buffers"][0]["byteLength"] = binary.size();
  auto text = json.dump();
  while (text.size() % 4)
    text += ' ';
  std::vector<uint8_t> bytes;
  for (auto value :
       {0x46546c67u, 2u, uint32_t(28 + text.size() + binary.size()),
        uint32_t(text.size()), 0x4e4f534au})
    put(bytes, value);
  bytes.insert(bytes.end(), text.begin(), text.end());
  put(bytes, uint32_t(binary.size()));
  put(bytes, 0x004e4942);
  bytes.insert(bytes.end(), binary.begin(), binary.end());
  std::ofstream result(output, std::ios::binary);
  result.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}
} // namespace
int main(int argc, char **argv) try {
  if (argc != 4)
    return 2;
  auto home = std::filesystem::temp_directory_path() /
              ("dome-vrm-export-" + std::to_string(getpid()));
  std::filesystem::create_directories(home);
  setenv("HOME", home.c_str(), 1);
  setenv("XDG_STATE_HOME", home.c_str(), 1);
  setenv("XDG_CONFIG_HOME", home.c_str(), 1);
  std::filesystem::create_directories(argv[3]);
  struct Cleanup {
    std::filesystem::path p;
    ~Cleanup() {
      std::error_code ec;
      std::filesystem::remove_all(p, ec);
    }
  } cleanup{home};
  for (int model = 1; model <= 2; ++model) {
    ImportAuthoringOptions options;
    options.paint_resolution = 64;
    Parsed original(argv[model]);
    auto imported = prepare_authoring_import(Scene{}, argv[model], options);
    auto objects = imported.scene.surfaces();
    check(objects.size() == 19, "Actual complete native imported avatar "
                                "surfaces are exportable inputs");
    for (float weight : {0.f, .35f}) {
      auto scene = imported.scene;
      for (auto id : imported.objects) {
        auto d = *scene.object(id).deformation;
        d.pose.clear();
        for (const auto &b : d.skeleton->bones)
          d.pose.push_back(b.rest);
        if (weight > 0)
          d.pose[d.humanoid->joints.at("hips")].scale = {1.1f, .9f, 1.2f};
        if (d.morphs) {
          d.morph_weights.assign(d.morphs->targets.size(), 0);
          if (!d.morph_weights.empty())
            d.morph_weights[0] = weight;
        }
        scene.set_deformation(id, std::move(d));
      }
      auto path =
          std::filesystem::path(argv[3]) /
          (std::to_string(model) + "-" + std::to_string(weight) + ".vrm");
      auto surfaces = scene.surfaces();
      auto report = export_vrm(surfaces, path);
      Parsed output(path);
      check(report.meshes == 19 && output.data->meshes_count == 19,
            "Version-preserving native source exporter emits every actual "
            "source primitive");
      check(cgltf_validate(output.data) == cgltf_result_success,
            "New emitted VRM has independently valid glTF structure despite "
            "inherited invalid raw advisory hint");
      const cgltf_skin *first = nullptr;
      size_t parts = 0;
      for (size_t n = 0; n < output.data->nodes_count; ++n) {
        auto &node = output.data->nodes[n];
        if (!node.mesh)
          continue;
        check(node.skin && node.skin->skeleton,
              "Every emitted skinned source surface has a valid common-root "
              "hint");
        auto mesh_index = size_t(node.mesh - output.data->meshes);
        const auto &native = surfaces.at(mesh_index);
        exact_original_binds(native, node, *original.data);
        auto positions = evaluated(node);
        auto expected = native.preview_mesh();
        auto world = native.world_matrix();
        float error = 0;
        const auto &primitive = node.mesh->primitives[0];
        check(primitive.indices->count == expected.indices.size(),
              "Emitted surface retains exact ordered native triangle corners");
        for (size_t c = 0; c < expected.indices.size(); ++c) {
          auto p = expected.positions[expected.indices[c]];
          auto q = world.point({p.x, p.y, p.z});
          auto value =
              positions.at(cgltf_accessor_read_index(primitive.indices, c));
          error = std::max(error, length(value - Vec3{float(q[0]), float(q[1]),
                                                      float(q[2])}));
        }
        if (error >= 2e-6f)
          throw std::runtime_error(
              "Independent original-space posed corner error " +
              std::to_string(error));
        if (!first)
          first = node.skin;
        check(node.skin->joints_count == first->joints_count,
              "Distinct source skins use one complete source-joint vocabulary");
        for (size_t j = 0; j < first->joints_count; ++j)
          check(node.skin->joints[j] == first->joints[j],
                "All nineteen parts reference one actual shared emitted "
                "original-node graph");
        ++parts;
      }
      check(
          parts == 19,
          "All actual source instances retain separate emitted surface nodes");
    }
  }
  {
    ImportAuthoringOptions options;
    options.paint_resolution = 64;
    auto fixture = home / "authored-jaw.vrm";
    with_jaw_source(argv[1], fixture);
    auto prepared = prepare_authoring_import(Scene{}, fixture, options);
    auto scene = prepared.scene;
    auto id = prepared.objects.front();
    auto d = *scene.object(id).deformation;
    auto head = d.humanoid->joints.at("head");
    d.skeleton->bones[head].name = "Edited head 頭";
    std::set<uint32_t> mapped;
    for (auto &[role, joint] : d.humanoid->joints)
      mapped.insert(joint);
    std::optional<uint32_t> jaw;
    for (size_t candidate = 0; candidate < d.skeleton->bones.size();
         ++candidate) {
      if (mapped.contains(uint32_t(candidate)))
        continue;
      for (int parent = d.skeleton->bones[candidate].parent; parent >= 0;
           parent = d.skeleton->bones[size_t(parent)].parent)
        if (uint32_t(parent) == head) {
          jaw = uint32_t(candidate);
          break;
        }
      if (jaw)
        break;
    }
    check(bool(jaw), "Actual avatar supplies an unmapped descendant for an "
                     "explicit optional-role editing fixture");
    d.humanoid->joints["jaw"] = *jaw;
    scene.set_deformation(id, d);
    auto before = scene.encode();
    auto path = std::filesystem::path(argv[3]) / "edited-metadata.vrm";
    export_vrm(scene.surfaces(), path);
    Parsed parsed(path);
    auto json = nlohmann::json::parse(
        parsed.data->json, parsed.data->json + parsed.data->json_size);
    std::map<std::string, uint32_t> roles;
    for (auto &bone : json["extensions"]["VRM"]["humanoid"]["humanBones"])
      roles.emplace(bone["bone"].get<std::string>(),
                    bone["node"].get<uint32_t>());
    check(roles.contains("jaw") &&
              std::string(parsed.data->nodes[roles.at("head")].name) ==
                  "Edited head 頭",
          "Native authored joint-name and optional humanoid-role edits reach "
          "one shared emitted source graph");
    check(scene.encode() == before, "Metadata emission does not rewrite any "
                                    "retained original source/native history");
    auto conflict = scene;
    auto other = *conflict.object(prepared.objects[1]).deformation;
    other.skeleton->bones[other.humanoid->joints.at("head")].name =
        "Conflicting head";
    conflict.set_deformation(prepared.objects[1], std::move(other));
    auto protected_path = home / "conflict.vrm";
    {
      std::ofstream f(protected_path);
      f << "retained output";
    }
    bool refused = false;
    try {
      export_vrm(conflict.surfaces(), protected_path);
    } catch (const std::invalid_argument &error) {
      refused =
          std::string_view(error.what()).find("conflicting authored names") !=
          std::string_view::npos;
    }
    std::ifstream previous(protected_path);
    std::string retained{std::istreambuf_iterator<char>(previous), {}};
    check(refused && retained == "retained output",
          "Conflicting per-skin authored names refuse atomically instead of "
          "silently choosing or dropping an edit");
  }
  std::cout << checks << " native original-space VRM export checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
