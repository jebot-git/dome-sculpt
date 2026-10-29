// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "io.hpp"
#include <bit>
#include <nlohmann/json.hpp>
namespace dome {
namespace {
void append(std::vector<uint8_t> &bytes, uint32_t value) {
  for (int n = 0; n < 4; ++n)
    bytes.push_back(uint8_t(value >> (n * 8)));
}
} // namespace
void export_glb(const Mesh &mesh, const std::filesystem::path &path) {
  export_glb(std::vector<ObjectSurface>{{1, "Sculpt", {}, Material{}, mesh}},
             path);
}
void export_glb(const std::vector<ObjectSurface> &objects,
                const std::filesystem::path &path) {
  if (path.extension() != ".glb")
    throw std::invalid_argument(
        "This exporter currently writes binary glTF (.glb)");
  if (objects.empty())
    throw std::invalid_argument("The scene has no visible surface to export");
  nlohmann::json j = {
      {"asset", {{"version", "2.0"}, {"generator", "Dome Sculpt"}}},
      {"scene", 0},
      {"scenes", {{{"nodes", nlohmann::json::array()}}}}};
  std::vector<uint8_t> data;
  auto floats = [&](auto values) {
    for (float f : values)
      append(data, std::bit_cast<uint32_t>(f));
  };
  auto add = [&](size_t offset, size_t bytes, int target, size_t count,
                 int component, const char *type) {
    int i = int(j["bufferViews"].size());
    j["bufferViews"].push_back({{"buffer", 0},
                                {"byteOffset", offset},
                                {"byteLength", bytes},
                                {"target", target}});
    j["accessors"].push_back({{"bufferView", i},
                              {"componentType", component},
                              {"count", count},
                              {"type", type}});
    return int(j["accessors"].size()) - 1;
  };
  for (auto &o : objects) {
    auto &mesh = o.mesh;
    mesh.validate();
    o.material.validate();
    validate_transform(o.transform);
    if (mesh.positions.empty())
      throw std::invalid_argument("Cannot export an empty mesh");
    size_t begin = data.size();
    Vec3 lo = mesh.positions.front(), hi = lo;
    for (auto p : mesh.positions) {
      floats(std::array{p.x, p.y, p.z});
      lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
      hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    int positions = add(begin, data.size() - begin, 34962,
                        mesh.positions.size(), 5126, "VEC3");
    j["accessors"][positions]["min"] = {lo.x, lo.y, lo.z};
    j["accessors"][positions]["max"] = {hi.x, hi.y, hi.z};
    begin = data.size();
    for (auto p : mesh.normals)
      floats(std::array{p.x, p.y, p.z});
    int normals = add(begin, data.size() - begin, 34962, mesh.normals.size(),
                      5126, "VEC3");
    nlohmann::json attributes = {{"POSITION", positions}, {"NORMAL", normals}};
    if (!mesh.uv.empty()) {
      begin = data.size();
      for (auto p : mesh.uv)
        floats(p);
      attributes["TEXCOORD_0"] =
          add(begin, data.size() - begin, 34962, mesh.uv.size(), 5126, "VEC2");
    }
    begin = data.size();
    for (auto index : mesh.indices)
      append(data, index);
    int indices = add(begin, data.size() - begin, 34963, mesh.indices.size(),
                      5125, "SCALAR");
    int node = int(j["nodes"].size()), material = int(j["materials"].size()),
        mesh_index = int(j["meshes"].size());
    auto t = o.transform;
    auto &m = o.material;
    j["scenes"][0]["nodes"].push_back(node);
    j["nodes"].push_back(
        {{"name", o.name},
         {"mesh", mesh_index},
         {"translation", {t.position.x, t.position.y, t.position.z}},
         {"rotation",
          {t.orientation.x, t.orientation.y, t.orientation.z, t.orientation.w}},
         {"scale", {t.scale, t.scale, t.scale}}});
    j["meshes"].push_back({{"name", o.name},
                           {"primitives",
                            {{{"attributes", attributes},
                              {"indices", indices},
                              {"material", material},
                              {"mode", 4}}}}});
    j["materials"].push_back(
        {{"name", m.name},
         {"pbrMetallicRoughness",
          {{"baseColorFactor", m.color},
           {"metallicFactor", m.metallic},
           {"roughnessFactor", m.roughness}}},
         {"emissiveFactor", m.emissive},
         {"doubleSided", m.double_sided},
         {"alphaMode", m.color[3] < 1 ? "BLEND" : "OPAQUE"}});
  }
  j["buffers"] = {{{"byteLength", data.size()}}};
  std::string text = j.dump();
  while (text.size() % 4)
    text.push_back(' ');
  uint64_t length = 12 + 8 + text.size() + 8 + data.size();
  if (length > UINT32_MAX)
    throw std::length_error("glTF exceeds the GLB size limit");
  std::vector<uint8_t> output;
  output.reserve(length);
  append(output, 0x46546c67);
  append(output, 2);
  append(output, uint32_t(length));
  append(output, uint32_t(text.size()));
  append(output, 0x4e4f534a);
  output.insert(output.end(), text.begin(), text.end());
  append(output, uint32_t(data.size()));
  append(output, 0x004e4942);
  output.insert(output.end(), data.begin(), data.end());
  atomic_write(path, output);
}
} // namespace dome
