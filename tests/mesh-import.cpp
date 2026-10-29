// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import.hpp"
#include "dome/mesh-processing.hpp"
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
  fs::remove_all(root);
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
