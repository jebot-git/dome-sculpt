// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh.hpp"
#include "io.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>
namespace dome {
namespace {
void append(std::vector<uint8_t> &bytes, uint32_t value) {
  for (int n = 0; n < 4; ++n)
    bytes.push_back(uint8_t(value >> (n * 8)));
}
} // namespace
void export_glb(const Mesh &mesh, const std::filesystem::path &path) {
  mesh.validate();
  if (path.extension() != ".glb")
    throw std::invalid_argument(
        "This exporter currently writes binary glTF (.glb)");
  nlohmann::json j = {
      {"asset", {{"version", "2.0"}, {"generator", "Dome Sculpt"}}},
      {"scene", 0},
      {"scenes", {{{"nodes", {0}}}}},
      {"nodes", {{{"mesh", 0}, {"name", "Sculpt"}}}}};
  std::vector<uint8_t> data;
  auto floats = [&](auto values) {
    for (float f : values)
      append(data, std::bit_cast<uint32_t>(f));
  };
  Vec3 lo = mesh.positions[0], hi = lo;
  for (auto p : mesh.positions) {
    floats(std::array{p.x, p.y, p.z});
    lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
    hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
  }
  size_t normal_offset = data.size();
  for (auto p : mesh.normals)
    floats(std::array{p.x, p.y, p.z});
  size_t uv_offset = data.size();
  for (auto p : mesh.uv)
    floats(p);
  size_t index_offset = data.size();
  for (uint32_t index : mesh.indices)
    append(data, index);
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
  int positions =
      add(0, normal_offset, 34962, mesh.positions.size(), 5126, "VEC3");
  j["accessors"][positions]["min"] = {lo.x, lo.y, lo.z};
  j["accessors"][positions]["max"] = {hi.x, hi.y, hi.z};
  int normals = add(normal_offset, uv_offset - normal_offset, 34962,
                    mesh.normals.size(), 5126, "VEC3");
  nlohmann::json attributes = {{"POSITION", positions}, {"NORMAL", normals}};
  if (!mesh.uv.empty())
    attributes["TEXCOORD_0"] = add(uv_offset, index_offset - uv_offset, 34962,
                                   mesh.uv.size(), 5126, "VEC2");
  int indices = add(index_offset, data.size() - index_offset, 34963,
                    mesh.indices.size(), 5125, "SCALAR");
  j["buffers"] = {{{"byteLength", data.size()}}};
  j["meshes"] = {{{"primitives",
                   {{{"attributes", attributes},
                     {"indices", indices},
                     {"material", 0},
                     {"mode", 4}}}}}};
  j["materials"] = {{{"name", "Clay"},
                     {"pbrMetallicRoughness",
                      {{"baseColorFactor", {.58, .30, .15, 1.0}},
                       {"metallicFactor", 0.0},
                       {"roughnessFactor", .8}}}}};
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
