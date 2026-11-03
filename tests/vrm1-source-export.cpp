// SPDX-License-Identifier: GPL-3.0-or-later
#include "cgltf.h"
#include "dome/document.hpp"
#include "dome/import-authoring.hpp"
#include "dome/rig-edit.hpp"
#include "dome/vrm.hpp"
#include "vrm1-metadata.hpp"
#include <fstream>
#include <iostream>
#define main matcap_fixture_main
#include "vrm-matcap-import.cpp"
#undef main
namespace {
using J = nlohmann::json;
std::vector<uint8_t> bytes(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
J json(const std::filesystem::path &p) {
  auto b = bytes(p);
  auto count = uint32_t(b.at(12)) | uint32_t(b.at(13)) << 8 |
               uint32_t(b.at(14)) << 16 | uint32_t(b.at(15)) << 24;
  return J::parse(b.begin() + 20, b.begin() + 20 + count);
}
import_fixture::Fixture metadata_source() {
  auto f = source(false);
  f.j["meshes"][0]["extras"]["authorOpaque"] = {
      {"text", "Original mesh 日本語"}, {"values", {1, 3, 7}}};
  f.j["meshes"][0]["primitives"][0]["extras"] = {
      {"authorOpaque", {{"flag", true}, {"value", .125f}}}};
  import_fixture::Fixture original;
  for (const char *uv : {"TEXCOORD_0", "TEXCOORD_1"})
    f.j["meshes"][0]["primitives"][0]["attributes"][uv] =
        original.j["meshes"][0]["primitives"][0]["attributes"][uv];
  f.j["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"] = {
      {"index", 0},
      {"extensions",
       {{"KHR_texture_transform",
         {{"offset", {.07f, -.11f}},
          {"rotation", .19f},
          {"scale", {.8f, 1.2f}},
          {"texCoord", 1}}}}}};
  f.j["materials"][0]["extensions"]["VRMC_materials_mtoon"]
     ["shadeMultiplyTexture"] = {{"index", 0}, {"texCoord", 1}};
  f.j["extensionsUsed"].push_back("KHR_texture_transform");
  auto &vrm = f.j["extensions"]["VRMC_vrm"];
  vrm["firstPerson"]["meshAnnotations"] = {
      {{"node", 1}, {"type", "both"}},
      {{"node", 2}, {"type", "thirdPersonOnly"}}};
  vrm["meta"]["thumbnailImage"] = 0;
  vrm["lookAt"] = {{"type", "expression"},
                   {"offsetFromHeadBone", {.01f, .02f, -.03f}},
                   {"rangeMapHorizontalInner",
                    {{"inputMaxValue", 90}, {"outputScale", .7f}}}};
  vrm["expressions"]["preset"]["happy"] = {
      {"isBinary", true},
      {"overrideBlink", "blend"},
      {"morphTargetBinds", {{{"node", 1}, {"index", 0}, {"weight", .35f}}}},
      {"materialColorBinds",
       {{{"material", 0},
         {"type", "color"},
         {"targetValue", {.4f, .5f, .6f, .7f}}}}},
      {"textureTransformBinds",
       {{{"material", 0}, {"scale", {.7f, .9f}}, {"offset", {.2f, -.1f}}}}}};
  const auto head = vrm["humanoid"]["humanBones"]["head"]["node"].get<size_t>();
  const auto root = f.j["nodes"].size();
  // Authored, genuinely weighted skins with different mesh bind spaces. The
  // third part below is genuinely rigid, attached to that same weighted head.
  while (f.bin.size() % 4)
    f.bin.push_back(0);
  auto start = f.bin.size();
  for (size_t v = 0; v < 4; ++v)
    for (size_t k = 0; k < 4; ++k) {
      f.bin.push_back(0);
      f.bin.push_back(0);
    }
  const auto view = f.j["bufferViews"].size();
  f.j["bufferViews"].push_back({{"buffer", 0},
                                {"byteOffset", start},
                                {"byteLength", f.bin.size() - start}});
  const auto joints = f.j["accessors"].size();
  f.j["accessors"].push_back({{"bufferView", view},
                              {"componentType", 5123},
                              {"count", 4},
                              {"type", "VEC4"}});
  auto weights =
      f.values({1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, 4, "VEC4");
  f.j["meshes"][0]["primitives"][0]["attributes"]["JOINTS_0"] = joints;
  f.j["meshes"][0]["primitives"][0]["attributes"]["WEIGHTS_0"] = weights;
  auto one =
      f.values({1.2f, 0, 0, 0, 0, 1.2f, 0, 0, 0, 0, 1.2f, 0, .2f, -.3f, 0, 1},
               16, "MAT4");
  auto two = f.values({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -.3f, -.3f, -.2f, 1},
                      16, "MAT4");
  f.j["skins"] = {{{"name", "First exact head bind"},
                   {"joints", {head}},
                   {"inverseBindMatrices", one}},
                  {{"name", "Second exact head bind"},
                   {"joints", {head}},
                   {"inverseBindMatrices", two}}};
  f.j["nodes"][1]["skin"] = 0;
  f.j["nodes"][2]["skin"] = 1;
  f.j["nodes"][head]["children"].push_back(root);
  f.j["nodes"].push_back({{"name", "Actual spring root"},
                          {"translation", {0, .03f, 0}},
                          {"children", {root + 1}}});
  f.j["nodes"].push_back(
      {{"name", "Actual spring tail"}, {"translation", {0, .04f, 0}}});
  f.j["extensions"]["VRMC_springBone"] = {
      {"specVersion", "1.0"},
      {"colliders",
       {{{"node", head},
         {"shape",
          {{"sphere", {{"offset", {.01f, .02f, .03f}}, {"radius", .015f}}}}}}}},
      {"colliderGroups", {{{"name", "Actual collider"}, {"colliders", {0}}}}},
      {"springs",
       {{{"name", "Explicit ordered spring"},
         {"joints",
          {{{"node", root},
            {"stiffness", .4f},
            {"gravityDir", {.2f, -.9f, .1f}}},
           {{"node", root + 1}, {"dragForce", .3f}}}},
         {"center", head},
         {"colliderGroups", {0}}}}}};
  for (const char *kind : {"roll", "aim", "rotation"}) {
    auto id = f.j["nodes"].size();
    J constraint = {{"source", head}, {"weight", .7f}};
    if (std::string_view(kind) == "roll")
      constraint["rollAxis"] = "Y";
    if (std::string_view(kind) == "aim")
      constraint["aimAxis"] = "PositiveZ";
    f.j["nodes"][0]["children"].push_back(id);
    f.j["nodes"].push_back(
        {{"name", std::string("Actual ") + kind + " constraint"},
         {"extensions",
          {{"VRMC_node_constraint",
            {{"specVersion", "1.0"}, {"constraint", {{kind, constraint}}}}}}}});
  }
  const auto accessory = f.j["nodes"].size();
  f.j["nodes"][head]["children"].push_back(accessory);
  f.j["nodes"].push_back({{"name", "Genuine rigid head accessory"},
                          {"mesh", 0},
                          {"translation", {.1f, .2f, -.1f}},
                          {"weights", {.6f}}});
  // A real unweighted source-joint chain exceeds the editable-group cap, while
  // remaining within the unchanged rig/node quotas. Every original node and
  // authored IBM stays in source authority; only mesh-ancestor placement groups
  // are needed to present the three actual surfaces.
  const auto hair = f.j["nodes"].size();
  for (size_t i = 0; i < 130; ++i) {
    auto n = f.j["nodes"].size();
    auto parent = i ? n - 1 : head;
    f.j["nodes"][parent]["children"].push_back(n);
    f.j["nodes"].push_back(
        {{"name", "Original unweighted hair joint " + std::to_string(i)},
         {"translation", {0, .001f, 0}}});
  }
  for (size_t skin = 0; skin < 2; ++skin) {
    while (f.bin.size() % 4)
      f.bin.push_back(0);
    auto offset = f.bin.size();
    for (size_t joint = 0; joint <= 130; ++joint) {
      std::array<float, 16> matrix{1, 0, 0, 0, 0, 1, 0, 0,
                                   0, 0, 1, 0, 0, 0, 0, 1};
      if (!joint) {
        if (skin == 0) {
          matrix[0] = matrix[5] = matrix[10] = 1.2f;
          matrix[12] = .2f;
          matrix[13] = -.3f;
        } else {
          matrix[12] = -.3f;
          matrix[13] = -.3f;
          matrix[14] = -.2f;
        }
      }
      for (float value : matrix)
        import_fixture::word(f.bin, std::bit_cast<uint32_t>(value));
      if (joint)
        f.j["skins"][skin]["joints"].push_back(hair + joint - 1);
    }
    auto view = f.j["bufferViews"].size();
    f.j["bufferViews"].push_back({{"buffer", 0},
                                  {"byteOffset", offset},
                                  {"byteLength", f.bin.size() - offset}});
    auto accessor = f.j["accessors"].size();
    f.j["accessors"].push_back({{"bufferView", view},
                                {"componentType", 5126},
                                {"count", 131},
                                {"type", "MAT4"}});
    f.j["skins"][skin]["inverseBindMatrices"] = accessor;
  }
  for (auto &node : f.j["nodes"])
    if (node.value("name", std::string{}) == "Actual rotation constraint")
      node["extensions"]["VRMC_node_constraint"]["constraint"]["rotation"]
          ["source"] = hair + 129;
  f.j["extensionsUsed"].push_back("VRMC_springBone");
  f.j["extensionsUsed"].push_back("VRMC_node_constraint");
  return f;
}
void independent(const Scene &scene, const std::filesystem::path &output) {
  cgltf_options options{};
  cgltf_data *raw = nullptr;
  check(cgltf_parse_file(&options, output.c_str(), &raw) ==
            cgltf_result_success,
        "Independent cgltf consumer parses actual source-version V1 output");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> owner(raw, cgltf_free);
  check(cgltf_load_buffers(&options, raw, output.c_str()) ==
                cgltf_result_success &&
            cgltf_validate(raw) == cgltf_result_success,
        "Independent output accessors, index tables and buffers are valid");
  auto surfaces = scene.preview_surfaces();
  size_t parts = 0, corners = 0;
  float error = 0;
  const auto original_bytes = surfaces.front().appearance->original_bytes;
  cgltf_data *original = nullptr;
  check(cgltf_parse(&options, original_bytes->data(), original_bytes->size(),
                    &original) == cgltf_result_success,
        "Independent reader captures the same immutable original GLB bytes");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> original_owner(original,
                                                                    cgltf_free);
  check(
      cgltf_load_buffers(&options, original, nullptr) == cgltf_result_success,
      "Independent original accessor buffers load without filesystem re-read");
  for (size_t n = 0; n < raw->nodes_count; ++n) {
    const auto &node = raw->nodes[n];
    if (!node.mesh)
      continue;
    const auto mesh = size_t(node.mesh - raw->meshes);
    check(mesh < surfaces.size(),
          "Every emitted current primitive has one native surface witness");
    const auto &surface = surfaces[mesh];
    auto expected = surface.preview_mesh();
    auto world = surface.world_matrix();
    check(bool(node.skin) ==
              bool(surface.deformation && surface.deformation->skin),
          "Real source skins and genuine rigid parts retain their respective "
          "ownership without dummy skins");
    std::array<float, 16> node_world;
    cgltf_node_transform_world(&node, node_world.data());
    const auto &primitive = node.mesh->primitives[0];
    auto attribute = [](const cgltf_attribute *attrs, size_t count,
                        cgltf_attribute_type type,
                        int set = 0) -> const cgltf_accessor * {
      for (size_t i = 0; i < count; ++i)
        if (attrs[i].type == type && attrs[i].index == set)
          return attrs[i].data;
      return nullptr;
    };
    const auto *position =
        attribute(primitive.attributes, primitive.attributes_count,
                  cgltf_attribute_type_position);
    check(position, "Emitted surface has actual POSITION accessor");
    auto count = primitive.indices ? primitive.indices->count : position->count;
    check(
        count == expected.indices.size(),
        "Every ordered current triangle corner survives source graph emission");
    auto point = [](const std::array<float, 16> &m, Vec3 p) {
      return Vec3{m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
                  m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
                  m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
    };
    std::vector<std::array<float, 16>> skin;
    if (node.skin)
      for (size_t j = 0; j < node.skin->joints_count; ++j) {
        std::array<float, 16> w,
            b{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, product{};
        cgltf_node_transform_world(node.skin->joints[j], w.data());
        if (node.skin->inverse_bind_matrices)
          check(cgltf_accessor_read_float(node.skin->inverse_bind_matrices, j,
                                          b.data(), 16),
                "Independent authored per-skin inverse bind decodes");
        for (size_t c = 0; c < 4; ++c)
          for (size_t r = 0; r < 4; ++r)
            for (size_t k = 0; k < 4; ++k)
              product[c * 4 + r] += w[k * 4 + r] * b[c * 4 + k];
        skin.push_back(product);
      }
    if (node.skin) {
      const auto &a = *surface.appearance;
      const auto *before = original->nodes[size_t(a.source_node)].skin;
      check(before && a.source_joint_nodes.size() == node.skin->joints_count,
            "Skin/joint correspondence uses authenticated original node IDs, "
            "not names");
      for (size_t j = 0; j < a.source_joint_nodes.size(); ++j) {
        std::array<float, 16> wanted{1, 0, 0, 0, 0, 1, 0, 0,
                                     0, 0, 1, 0, 0, 0, 0, 1},
            actual{};
        for (size_t old = 0; old < before->joints_count; ++old)
          if (before->joints[old] ==
                  original->nodes + a.source_joint_nodes[j] &&
              before->inverse_bind_matrices)
            check(cgltf_accessor_read_float(before->inverse_bind_matrices, old,
                                            wanted.data(), 16),
                  "Original authored IBM accessor independently decodes");
        check(cgltf_accessor_read_float(node.skin->inverse_bind_matrices, j,
                                        actual.data(), 16),
              "Current bone-aligned IBM accessor independently decodes");
        check(std::equal(actual.begin(), actual.end(), wanted.begin(),
                         [](float x, float y) {
                           return std::bit_cast<uint32_t>(x) ==
                                  std::bit_cast<uint32_t>(y);
                         }),
              "Each original skin retains exact IBM float bits; unweighted "
              "closure is explicit identity");
      }
    }
    for (size_t c = 0; c < count; ++c) {
      auto v = primitive.indices
                   ? cgltf_accessor_read_index(primitive.indices, c)
                   : c;
      float components[3];
      check(cgltf_accessor_read_float(position, v, components, 3),
            "Independent current POSITION accessor decodes");
      for (size_t t = 0; t < primitive.targets_count; ++t) {
        float weight = node.weights_count         ? node.weights[t]
                       : node.mesh->weights_count ? node.mesh->weights[t]
                                                  : 0;
        auto delta = attribute(primitive.targets[t].attributes,
                               primitive.targets[t].attributes_count,
                               cgltf_attribute_type_position);
        if (delta) {
          float d[3];
          check(cgltf_accessor_read_float(delta, v, d, 3),
                "Independent POSITION target accessor decodes");
          for (size_t k = 0; k < 3; ++k)
            components[k] += weight * d[k];
        }
      }
      Vec3 p{components[0], components[1], components[2]}, actual{};
      if (node.skin)
        for (int set = 0;; ++set) {
          auto joints =
              attribute(primitive.attributes, primitive.attributes_count,
                        cgltf_attribute_type_joints, set);
          if (!joints)
            break;
          auto weights =
              attribute(primitive.attributes, primitive.attributes_count,
                        cgltf_attribute_type_weights, set);
          float js[4], ws[4];
          check(weights && cgltf_accessor_read_float(joints, v, js, 4) &&
                    cgltf_accessor_read_float(weights, v, ws, 4),
                "Independent original joint slots and real nonzero influences "
                "decode");
          for (size_t k = 0; k < 4; ++k)
            if (ws[k] != 0)
              actual += point(skin.at(size_t(js[k])), p) * ws[k];
        }
      else
        actual = point(node_world, p);
      auto native = expected.positions[expected.indices[c]];
      auto wanted = world.point({native.x, native.y, native.z});
      error = std::max(error,
                       length(actual - Vec3{float(wanted[0]), float(wanted[1]),
                                            float(wanted[2])}));
      ++corners;
    }
    ++parts;
  }
  check(parts == surfaces.size() && error < 2e-6f,
        "Independent posed skin/rigid world corners match actual native "
        "geometry within two micrometres");
  std::cout << "VRM1 independent corners=" << corners << " maxerror=" << error
            << '\n';
  auto emitted = json(output);
  check(emitted["extensions"]["VRMC_vrm"]["specVersion"] == "1.0" &&
            !emitted["extensions"].contains("VRM"),
        "Output preserves actual VRM1 version without legacy relabel");
}
} // namespace
int main(int argc, char **argv) try {
  if (argc < 3)
    return 2;
  if (std::string_view(argv[1]) == "--fixture") {
    source(false).save(argv[2]);
    return 0;
  }
  const bool selftest = std::string_view(argv[1]) == "--selftest";
  std::filesystem::path input = argv[1], output = argv[2];
  if (selftest) {
    std::filesystem::create_directories(output);
    input = output / "source.vrm";
    metadata_source().save(input);
    output /= "current.vrm";
  }
  dome::ImportAuthoringOptions options;
  options.paint_resolution = 64;
  options.prepare_texture_atlas = false;
  auto imported = dome::prepare_authoring_import(dome::Scene{}, input, options);
  auto before = imported.scene.encode();
  std::cout << "Native VRM1 imported " << imported.objects.size() << " parts\n"
            << std::flush;
  try {
    auto report = dome::export_vrm(imported.scene.preview_surfaces(), output);
    if (imported.scene.encode() != before)
      throw std::runtime_error("Export modified native source");
    std::cout << "Native VRM1 export meshes=" << report.meshes
              << " joints=" << report.joints << '\n';
    independent(imported.scene, output);
    const auto project = output.parent_path() / "retained.dsculpt";
    imported.scene.save(project);
    check(Scene::load(project).encode() == before,
          "Native source/typed roles/transforms/morphs and raw V1 metadata "
          "restart exactly");
    if (selftest) {
      auto fixture = metadata_source();
      auto expected = fixture.j;
      detail::validate_vrm1_metadata(expected);
      const auto emitted = json(output);
      const auto &actual = emitted["extensions"]["VRMC_vrm"];
      for (const auto &mesh : emitted.at("meshes"))
        check(
            mesh.at("extras").at("authorOpaque") ==
                    expected.at("meshes")[0].at("extras").at("authorOpaque") &&
                mesh.at("primitives")[0].at("extras") ==
                    expected.at("meshes")[0].at("primitives")[0].at("extras"),
            "Opaque authored mesh and primitive extras retain exact JSON "
            "values on every current instance without replacing target names");
      check(actual["meta"]["name"] ==
                    expected["extensions"]["VRMC_vrm"]["meta"]["name"] &&
                actual["meta"]["authors"] ==
                    expected["extensions"]["VRMC_vrm"]["meta"]["authors"] &&
                actual["lookAt"] ==
                    expected["extensions"]["VRMC_vrm"]["lookAt"],
            "Actual author/license/look-at payloads retain version-specific "
            "source meaning");
      for (const auto &bone : actual["humanoid"]["humanBones"].items()) {
        auto source_node = expected["extensions"]["VRMC_vrm"]["humanoid"]
                                   ["humanBones"][bone.key()]["node"]
                                       .get<size_t>();
        auto current = bone.value()["node"].get<size_t>();
        check(emitted["nodes"][current]["name"] ==
                  expected["nodes"][source_node]["name"],
              "V1 humanoid object/thumb role references map to their actual "
              "original nodes");
      }
      for (const auto &annotation : actual["firstPerson"]["meshAnnotations"])
        check(
            emitted["nodes"][annotation["node"].get<size_t>()].contains("mesh"),
            "V1 first-person binds real emitted mesh nodes");
      const auto &expression = actual["expressions"]["preset"]["happy"];
      check(expression["isBinary"] == true &&
                expression["overrideBlink"] == "blend" &&
                expression["morphTargetBinds"].size() == 1 &&
                expression["materialColorBinds"].size() == 3 &&
                expression["textureTransformBinds"].size() == 3,
            "V1 expressions expand material forks without multiplying "
            "node-specific morph binds");
      for (const auto &bind : expression["materialColorBinds"])
        check(bind["targetValue"] == J({.4f, .5f, .6f, .7f}),
              "V1 absolute material target RGBA remains exact");
      const auto &spring = emitted["extensions"]["VRMC_springBone"];
      check(spring["specVersion"] == "1.0" &&
                spring["springs"][0]["joints"].size() == 2,
            "Typed spring version and explicit joint order survive emission");
      for (size_t k = 0; k < 2; ++k) {
        auto n = spring["springs"][0]["joints"][k]["node"].get<size_t>();
        check(emitted["nodes"][n]["name"] ==
                  (k ? "Actual spring tail" : "Actual spring root"),
              "Spring joints retain exact source-node identities and order");
      }
      size_t constraints = 0;
      for (const auto &node : emitted["nodes"])
        if (node.contains("extensions") &&
            node["extensions"].contains("VRMC_node_constraint")) {
          ++constraints;
          for (const auto &kind :
               node["extensions"]["VRMC_node_constraint"]["constraint"]
                   .items()) {
            const auto n = kind.value()["source"].get<size_t>();
            check(emitted["nodes"][n]["name"] ==
                      (kind.key() == "rotation"
                           ? "Original unweighted hair joint 129"
                           : "head"),
                  "Roll/aim/rotation constraints reference the actual emitted "
                  "source endpoint");
          }
        }
      check(constraints == 3, "All three real constraint kinds survive export");
      check(imported.scene.groups().size() < 128 &&
                imported.scene.object(imported.objects.front())
                        .appearance->asset->nodes.size() > 128,
            "Full original joint/reference graph survives without expanding "
            "the editable Scene group cap");
      for (size_t i = 0; i < 130; ++i)
        check(std::any_of(emitted["nodes"].begin(), emitted["nodes"].end(),
                          [&](const auto &node) {
                            return node.value("name", std::string{}) ==
                                   "Original unweighted hair joint " +
                                       std::to_string(i);
                          }),
              "Every unweighted original joint outside mesh-ancestor closure "
              "survives source export");
      check(imported.scene.object(imported.objects.front())
                        .appearance->asset->avatar_version ==
                    ImportedAvatarVersion::Vrm1 &&
                Scene::load(project)
                        .object(imported.objects.front())
                        .appearance->asset->avatar_version ==
                    ImportedAvatarVersion::Vrm1,
            "Derived V1 kind comes from actual immutable metadata and is "
            "recomputed on native replay");
      for (int invalid = 0; invalid < 6; ++invalid) {
        auto bad = fixture;
        if (invalid == 0)
          bad.j["extensions"]["VRMC_springBone"]["springs"][0]["joints"][1]
               ["node"] = 0;
        if (invalid == 1)
          for (size_t n = 0; n < bad.j["nodes"].size(); ++n)
            if (bad.j["nodes"][n].value("name", std::string{}) ==
                "Actual rotation constraint")
              bad.j["nodes"][n]["extensions"]["VRMC_node_constraint"]
                   ["constraint"]["rotation"]["source"] = n;
        if (invalid == 2)
          bad.j["extensions"]["VRMC_vrm"]["expressions"]["preset"]["happy"]
               ["morphTargetBinds"][0]["index"] = 99;
        if (invalid == 3)
          bad.j["extensions"]["VRMC_springBone"]["specVersion"] = "1.1";
        if (invalid == 4)
          bad.j["extensions"]["VRMC_vrm"]["specVersion"] = "1.1";
        if (invalid == 5)
          bad.j["extensions"]["VRMC_vrm"]["humanoid"]["humanBones"]["head"]
               ["node"] = 100000;
        const auto file = output.parent_path() /
                          ("invalid" + std::to_string(invalid) + ".vrm");
        bad.save(file);
        bool refused = false;
        try {
          (void)prepare_authoring_import(imported.scene, file, options);
        } catch (const std::exception &) {
          refused = true;
        }
        check(refused && imported.scene.encode() == before,
              "Malformed spring order/constraint ownership/morph "
              "identity/version refuses atomically");
      }
      for (int unsupported = 0; unsupported < 5; ++unsupported) {
        auto unrepresented = metadata_source();
        J opaque = {{"sourceNode", 0}};
        if (unsupported == 0)
          unrepresented.j["extensions"]["DOME_unrepresented"] = opaque;
        if (unsupported == 1)
          unrepresented.j["nodes"][0]["extensions"]["DOME_unrepresented"] =
              opaque;
        if (unsupported == 2)
          unrepresented.j["meshes"][0]["extensions"]["DOME_unrepresented"] =
              opaque;
        if (unsupported == 3)
          unrepresented.j["meshes"][0]["primitives"][0]["extensions"]
                         ["DOME_unrepresented"] = opaque;
        if (unsupported == 4)
          unrepresented.j["scenes"].push_back({{"nodes", {0}}});
        const auto file =
            output.parent_path() /
            ("unrepresented" + std::to_string(unsupported) + ".vrm");
        unrepresented.save(file);
        auto retained = prepare_authoring_import(Scene{}, file, options);
        auto exact = retained.scene.encode();
        const auto previous = bytes(output);
        bool refused = false;
        try {
          (void)export_vrm(retained.scene.preview_surfaces(), output);
        } catch (const std::invalid_argument &) {
          refused = true;
        }
        check(refused && bytes(output) == previous &&
                  retained.scene.encode() == exact,
              "Unrepresented actual extension/reference or multi-scene state "
              "refuses without losing raw source or replacing prior output");
      }
      import_fixture::Fixture unused;
      unused.j["extensionsUsed"].push_back("VRMC_vrm");
      const auto unused_path = output.parent_path() / "unused-tag.glb";
      unused.save(unused_path);
      auto ordinary = prepare_authoring_import(Scene{}, unused_path, options);
      check(ordinary.scene.object(ordinary.objects.front())
                    .appearance->asset->avatar_version ==
                ImportedAvatarVersion::None,
            "A mere unused extensionsUsed declaration creates no avatar "
            "metadata authority");
      Document ordinary_doc;
      ordinary_doc.replace(ordinary.scene);
      check(
          !ordinary_doc.frame().state->can_export_avatar,
          "Ordinary glTF with an unused VRM tag does not enable avatar export");
      const auto ordinary_path = output.parent_path() / "ordinary.dsculpt";
      ordinary.scene.save(ordinary_path);
      check(Scene::load(ordinary_path)
                    .object(ordinary.objects.front())
                    .appearance->asset->avatar_version ==
                ImportedAvatarVersion::None,
            "Unused-tag None kind survives real archive replay without "
            "namespace inference");
      auto invalid_meta = metadata_source();
      invalid_meta.j["extensions"]["VRMC_vrm"]["meta"]["references"] =
          J::array();
      const auto invalid_meta_path =
          output.parent_path() / "preserved-invalid-meta.vrm";
      invalid_meta.save(invalid_meta_path);
      auto retained_meta =
          prepare_authoring_import(Scene{}, invalid_meta_path, options);
      auto prior_output = bytes(output);
      bool meta_refused = false;
      try {
        (void)export_vrm(retained_meta.scene.preview_surfaces(), output);
      } catch (const std::invalid_argument &e) {
        meta_refused =
            std::string_view(e.what()).find("empty optional meta.references") !=
            std::string_view::npos;
      }
      check(meta_refused && bytes(output) == prior_output &&
                *retained_meta.scene.object(retained_meta.objects.front())
                        .appearance->original_bytes == invalid_meta.bytes(),
            "Schema-invalid empty reference list stays exact in native source "
            "while conforming export refuses atomically");
      auto atlas_options = options;
      atlas_options.prepare_texture_atlas = true;
      auto atlas = prepare_authoring_import(Scene{}, input, atlas_options);
      bool atlas_refused = false;
      try {
        (void)export_vrm(atlas.scene.preview_surfaces(), output);
      } catch (const std::invalid_argument &e) {
        atlas_refused = std::string_view(e.what()).find("UV expression") !=
                        std::string_view::npos;
      }
      check(atlas_refused && bytes(output) == prior_output &&
                imported.scene.encode() == before,
            "Real editable-atlas preparation cannot silently reinterpret "
            "original V1 UV-expression targets");
      Document doc;
      auto baseline = doc.copy_scene().encode();
      ImportSettings settings;
      settings.paint_resolution = 64;
      doc.import_asset(input, settings);
      auto saved = doc.save_async(output.parent_path() / "worker.dsculpt");
      saved.get();
      check(doc.wait_asset() && doc.frame().state->error.empty() &&
                doc.copy_scene().encode() == before,
            "Actual native import plus immediate ordered Save preserves exact "
            "V1 source");
      check(doc.frame().state->can_export_avatar,
            "Existing native avatar export command recognizes authenticated V1 "
            "source groups");
      auto future = doc.export_asset(output.parent_path() / "worker.vrm",
                                     AssetExportFormat::Avatar);
      future.get();
      check(doc.copy_scene().encode() == before,
            "Actual accepted native export leaves Scene and immutable source "
            "unchanged");
      check(doc.undo() && doc.copy_scene().encode() == baseline,
            "Native V1 admission is one complete global Undo");
      check(doc.redo() && doc.copy_scene().encode() == before,
            "Native V1 Redo retains every source/typed reference");
      auto posed_source = metadata_source();
      posed_source.j["extensions"].erase("VRMC_springBone");
      for (auto &node : posed_source.j["nodes"])
        if (node.contains("extensions")) {
          node["extensions"].erase("VRMC_node_constraint");
          if (node["extensions"].empty())
            node.erase("extensions");
        }
      auto &used = posed_source.j["extensionsUsed"];
      for (auto at = used.begin(); at != used.end();)
        if (*at == "VRMC_springBone" || *at == "VRMC_node_constraint")
          at = used.erase(at);
        else
          ++at;
      const auto posed_input = output.parent_path() / "kinematic-source.vrm";
      posed_source.save(posed_input);
      Document kinematic;
      kinematic.import_asset(posed_input, settings);
      check(kinematic.wait_asset() && kinematic.frame().state->error.empty(),
            "A separate genuine V1 source declares kinematic authoring without "
            "physics constraints");
      auto resting = kinematic.copy_scene();
      uint64_t active = 0;
      for (const auto &o : resting.objects())
        if (o.deformation && o.deformation->skin) {
          active = o.id;
          break;
        }
      check(active != 0,
            "Kinematic V1 fixture uses a real nonzero weighted source skin");
      kinematic.select_object(active);
      resting = kinematic.copy_scene();
      kinematic.edit_scene(
          "Pose actual V1 source",
          [active](Scene &scene, const std::function<bool()> &cancelled) {
            RigEdit edit;
            edit.joint = JointAdjustment{
                scene.object(active).deformation->humanoid->joints.at("head"),
                {0, 0, 13},
                {}};
            edit_rig(scene, active, edit, cancelled);
          });
      auto pose_saved =
          kinematic.save_async(output.parent_path() / "kinematic.dsculpt");
      pose_saved.get();
      check(kinematic.wait_asset() && kinematic.frame().state->error.empty(),
            "Native ordered worker transports one current source pose across "
            "distinct skins and a genuine rigid descendant");
      auto current = kinematic.copy_scene();
      check(current.encode() != resting.encode() &&
                Scene::load(output.parent_path() / "kinematic.dsculpt")
                        .encode() == current.encode(),
            "Current linked V1 pose and rigid compensation persist exactly "
            "before export");
      for (const auto &o : resting.objects())
        if (o.appearance)
          check(current.object(o.id).appearance == o.appearance &&
                    current.object(o.id).painted->mesh_owner() ==
                        o.painted->mesh_owner() &&
                    current.object(o.id).painted->encode() ==
                        o.painted->encode(),
                "Current source posing retains exact "
                "mesh/paint/raw-image/material/metadata owners");
      auto pose_export = kinematic.export_asset(
          output.parent_path() / "kinematic.vrm", AssetExportFormat::Avatar);
      pose_export.get();
      independent(current, output.parent_path() / "kinematic.vrm");
      check(kinematic.undo() &&
                kinematic.copy_scene().encode() == resting.encode() &&
                kinematic.redo() &&
                kinematic.copy_scene().encode() == current.encode(),
            "Linked kinematic V1 pose remains one exact native Undo/Redo");
    }
    return argc > 3 ? 1 : 0;
  } catch (const std::exception &e) {
    std::cerr << "VRM1 export refusal: " << e.what() << '\n';
    if (argc > 3 &&
        std::string_view(e.what()).find("only original VRM0") !=
            std::string_view::npos &&
        imported.scene.encode() == before)
      return 0;
    return 1;
  }
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
