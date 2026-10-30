// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/import-authoring.hpp"
#include <bit>
#include <fstream>
#include <nlohmann/json.hpp>
namespace import_fixture {
using namespace dome;
using Json = nlohmann::json;
inline void word(std::vector<uint8_t> &out, uint32_t v) {
  for (int b = 0; b < 4; ++b)
    out.push_back(uint8_t(v >> (b * 8)));
}
struct Fixture {
  Json j = {{"asset", {{"version", "2.0"}}},
            {"accessors", Json::array()},
            {"bufferViews", Json::array()}};
  std::vector<uint8_t> bin;
  int values(std::initializer_list<float> data, int dimension,
             const char *type) {
    auto start = bin.size();
    for (float f : data)
      word(bin, std::bit_cast<uint32_t>(f));
    int view = int(j["bufferViews"].size());
    j["bufferViews"].push_back({{"buffer", 0},
                                {"byteOffset", start},
                                {"byteLength", bin.size() - start}});
    int a = int(j["accessors"].size());
    j["accessors"].push_back({{"bufferView", view},
                              {"componentType", 5126},
                              {"count", data.size() / dimension},
                              {"type", type}});
    return a;
  }
  Fixture() {
    int p = values({-.1f, -.1f, 0, .1f, -.1f, 0, .1f, .1f, 0, -.1f, .1f, 0}, 3,
                   "VEC3");
    j["accessors"][p]["min"] = {-.1f, -.1f, 0};
    j["accessors"][p]["max"] = {.1f, .1f, 0};
    int n = values({0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}, 3, "VEC3");
    int t =
        values({1, 0, 0, -1, 1, 0, 0, -1, 1, 0, 0, -1, 1, 0, 0, -1}, 4, "VEC4");
    int u = values({0, 0, 1, 0, 1, 1, 0, 1}, 2, "VEC2"),
        u1 = values({.2f, .3f, .7f, .3f, .7f, .8f, .2f, .8f}, 2, "VEC2");
    int c = values({.5f, .2f, .8f, .4f, .2f, .7f, .3f, .6f, .8f, .1f, .2f, .8f,
                    .3f, .4f, .5f, 1},
                   4, "VEC4");
    auto start = bin.size();
    for (auto v : {0u, 1u, 2u, 0u, 2u, 3u})
      word(bin, v);
    int view = int(j["bufferViews"].size());
    j["bufferViews"].push_back({{"buffer", 0},
                                {"byteOffset", start},
                                {"byteLength", bin.size() - start}});
    int index = int(j["accessors"].size());
    j["accessors"].push_back({{"bufferView", view},
                              {"componentType", 5125},
                              {"count", 6},
                              {"type", "SCALAR"}});
    int dp =
        values({0, .01f, 0, 0, .02f, 0, 0, .03f, 0, 0, .04f, 0}, 3, "VEC3");
    int dn = values({.1f, 0, 0, .2f, 0, 0, .3f, 0, 0, .4f, 0, 0}, 3, "VEC3");
    int dt = values({0, .1f, 0, 0, .2f, 0, 0, .3f, 0, 0, .4f, 0}, 3, "VEC3");
    Json primitive = {
        {"attributes",
         {{"POSITION", p},
          {"NORMAL", n},
          {"TANGENT", t},
          {"TEXCOORD_0", u},
          {"TEXCOORD_1", u1},
          {"COLOR_0", c}}},
        {"indices", index},
        {"material", 0},
        {"targets", {{{"POSITION", dp}, {"NORMAL", dn}, {"TANGENT", dt}}}}};
    j["meshes"] = {{{"name", "Source mesh"},
                    {"primitives", {primitive}},
                    {"weights", {.2f}},
                    {"extras", {{"targetNames", {"Lift"}}}}}};
    j["nodes"] = {{{"name", "Root 日本語"},
                   {"translation", {.3f, .1f, 0}},
                   {"children", {1, 2}}},
                  {{"name", "Instance one"},
                   {"mesh", 0},
                   {"translation", {.2f, 0, 0}},
                   {"scale", {1.2f, 1.2f, 1.2f}},
                   {"weights", {.7f}}},
                  {{"name", "Instance two"},
                   {"mesh", 0},
                   {"translation", {-.3f, 0, -.2f}},
                   {"weights", {.3f}}}};
    j["scenes"] = {{{"name", "Selected"}, {"nodes", {0}}}};
    j["scene"] = 0;
    Image image{
        2,
        2,
        {255, 0, 0, 32, 0, 255, 0, 96, 0, 0, 255, 160, 255, 255, 255, 255}};
    auto png = image.png();
    while (bin.size() % 4)
      bin.push_back(0);
    int image_view = int(j["bufferViews"].size());
    j["bufferViews"].push_back({{"buffer", 0},
                                {"byteOffset", bin.size()},
                                {"byteLength", png.size()}});
    bin.insert(bin.end(), png.begin(), png.end());
    j["images"] = {{{"name", "RGBA"},
                    {"bufferView", image_view},
                    {"mimeType", "image/png"}}};
    j["samplers"] = {{{"wrapS", 33648},
                      {"wrapT", 33071},
                      {"magFilter", 9728},
                      {"minFilter", 9986}}};
    j["textures"] = {{{"source", 0}, {"sampler", 0}}};
    j["materials"] = {
        {{"name", "Alpha source"},
         {"pbrMetallicRoughness",
          {{"baseColorFactor", {.7f, .8f, .9f, .6f}},
           {"baseColorTexture", {{"index", 0}}},
           {"metallicFactor", .25f},
           {"roughnessFactor", .65f}}},
         {"alphaMode", "MASK"},
         {"alphaCutoff", .3f},
         {"doubleSided", true},
         {"extensions", {{"KHR_materials_unlit", Json::object()}}}}};
    j["extensionsUsed"] = {"KHR_materials_unlit"};
  }
  std::vector<uint8_t> bytes() {
    auto json = j;
    json["buffers"] = {{{"byteLength", bin.size()}}};
    auto text = json.dump();
    while (text.size() % 4)
      text += ' ';
    auto binary = bin;
    while (binary.size() % 4)
      binary.push_back(0);
    std::vector<uint8_t> out;
    word(out, 0x46546c67);
    word(out, 2);
    word(out, uint32_t(28 + text.size() + binary.size()));
    word(out, uint32_t(text.size()));
    word(out, 0x4e4f534a);
    out.insert(out.end(), text.begin(), text.end());
    word(out, uint32_t(binary.size()));
    word(out, 0x004e4942);
    out.insert(out.end(), binary.begin(), binary.end());
    return out;
  }
  void save(const std::filesystem::path &p) {
    auto b = bytes();
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char *>(b.data()), b.size());
  }
};

} // namespace import_fixture
