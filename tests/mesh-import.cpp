// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import.hpp"
#include "dome/mesh-processing.hpp"
#include "dome/scene.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
using Json = nlohmann::json;
namespace fs = std::filesystem;
void require(bool v, const char *s) {
  if (!v)
    throw std::runtime_error(s);
  std::cout << "PASS " << s << '\n';
}
void save(const fs::path &p, const std::string &s) {
  std::ofstream(p, std::ios::binary) << s;
}
template <class F> void rejects(F f, const char *s) {
  bool bad = false;
  try {
    f();
  } catch (const std::exception &) {
    bad = true;
  }
  require(bad, s);
}
std::string b64(const std::vector<uint8_t> &bytes) {
  const char *alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0; i < bytes.size(); i += 3) {
    uint32_t n = uint32_t(bytes[i]) << 16;
    if (i + 1 < bytes.size())
      n |= uint32_t(bytes[i + 1]) << 8;
    if (i + 2 < bytes.size())
      n |= bytes[i + 2];
    out += alphabet[(n >> 18) & 63];
    out += alphabet[(n >> 12) & 63];
    out += i + 1 < bytes.size() ? alphabet[(n >> 6) & 63] : '=';
    out += i + 2 < bytes.size() ? alphabet[n & 63] : '=';
  }
  return out;
}
void word(std::vector<uint8_t> &b, uint32_t n, int bytes = 4) {
  for (int i = 0; i < bytes; i++)
    b.push_back(n >> (i * 8));
}
void floats(std::vector<uint8_t> &b, std::initializer_list<float> v) {
  for (float f : v)
    word(b, std::bit_cast<uint32_t>(f));
}
int main() try {
  auto root =
      fs::temp_directory_path() / ("dome-import-" + std::to_string(getpid()));
  fs::create_directories(root / "asset/materials");
  fs::create_directories(root / "asset/textures");
  auto asset = root / "asset";
  std::string image = "not-decoded-by-importer";
  save(asset / "textures/albedo.png", image);
  save(asset / "materials/colors.mtl",
       "newmtl Red\nKd 0.8 0.1 0.2\nNs 18\nmap_Kd "
       "../textures/albedo.png\nnewmtl Blue\nKd 0.1 0.2 0.9\n");
  save(asset / "concave.obj",
       "mtllib materials/colors.mtl\no Concave\nv 0 0 0\nv 2 0 0\nv 2 2 0\nv 1 "
       "1 0\nv 0 2 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt .5 .5\nvt 0 1\nvn 0 0 "
       "1\nusemtl Red\nf -5/1/1 -4/2/1 -3/3/1 -2/4/1 -1/5/1\ng Other\nusemtl "
       "Blue\nf 1/1/1 4/4/1 5/5/1\n");
  auto obj = import_mesh(asset / "concave.obj");
  require(obj.primitives.size() == 2 &&
              obj.primitives[0].mesh.indices.size() == 9,
          "OBJ concave polygons triangulate and retain groups");
  double area = 0;
  auto &m = obj.primitives[0].mesh;
  for (size_t i = 0; i < m.indices.size(); i += 3)
    area += length(cross(
                m.positions[m.indices[i + 1]] - m.positions[m.indices[i]],
                m.positions[m.indices[i + 2]] - m.positions[m.indices[i]])) *
            .5;
  require(std::abs(area - 3) < 1e-6,
          "Concave OBJ triangulation preserves the actual polygon area");
  require(obj.materials[obj.primitives[0].material].color[0] == .8f &&
              obj.materials[obj.primitives[1].material].color[2] == .9f,
          "OBJ material assignments and MTL colors remain distinct");
  require(
      obj.images.size() == 1 &&
          std::string(obj.images[0].bytes.begin(), obj.images[0].bytes.end()) ==
              image &&
          obj.images[0].source_uri == "textures/albedo.png",
      "Nested MTL textures retain encoded bytes and asset-relative provenance");
  ImportLimits locked;
  locked.external_resources = false;
  rejects([&] { import_mesh(asset / "concave.obj", locked); },
          "External resource opt-out rejects companion MTL reads");
  save(root / "outside.mtl", "newmtl Private\n");
  save(asset / "escape.obj", "mtllib ../outside.mtl\nv 0 0 0\n");
  rejects([&] { import_mesh(asset / "escape.obj"); },
          "OBJ resources cannot escape the asset directory");
  save(asset / "bad.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 9\n");
  rejects([&] { import_mesh(asset / "bad.obj"); },
          "OBJ invalid face indexes reject before mesh publication");
  save(asset / "bad.obj", "v 0 0 0\nv 1 1 0\nv 0 1 0\nv 1 0 0\nf 1 2 3 4\n");
  rejects([&] { import_mesh(asset / "bad.obj"); },
          "Self-crossing OBJ polygons are rejected");
  ImportLimits tiny;
  tiny.max_vertices = 3;
  rejects([&] { import_mesh(asset / "concave.obj", tiny); },
          "Expanded OBJ vertices obey import limits");
  ImportLimits cancelled;
  cancelled.cancelled = [] { return true; };
  rejects([&] { import_mesh(asset / "concave.obj", cancelled); },
          "Import cancellation stops work before publication");
  std::vector<uint8_t> b;
  floats(b, {0, 0, 0, 1, 0, 0, 0, 1, 0});
  word(b, 0, 2);
  word(b, 1, 2);
  word(b, 2, 2);
  for (int i = 0; i < 3; i++) {
    word(b, 0, 2);
    word(b, 0, 2);
    word(b, 32767, 2);
  }
  floats(b, {0, 0, 1, 0, 0, 1});
  for (int i = 0; i < 3; i++) {
    b.push_back(255);
    b.push_back(128);
    b.push_back(0);
    b.push_back(255);
  }
  for (int i = 0; i < 3; i++)
    floats(b, {1, 0, 0, 1});
  require(b.size() == 144,
          "glTF typed/interleaved fixture has exact byte layout");
  Json gltf = {
      {"asset", {{"version", "2.0"}}},
      {"buffers",
       Json::array(
           {{{"byteLength", b.size()},
             {"uri", "data:application/octet-stream;base64," + b64(b)}}})},
      {"bufferViews",
       Json::array({{{"buffer", 0}, {"byteOffset", 0}, {"byteLength", 36}},
                    {{"buffer", 0}, {"byteOffset", 36}, {"byteLength", 6}},
                    {{"buffer", 0}, {"byteOffset", 42}, {"byteLength", 18}},
                    {{"buffer", 0}, {"byteOffset", 60}, {"byteLength", 24}},
                    {{"buffer", 0}, {"byteOffset", 84}, {"byteLength", 12}},
                    {{"buffer", 0}, {"byteOffset", 96}, {"byteLength", 48}}})},
      {"accessors", Json::array({{{"bufferView", 0},
                                  {"componentType", 5126},
                                  {"count", 3},
                                  {"type", "VEC3"}},
                                 {{"bufferView", 1},
                                  {"componentType", 5123},
                                  {"count", 3},
                                  {"type", "SCALAR"}},
                                 {{"bufferView", 2},
                                  {"componentType", 5122},
                                  {"normalized", true},
                                  {"count", 3},
                                  {"type", "VEC3"}},
                                 {{"bufferView", 3},
                                  {"componentType", 5126},
                                  {"count", 3},
                                  {"type", "VEC2"}},
                                 {{"bufferView", 4},
                                  {"componentType", 5121},
                                  {"normalized", true},
                                  {"count", 3},
                                  {"type", "VEC4"}},
                                 {{"bufferView", 5},
                                  {"componentType", 5126},
                                  {"count", 3},
                                  {"type", "VEC4"}}})},
      {"images", Json::array({{{"uri", "data:image/png;base64,AQIDBA=="}}})},
      {"textures", Json::array({{{"source", 0}}})},
      {"materials",
       Json::array({{{"name", "Paint"},
                     {"pbrMetallicRoughness",
                      {{"baseColorFactor", {.4, .5, .6, .7}},
                       {"metallicFactor", .2},
                       {"roughnessFactor", .8},
                       {"baseColorTexture", {{"index", 0}, {"texCoord", 1}}}}},
                     {"alphaMode", "BLEND"},
                     {"doubleSided", true}}})},
      {"meshes",
       Json::array({{{"primitives", Json::array({{{"attributes",
                                                   {{"POSITION", 0},
                                                    {"NORMAL", 2},
                                                    {"TEXCOORD_0", 3},
                                                    {"TEXCOORD_1", 3},
                                                    {"COLOR_0", 4},
                                                    {"TANGENT", 5}}},
                                                  {"indices", 1},
                                                  {"material", 0}}})}}})},
      {"nodes",
       Json::array(
           {{{"name", "Parent"}, {"translation", {1, 2, 3}}, {"children", {1}}},
            {{"name", "Child"}, {"mesh", 0}, {"scale", {-2, 1, .5}}}})},
      {"scenes", Json::array({{{"nodes", {0}}}})},
      {"scene", 0}};
  auto path = asset / "typed.gltf";
  save(path, gltf.dump());
  auto imported = import_mesh(path);
  require(imported.primitives.size() == 1,
          "Native glTF static triangle import succeeds");
  auto &p = imported.primitives[0];
  require(p.mesh.positions[0].x == 1 && p.mesh.positions[0].y == 2 &&
              p.mesh.positions[1].x == -1 && p.mesh.positions[2].y == 3 &&
              p.source_world[12] == 1,
          "glTF hierarchical node transforms bake correctly and retain source "
          "matrix");
  require(
      p.mesh.indices == std::vector<uint32_t>({0, 2, 1}) &&
          p.mesh.normals[0].z > .99f && p.tangents[0][0] < -.99f &&
          p.tangents[0][3] == -1,
      "Mirrored nonuniform transforms preserve normal and tangent orientation");
  require(p.mesh.uv.size() == 3 && p.uv1.size() == 3 && p.colors.size() == 3 &&
              std::abs(p.colors[0][1] - 128.f / 255) < 1e-6f,
          "glTF normalized colors and both UV sets are preserved");
  auto material = imported.materials[p.material];
  require(material.color_texcoord == 1 && material.color_texture == 0 &&
              material.double_sided &&
              material.alpha == ImportedMaterial::AlphaMode::Blend &&
              imported.images[0].bytes == std::vector<uint8_t>({1, 2, 3, 4}),
          "glTF material texture coordinate sets, alpha mode and encoded image "
          "bytes survive");
  auto mutate = [&](Json j, const char *message) {
    save(asset / "invalid.gltf", j.dump());
    rejects([&] { import_mesh(asset / "invalid.gltf"); }, message);
  };
  auto invalid = gltf;
  invalid["accessors"][0]["count"] = 0;
  mutate(invalid,
         "Zero-count accessors reject the known cgltf1.15 overflow edge case");
  invalid = gltf;
  invalid["accessors"][0]["byteOffset"] = SIZE_MAX - 8;
  mutate(invalid,
         "Accessor offset arithmetic cannot overflow buffer validation");
  invalid = gltf;
  invalid["bufferViews"][0]["byteStride"] = 4;
  mutate(invalid, "Undersized attribute strides are rejected");
  invalid = gltf;
  invalid["extensionsRequired"] = {"KHR_draco_mesh_compression"};
  mutate(invalid,
         "Unsupported required glTF extensions cannot silently disappear");
  invalid = gltf;
  invalid["meshes"][0]["primitives"][0]["targets"] =
      Json::array({{{"POSITION", 0}}});
  mutate(invalid, "Morph target primitives require a dedicated import path");
  invalid = gltf;
  invalid["accessors"][0]["byteOffset"] = 1;
  mutate(invalid,
         "Misaligned floating-point accessors are rejected before reads");
  invalid = gltf;
  invalid["meshes"][0]["primitives"][0]["attributes"]["COLOR_1"] = 3;
  mutate(invalid, "Unsupported vertex attribute sets cannot be dropped");
  rejects(
      [&] {
        ImportLimits limits;
        limits.max_file_bytes = fs::file_size(path) + 1;
        limits.max_resource_bytes = limits.max_file_bytes;
        import_mesh(path, limits);
      },
      "Parser and resource quota prevents oversized asset allocation");
  auto sparse = gltf;
  auto sparse_bytes = b;
  sparse_bytes.insert(sparse_bytes.end(), {0, 0, 0, 0});
  floats(sparse_bytes, {.25f, 0, 0});
  sparse["buffers"][0]["byteLength"] = sparse_bytes.size();
  sparse["buffers"][0]["uri"] =
      "data:application/octet-stream;base64," + b64(sparse_bytes);
  sparse["bufferViews"].push_back(
      {{"buffer", 0}, {"byteOffset", 144}, {"byteLength", 1}});
  sparse["bufferViews"].push_back(
      {{"buffer", 0}, {"byteOffset", 148}, {"byteLength", 12}});
  sparse["accessors"][0]["sparse"] = {
      {"count", 1},
      {"indices", {{"bufferView", 6}, {"componentType", 5121}}},
      {"values", {{"bufferView", 7}}}};
  save(asset / "sparse.gltf", sparse.dump());
  require(
      import_mesh(asset / "sparse.gltf").primitives[0].mesh.positions[0].x ==
          .5f,
      "Sparse floating-point attributes override the base accessor");
  sparse_bytes[144] = 3;
  sparse["buffers"][0]["uri"] =
      "data:application/octet-stream;base64," + b64(sparse_bytes);
  mutate(sparse,
         "Out-of-range sparse indices are rejected before typed access");
  auto external = gltf;
  save(asset / "buffer data.bin", std::string(b.begin(), b.end()));
  external["buffers"][0]["uri"] = "buffer%20data.bin";
  save(path, external.dump());
  require(import_mesh(path).primitives[0].mesh.positions[1].x == -1,
          "Percent-escaped external glTF buffer names resolve under the asset "
          "directory");
  external["buffers"][0]["uri"] = "%2e%2e/outside.bin";
  mutate(external, "Encoded glTF resource traversal is rejected");
  Mesh triangle;
  triangle.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  triangle.normals.assign(3, {0, 0, 1});
  triangle.uv = {{{0, 0}}, {{1, 0}}, {{0, 1}}};
  triangle.indices = {0, 1, 2};
  export_glb(triangle, asset / "roundtrip.glb");
  auto roundtrip = import_mesh(asset / "roundtrip.glb");
  require(roundtrip.primitives.size() == 1 &&
              roundtrip.primitives[0].mesh.indices == triangle.indices &&
              roundtrip.primitives[0].mesh.uv == triangle.uv,
          "Actual native GLB export reimports with triangle and UV identity");

  ImportLimits preserve;
  preserve.mode = ImportMode::PreserveGltf;
  save(asset / "local.gltf", gltf.dump());
  auto local = import_mesh(asset / "local.gltf", preserve);
  require(
      local.nodes.size() == 2 && local.meshes.size() == 1 &&
          local.scenes.size() == 1 && local.default_scene == 0 &&
          local.nodes[1].parent == 0 &&
          local.nodes[0].children == std::vector<uint32_t>{1} &&
          local.nodes[1].mesh == 0,
      "Preserving glTF import retains complete scene, node and mesh identity");
  require(local.primitives[0].mesh_local &&
              local.primitives[0].mesh.positions[1].x == 1 &&
              local.primitives[0].mesh.indices ==
                  std::vector<uint32_t>({0, 1, 2}) &&
              local.primitives[0].tangents[0][3] == 1 &&
              local.nodes[1].world[0] == -2 && local.nodes[1].world[12] == 1 &&
              local.nodes[1].has_scale,
          "Mesh-local import retains reflected instance transforms without "
          "baking geometry or tangent handedness");
  auto instanced = gltf;
  instanced["nodes"].push_back(
      {{"mesh", 0},
       {"matrix", {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 7, 8, 9, 1}}});
  instanced["scenes"][0]["nodes"].push_back(2);
  save(asset / "instances.gltf", instanced.dump());
  auto instances = import_mesh(asset / "instances.gltf", preserve);
  require(instances.primitives.size() == 1 && instances.nodes.size() == 3 &&
              instances.nodes[2].mesh == 0 && instances.nodes[2].has_matrix &&
              !instances.nodes[2].has_translation &&
              instances.nodes[2].local[12] == 7,
          "Shared mesh instances preserve authored matrix-versus-TRS state "
          "without duplicating mesh data");

  auto morph_json = gltf;
  auto morph_binary = b;
  auto append_morph = [&](std::initializer_list<float> values) {
    auto view = morph_json["bufferViews"].size(),
         accessor = morph_json["accessors"].size();
    size_t offset = morph_binary.size();
    floats(morph_binary, values);
    morph_json["bufferViews"].push_back({{"buffer", 0},
                                         {"byteOffset", offset},
                                         {"byteLength", values.size() * 4}});
    morph_json["accessors"].push_back({{"bufferView", view},
                                       {"componentType", 5126},
                                       {"count", 3},
                                       {"type", "VEC3"}});
    return accessor;
  };
  auto mp = append_morph({.1f, 0, 0, 0, .2f, 0, 0, 0, .3f}),
       mn = append_morph({0, .01f, 0, 0, .02f, 0, 0, .03f, 0}),
       mt = append_morph({.01f, 0, 0, .02f, 0, 0, .03f, 0, 0});
  morph_json["buffers"][0]["uri"] =
      "data:application/octet-stream;base64," + b64(morph_binary);
  morph_json["buffers"][0]["byteLength"] = morph_binary.size();
  morph_json["meshes"][0]["primitives"][0]["targets"] =
      Json::array({{{"POSITION", mp}, {"NORMAL", mn}, {"TANGENT", mt}}});
  morph_json["meshes"][0]["weights"] = {-.5f};
  morph_json["meshes"][0]["extras"]["targetNames"] = {"squash"};
  morph_json["nodes"][1]["weights"] = {1.25f};
  auto unmorphed = morph_json["meshes"][0]["primitives"][0];
  unmorphed.erase("targets");
  morph_json["meshes"][0]["primitives"].push_back(unmorphed);
  save(asset / "morph-channels.gltf", morph_json.dump());
  auto channels = import_mesh(asset / "morph-channels.gltf", preserve);
  require(channels.primitives.size() == 2 &&
              channels.primitives[0].morph_targets[0].positions[0].x == .1f &&
              channels.primitives[0].morph_targets[0].normals[2].y == .03f &&
              channels.primitives[0].morph_targets[0].tangents[1].x == .02f,
          "All authored position, normal and three-component tangent morph "
          "differences remain in mesh space");
  require(channels.meshes[0].weights == std::vector<float>{-.5f} &&
              channels.nodes[1].weights == std::vector<float>{1.25f} &&
              channels.meshes[0].target_names ==
                  std::vector<std::string>{"squash"},
          "Mesh defaults and overriding node morph weights retain authored "
          "values outside zero-to-one");
  require(channels.primitives[0].morph_targets_authored &&
              !channels.primitives[1].morph_targets_authored &&
              channels.primitives[1].morph_targets.size() == 1 &&
              channels.primitives[1].morph_targets[0].positions.empty(),
          "Mixed morphed and unmorphed primitives preserve explicit versus "
          "implicit zero-target presence");
  auto reject_morph = [&](Json j, const char *message) {
    save(asset / "invalid-morph.gltf", j.dump());
    rejects([&] { import_mesh(asset / "invalid-morph.gltf", preserve); },
            message);
  };
  auto bad_morph = morph_json;
  bad_morph["meshes"][0]["primitives"][0]["attributes"].erase("NORMAL");
  reject_morph(bad_morph,
               "Morph channels require their matching base vertex attributes");
  bad_morph = morph_json;
  bad_morph["meshes"][0]["primitives"][0]["targets"][0]["TANGENT"] = 5;
  reject_morph(bad_morph, "Morph tangent differences require VEC3 rather than "
                          "tangent-handedness VEC4");
  bad_morph = morph_json;
  bad_morph["meshes"][0]["primitives"][0]["targets"][0]["TEXCOORD_0"] = 3;
  reject_morph(bad_morph, "Optional UV morph channels reject explicitly until "
                          "their import path exists");
  bad_morph = morph_json;
  bad_morph["nodes"][1]["weights"] = {0, 1};
  reject_morph(bad_morph,
               "Node morph override count must match its source mesh targets");
  auto sparse_morph = morph_json;
  auto sparse_morph_bytes = morph_binary;
  auto index_view = sparse_morph["bufferViews"].size();
  sparse_morph["bufferViews"].push_back(
      {{"buffer", 0},
       {"byteOffset", sparse_morph_bytes.size()},
       {"byteLength", 1}});
  sparse_morph_bytes.push_back(0);
  auto position_view = sparse_morph["accessors"][mp]["bufferView"];
  sparse_morph["accessors"][mp].erase("bufferView");
  sparse_morph["accessors"][mp]["sparse"] = {
      {"count", 1},
      {"indices", {{"bufferView", index_view}, {"componentType", 5121}}},
      {"values", {{"bufferView", position_view}}}};
  sparse_morph["buffers"][0]["uri"] =
      "data:application/octet-stream;base64," + b64(sparse_morph_bytes);
  sparse_morph["buffers"][0]["byteLength"] = sparse_morph_bytes.size();
  save(asset / "sparse-morph.gltf", sparse_morph.dump());
  auto sparse_channels = import_mesh(asset / "sparse-morph.gltf", preserve);
  require(
      sparse_channels.primitives[0].morph_targets[0].positions[0].x == .1f &&
          length(sparse_channels.primitives[0].morph_targets[0].positions[2]) ==
              0,
      "Sparse morph differences decode authored values over implicit all-zero "
      "base data");
  auto metadata_limits = preserve;
  metadata_limits.max_metadata_visits = 1;
  rejects([&] { import_mesh(asset / "local.gltf", metadata_limits); },
          "Hierarchy metadata traversal has an explicit work budget");

  DeformationState rig(triangle);
  rig.skeleton = Skeleton{};
  for (int i = 0; i < 5; i++) {
    Bone bone;
    bone.name = "joint" + std::to_string(i);
    bone.parent = i ? 0 : -1;
    bone.rest.translation = {float(i) * .03f, .1f, 0};
    rig.skeleton->bones.push_back(bone);
  }
  rig.skin = SkinWeights{};
  rig.skin->vertices.assign(3,
                            {{0, .2f}, {1, .2f}, {2, .2f}, {3, .2f}, {4, .2f}});
  rig.morphs = MorphSet(triangle);
  auto morph_positions = triangle.positions;
  for (auto &v : morph_positions)
    v.z += .04f;
  rig.morphs->set_target(triangle, "forward", morph_positions);
  rig.morph_weights = {.6f};
  for (auto &bone : rig.skeleton->bones)
    rig.pose.push_back(bone.rest);
  rig.pose[0].translation.y += .15f;
  rig.expressions = AvatarExpressions{{{"aa", {{"forward", 1}}}}};
  ObjectSurface authored_object;
  authored_object.id = 1;
  authored_object.name = "Imported rig";
  authored_object.transform = {{.3f, .2f, -.1f}, {0, 0, .6f, .8f}, .75f};
  authored_object.mesh = triangle;
  std::vector<ObjectSurface> authored = {authored_object};
  authored[0].deformation = std::make_shared<const DeformationState>(rig);
  export_glb(authored, asset / "native-rig.glb");
  rejects([&] { import_mesh(asset / "native-rig.glb"); },
          "Default static mode keeps explicit authored skin/morph rejection");
  auto imported_rig = import_mesh(asset / "native-rig.glb", preserve);
  auto &rig_primitive = imported_rig.primitives[0];
  require(imported_rig.nodes.size() == 7 && imported_rig.skins.size() == 1 &&
              imported_rig.meshes.size() == 1 &&
              imported_rig.nodes[1].skin == 0 &&
              imported_rig.skins[0].joints.size() == 5 &&
              rig_primitive.mesh.positions[0].z == triangle.positions[0].z,
          "Actual native skin/morph GLB reimports all joint nodes and "
          "untouched rest mesh coordinates");
  require(rig_primitive.influences.size() == 2 &&
              rig_primitive.influences[1].joints[0][0] == 4 &&
              rig_primitive.influences[1].weights[0][0] == .2f &&
              rig_primitive.influences[1].weights[0][1] == 0,
          "All native influence sets and zero-padded slots survive without "
          "truncation or normalization");
  auto native_ibm = rig.skeleton->inverse_bind();
  require(imported_rig.skins[0].inverse_bind_matrices_authored &&
              imported_rig.skins[0].inverse_bind_matrices == native_ibm &&
              std::abs(imported_rig.skins[0].inverse_bind_matrices[0][13] +
                       .1f) < 1e-6f,
          "Authored inverse binds remain exact and distinct from current posed "
          "node transforms");
  require(rig_primitive.morph_targets.size() == 1 &&
              std::abs(rig_primitive.morph_targets[0].positions[0].z - .04f) <
                  1e-6f &&
              imported_rig.meshes[0].weights == std::vector<float>{.6f} &&
              imported_rig.meshes[0].target_names ==
                  std::vector<std::string>{"forward"} &&
              Json::parse(imported_rig.meshes[0].extras_json)
                  .contains("domeSculptExpressions"),
          "Native GLB morph names, authored weights, displacement arrays and "
          "expression extras are retained");
  std::ifstream native_file(asset / "native-rig.glb", std::ios::binary);
  std::vector<uint8_t> native_bytes(
      (std::istreambuf_iterator<char>(native_file)), {});
  auto u32 = [&](size_t i) {
    return uint32_t(native_bytes[i]) | (uint32_t(native_bytes[i + 1]) << 8) |
           (uint32_t(native_bytes[i + 2]) << 16) |
           (uint32_t(native_bytes[i + 3]) << 24);
  };
  size_t json_length = u32(12), binary_header = 20 + json_length,
         binary_length = u32(binary_header);
  Json native_json = Json::parse(native_bytes.begin() + 20,
                                 native_bytes.begin() + 20 + json_length);
  std::vector<uint8_t> native_binary(native_bytes.begin() + binary_header + 8,
                                     native_bytes.begin() + binary_header + 8 +
                                         binary_length);
  auto embedded = [&](Json j, const std::vector<uint8_t> &binary) {
    j["buffers"][0]["uri"] =
        "data:application/octet-stream;base64," + b64(binary);
    j["buffers"][0]["byteLength"] = binary.size();
    return j;
  };
  auto preserve_invalid = [&](Json j, const char *message) {
    save(asset / "invalid-preserve.gltf", j.dump());
    rejects([&] { import_mesh(asset / "invalid-preserve.gltf", preserve); },
            message);
  };
  native_json = embedded(native_json, native_binary);
  auto authored_matrix =
      native_json["skins"][0]["inverseBindMatrices"].get<size_t>();
  auto no_ibm = native_json;
  no_ibm["skins"][0].erase("inverseBindMatrices");
  save(asset / "implicit-bind.gltf", no_ibm.dump());
  auto implicit_bind = import_mesh(asset / "implicit-bind.gltf", preserve);
  require(!implicit_bind.skins[0].inverse_bind_matrices_authored &&
              implicit_bind.skins[0].inverse_bind_matrices.empty(),
          "Absent inverse-bind accessor remains explicit implicit identity "
          "instead of recomputed Skeleton binds");
  auto broken = native_json;
  broken["accessors"][authored_matrix]["count"] = 4;
  preserve_invalid(
      broken,
      "Skin inverse-bind accessors cannot have fewer matrices than joints");
  broken = native_json;
  broken["skins"][0]["joints"][1] = broken["skins"][0]["joints"][0];
  preserve_invalid(broken, "Duplicate skin joint nodes reject before mapping");
  broken = native_json;
  broken["skins"][0]["skeleton"] = 1;
  preserve_invalid(broken,
                   "Authored skin skeleton must be an ancestor of every joint");
  broken = native_json;
  broken["scenes"][0]["nodes"] = {1};
  preserve_invalid(
      broken,
      "Skin joint common root must belong to the skinned instance scene");
  broken = native_json;
  broken["nodes"][0]["matrix"] = {1, 0, 0, 0, 0, 1, 0, 0,
                                  0, 0, 1, 0, 0, 0, 0, 1};
  preserve_invalid(
      broken, "Conflicting node matrix and TRS authoring rejects explicitly");
  auto attributes = native_json["meshes"][0]["primitives"][0]["attributes"];
  auto joints0 = attributes["JOINTS_0"].get<size_t>(),
       weights0 = attributes["WEIGHTS_0"].get<size_t>(),
       weights1 = attributes["WEIGHTS_1"].get<size_t>();
  broken = native_json;
  broken["meshes"][0]["primitives"][0]["attributes"].erase("WEIGHTS_1");
  preserve_invalid(broken, "Every joint set needs its matching weight set");
  broken = native_json;
  auto &bad_attributes = broken["meshes"][0]["primitives"][0]["attributes"];
  bad_attributes["JOINTS_2"] = bad_attributes["JOINTS_1"];
  bad_attributes["WEIGHTS_2"] = bad_attributes["WEIGHTS_1"];
  bad_attributes.erase("JOINTS_1");
  bad_attributes.erase("WEIGHTS_1");
  preserve_invalid(broken,
                   "Influence semantic sets must be consecutive without gaps");
  broken = native_json;
  broken["accessors"][joints0]["normalized"] = true;
  preserve_invalid(broken, "Joint indices cannot use normalized accessors");
  broken = native_json;
  broken["accessors"][weights0]["normalized"] = true;
  preserve_invalid(broken,
                   "Floating-point weights cannot be marked normalized");
  auto accessor_offset = [&](const Json &j, size_t id) {
    auto &a = j["accessors"][id];
    return j["bufferViews"][a["bufferView"].get<size_t>()].value("byteOffset",
                                                                 size_t(0)) +
           a.value("byteOffset", size_t(0));
  };
  auto set_float = [&](std::vector<uint8_t> &bytes, size_t offset,
                       float value) {
    auto n = std::bit_cast<uint32_t>(value);
    for (size_t k = 0; k < 4; k++)
      bytes[offset + k] = uint8_t(n >> (k * 8));
  };
  auto bad_binary = native_binary;
  set_float(bad_binary, accessor_offset(native_json, authored_matrix) + 12,
            .1f);
  preserve_invalid(
      embedded(native_json, bad_binary),
      "Authored joint inverse binds require the affine fourth row");
  bad_binary = native_binary;
  set_float(bad_binary, accessor_offset(native_json, authored_matrix),
            std::numeric_limits<float>::quiet_NaN());
  preserve_invalid(
      embedded(native_json, bad_binary),
      "Nonfinite inverse-bind matrix values reject before publication");
  bad_binary = native_binary;
  bad_binary[accessor_offset(native_json, joints0)] = 5;
  bad_binary[accessor_offset(native_json, joints0) + 1] = 0;
  preserve_invalid(
      embedded(native_json, bad_binary),
      "All influence joint slots are checked against every instancing skin");
  bad_binary = native_binary;
  bad_binary[accessor_offset(native_json, joints0) + 2] = 0;
  bad_binary[accessor_offset(native_json, joints0) + 3] = 0;
  preserve_invalid(embedded(native_json, bad_binary),
                   "Repeated nonzero joint influences reject instead of "
                   "merging authored data");
  bad_binary = native_binary;
  set_float(bad_binary, accessor_offset(native_json, weights0), -.1f);
  preserve_invalid(embedded(native_json, bad_binary),
                   "Negative skin weights reject without normalization");
  auto append_view = [&](Json &j, std::vector<uint8_t> &bytes,
                         const std::vector<uint8_t> &payload,
                         size_t alignment) {
    while (bytes.size() % alignment)
      bytes.push_back(0);
    auto index = j["bufferViews"].size();
    j["bufferViews"].push_back({{"buffer", 0},
                                {"byteOffset", bytes.size()},
                                {"byteLength", payload.size()}});
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return index;
  };
  auto trailing = native_json;
  auto trailing_bytes = native_binary;
  size_t original_ibm = accessor_offset(native_json, authored_matrix);
  std::vector<uint8_t> matrices(native_binary.begin() + original_ibm,
                                native_binary.begin() + original_ibm + 5 * 64);
  floats(matrices, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, .125f, 0, 0, 1});
  auto trailing_view = append_view(trailing, trailing_bytes, matrices, 4);
  trailing["accessors"][authored_matrix]["bufferView"] = trailing_view;
  trailing["accessors"][authored_matrix]["count"] = 6;
  save(asset / "trailing-bind.gltf", embedded(trailing, trailing_bytes).dump());
  auto trailing_bind = import_mesh(asset / "trailing-bind.gltf", preserve);
  require(trailing_bind.skins[0].inverse_bind_matrices.size() == 6 &&
              trailing_bind.skins[0].inverse_bind_matrices[5][12] == .125f,
          "Trailing authored inverse-bind matrices are retained without "
          "truncation");
  auto quantized = native_json;
  auto quantized_bytes = native_binary;
  std::vector<uint8_t> q8(12, 51), q16;
  for (int v = 0; v < 3; v++) {
    word(q16, 13107, 2);
    for (int k = 1; k < 4; k++)
      word(q16, 0, 2);
  }
  auto q8view = append_view(quantized, quantized_bytes, q8, 1),
       q16view = append_view(quantized, quantized_bytes, q16, 2);
  quantized["accessors"][weights0]["bufferView"] = q8view;
  quantized["accessors"][weights0]["componentType"] = 5121;
  quantized["accessors"][weights0]["normalized"] = true;
  quantized["accessors"][weights1]["bufferView"] = q16view;
  quantized["accessors"][weights1]["componentType"] = 5123;
  quantized["accessors"][weights1]["normalized"] = true;
  save(asset / "quantized-skin.gltf",
       embedded(quantized, quantized_bytes).dump());
  auto quantized_skin = import_mesh(asset / "quantized-skin.gltf", preserve);
  require(
      quantized_skin.primitives[0].influences[0].weight_component_type ==
              5121 &&
          quantized_skin.primitives[0].influences[1].weight_component_type ==
              5123 &&
          quantized_skin.primitives[0].influences[0].weights[0][0] ==
              51.f / 255,
      "Mixed normalized byte/short influence sets preserve their component "
      "provenance and values");
  auto invalid_quantized = quantized_bytes;
  invalid_quantized[quantized["bufferViews"][q8view]["byteOffset"]
                        .get<size_t>()] = 50;
  preserve_invalid(embedded(quantized, invalid_quantized),
                   "Normalized integer weight sums are validated exactly "
                   "across all influence sets");

  auto sparse_skin = native_json;
  auto sparse_skin_bytes = native_binary;
  auto sparse_joint_indices =
      append_view(sparse_skin, sparse_skin_bytes, {0, 1, 2}, 1);
  auto weight_offset = accessor_offset(native_json, weights0);
  std::vector<uint8_t> weight_payload(native_binary.begin() + weight_offset,
                                      native_binary.begin() + weight_offset +
                                          3 * 16);
  auto sparse_weight_values =
      append_view(sparse_skin, sparse_skin_bytes, weight_payload, 4);
  sparse_skin["accessors"][weights0].erase("bufferView");
  sparse_skin["accessors"][weights0]["sparse"] = {
      {"count", 3},
      {"indices",
       {{"bufferView", sparse_joint_indices}, {"componentType", 5121}}},
      {"values", {{"bufferView", sparse_weight_values}}}};
  save(asset / "sparse-skin.gltf",
       embedded(sparse_skin, sparse_skin_bytes).dump());
  auto sparse_weights = import_mesh(asset / "sparse-skin.gltf", preserve);
  require(sparse_weights.primitives[0].influences[0].weights[2][3] == .2f,
          "Sparse influence arrays decode all authored vertices without "
          "dropping influence sets");
  auto sparse_index = gltf;
  auto sparse_index_bytes = b;
  auto index_offsets = append_view(sparse_index, sparse_index_bytes, {1, 2}, 1);
  std::vector<uint8_t> index_values;
  word(index_values, 1, 2);
  word(index_values, 2, 2);
  auto index_payload =
      append_view(sparse_index, sparse_index_bytes, index_values, 2);
  sparse_index["accessors"][1].erase("bufferView");
  sparse_index["accessors"][1]["sparse"] = {
      {"count", 2},
      {"indices", {{"bufferView", index_offsets}, {"componentType", 5121}}},
      {"values", {{"bufferView", index_payload}}}};
  save(asset / "sparse-topology.gltf",
       embedded(sparse_index, sparse_index_bytes).dump());
  auto sparse_topology = import_mesh(asset / "sparse-topology.gltf", preserve);
  require(sparse_topology.primitives[0].mesh.indices ==
              std::vector<uint32_t>({0, 1, 2}),
          "Preserving mode decodes sparse triangle indices over their implicit "
          "zero base");
  auto animated = native_json;
  auto animated_bytes = native_binary;
  std::vector<uint8_t> times, positions;
  floats(times, {0, 1});
  floats(positions, {0, 0, 0, 1, 0, 0});
  auto times_view = append_view(animated, animated_bytes, times, 4),
       positions_view = append_view(animated, animated_bytes, positions, 4);
  size_t input_accessor = animated["accessors"].size();
  animated["accessors"].push_back({{"bufferView", times_view},
                                   {"componentType", 5126},
                                   {"count", 2},
                                   {"type", "SCALAR"}});
  size_t output_accessor = animated["accessors"].size();
  animated["accessors"].push_back({{"bufferView", positions_view},
                                   {"componentType", 5126},
                                   {"count", 2},
                                   {"type", "VEC3"}});
  animated["animations"] = Json::array(
      {{{"name", "move"},
        {"samplers", Json::array({{{"input", input_accessor},
                                   {"output", output_accessor},
                                   {"interpolation", "LINEAR"}}})},
        {"channels",
         Json::array(
             {{{"sampler", 0},
               {"target", {{"node", 0}, {"path", "translation"}}}}})}}});
  animated["extensionsUsed"] = {"EXT_not_interpreted"};
  animated["nodes"][0]["extras"] = "quoted \\\" custom text";
  save(asset / "animated-diagnostic.gltf",
       embedded(animated, animated_bytes).dump());
  auto animation = import_mesh(asset / "animated-diagnostic.gltf", preserve);
  require(animation.omitted_animations.size() == 1 &&
              animation.omitted_animations[0].name == "move" &&
              animation.omitted_animations[0].channels == 1 &&
              !animation.warnings.empty(),
          "Animation omission is explicit with retained clip names and channel "
          "counts");
  require(animation.extensions_used ==
                  std::vector<std::string>{"EXT_not_interpreted"} &&
              Json::parse(animation.nodes[0].extras_json) ==
                  animated["nodes"][0]["extras"],
          "Unknown optional extensions remain diagnostic and arbitrary string "
          "extras retain exact JSON meaning");

  auto escaped_names = gltf;
  auto quoted = std::string("root ") + char(34) + " rocket";
  escaped_names["nodes"][0]["name"] = quoted;
  auto escaped_json = escaped_names.dump();
  auto rocket = escaped_json.find("rocket");
  auto escapes = std::string(1, char(92)) + "ud83d" + char(92) + "ude80";
  escaped_json.replace(rocket, 6, escapes);
  save(asset / "unicode-name.gltf", escaped_json);
  auto unicode = import_mesh(asset / "unicode-name.gltf", preserve);
  auto expected_name =
      std::string("root ") + char(34) + " " +
      std::string({char(0xf0), char(0x9f), char(0x9a), char(0x80)});
  require(unicode.nodes[0].name == expected_name,
          "JSON-escaped names decode surrogate pairs without losing stable "
          "authored identities");

  auto escaped_buffer = gltf;
  escaped_buffer["buffers"][0]["uri"] = "buffer data.bin";
  auto escaped_buffer_json = escaped_buffer.dump();
  auto blank = escaped_buffer_json.find("buffer data.bin") + 6;
  escaped_buffer_json.replace(blank, 1, std::string(1, char(92)) + "u0020");
  save(asset / "escaped-buffer.gltf", escaped_buffer_json);
  require(import_mesh(asset / "escaped-buffer.gltf", preserve)
                  .primitives[0]
                  .mesh.positions[1]
                  .x == 1,
          "JSON-escaped external resource URIs decode before confined "
          "filesystem resolution");
  auto authored_float = native_binary;
  set_float(authored_float, accessor_offset(native_json, weights0), .3f);
  save(asset / "authored-float-weight.gltf",
       embedded(native_json, authored_float).dump());
  auto float_weight =
      import_mesh(asset / "authored-float-weight.gltf", preserve);
  require(float_weight.primitives[0].influences[0].weights[0][0] == .3f &&
              std::any_of(
                  float_weight.warnings.begin(), float_weight.warnings.end(),
                  [](const std::string &s) {
                    return s.find("without normalization") != std::string::npos;
                  }),
          "Non-unit authored float weights remain exact with explicit "
          "normalization diagnostics");
  auto zero_weights = native_binary;
  for (auto accessor : {weights0, weights1})
    for (size_t k = 0; k < 4; k++)
      set_float(zero_weights, accessor_offset(native_json, accessor) + k * 4,
                0);
  preserve_invalid(
      embedded(native_json, zero_weights),
      "Every bound skinned vertex requires at least one nonzero influence");
  auto shared_skin = native_json;
  shared_skin["skins"].push_back(shared_skin["skins"][0]);
  shared_skin["skins"][1]["joints"].erase(
      shared_skin["skins"][1]["joints"].end() - 1);
  shared_skin["nodes"].push_back({{"mesh", 0}, {"skin", 1}});
  shared_skin["scenes"][0]["nodes"].push_back(7);
  preserve_invalid(shared_skin, "Shared meshes validate joint slots "
                                "independently against every instancing skin");

  auto hierarchy_only = native_json;
  hierarchy_only.erase("meshes");
  hierarchy_only["nodes"][1].erase("mesh");
  hierarchy_only["nodes"][1].erase("skin");
  save(asset / "hierarchy-only.gltf", hierarchy_only.dump());
  auto skeleton_asset = import_mesh(asset / "hierarchy-only.gltf", preserve);
  require(skeleton_asset.primitives.empty() &&
              skeleton_asset.nodes.size() == 7 &&
              skeleton_asset.skins.size() == 1,
          "Preserving import retains valid skeleton-only hierarchy assets "
          "without manufacturing geometry");
  auto excessive_depth = gltf;
  excessive_depth["nodes"] = Json::array();
  for (size_t i = 0; i < 258; i++) {
    Json node = Json::object();
    if (i + 1 < 258)
      node["children"] = {i + 1};
    excessive_depth["nodes"].push_back(node);
  }
  excessive_depth["nodes"][257]["mesh"] = 0;
  save(asset / "deep-hierarchy.gltf", excessive_depth.dump());
  rejects([&] { import_mesh(asset / "deep-hierarchy.gltf", preserve); },
          "Hierarchy depth rejects before potentially quadratic validator "
          "parent walks");

  auto exact_json = gltf;
  auto exact_binary = b;
  set_float(exact_binary, 0, -0.f);
  for (size_t k = 0; k < 3; k++)
    set_float(exact_binary, 96 + k * 4, .57735026f);
  save(asset / "exact-authoring.gltf",
       embedded(exact_json, exact_binary).dump());
  auto exact = import_mesh(asset / "exact-authoring.gltf", preserve);
  require(std::signbit(exact.primitives[0].mesh.positions[0].x) &&
              exact.primitives[0].tangents[0][0] == .57735026f,
          "Preserving mode copies authored position and tangent floats without "
          "identity arithmetic or normalization");
  auto unused_json = gltf;
  auto unused_binary = b;
  std::vector<uint8_t> unused_positions;
  floats(unused_positions, {0, 0, 0, 1, 0, 0, 0, 1, 0, .25f, .25f, 0});
  auto unused_view =
      append_view(unused_json, unused_binary, unused_positions, 4);
  unused_json["accessors"][0]["bufferView"] = unused_view;
  unused_json["accessors"][0]["count"] = 4;
  unused_json["meshes"][0]["primitives"][0]["attributes"] = {{"POSITION", 0}};
  unused_json["meshes"][0]["primitives"][0].erase("material");
  save(asset / "unused-authoring.gltf",
       embedded(unused_json, unused_binary).dump());
  auto unused = import_mesh(asset / "unused-authoring.gltf", preserve);
  require(unused.primitives[0].mesh.positions.size() == 4 &&
              !unused.primitives[0].normals_authored &&
              unused.primitives[0].mesh.normals.size() == 4,
          "Absent normals use display fallbacks while retaining all "
          "unreferenced source vertices");

  auto cancel_preserve = preserve;
  int preserve_calls = 0;
  cancel_preserve.cancelled = [&] { return ++preserve_calls > 10; };
  rejects([&] { import_mesh(asset / "native-rig.glb", cancel_preserve); },
          "Authored hierarchy/deformation import can cancel before publishing "
          "a partial asset");

  fs::remove_all(root);
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
