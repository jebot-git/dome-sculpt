// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mtoon.hpp"
#include <fstream>
#include <cmath>
#include <span>
namespace vrm_source_fixture {
using namespace dome;
inline uint32_t word(std::span<const uint8_t> b, size_t at) {
  return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 |
         uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}
inline void source_variant(const std::filesystem::path &input,
                           const std::filesystem::path &output, int mode = 0) {
  std::ifstream file(input, std::ios::binary);
  std::vector<uint8_t> raw{std::istreambuf_iterator<char>(file), {}};
  auto j =
      nlohmann::json::parse(raw.begin() + 20, raw.begin() + 20 + word(raw, 12));
  auto begin = 20 + word(raw, 12);
  std::vector<uint8_t> tail(raw.begin() + begin, raw.end());
  // A new explicit source fixture, not a runtime normalization. Preserve
  // original geometry/images/skins/morphs while choosing the independently
  // implemented shader subset and a full-model orthographic placement.
  for (auto &property : j["extensions"]["VRM"]["materialProperties"]) {
    auto m = MToonMaterial::from_vrm0(property);
    for (size_t r = 2; r < m.textures.size(); ++r)
      m.textures[r].index = -1;
    m.parameters.base_color = {.75f, .6f, .4f, 1};
    m.parameters.shade_color = {.18f, .12f, .08f};
    m.parameters.toony = .5f;
    m.parameters.shift = -.2f;
    m.parameters.normal_enabled = false;
    m.parameters.normal_scale = 0;
    m.parameters.indirect_intensity = 0;
    m.parameters.light_color_attenuation = 0;
    m.parameters.emission = {};
    m.parameters.rim_color = {};
    m.parameters.outline = MToonOutline::None;
    m.parameters.scroll = {};
    m.parameters.rotation_speed = 0;
    m.parameters.cull = MToonCull::Back;
    m.parameters.alpha = MToonAlpha::Opaque;
    m.parameters.depth_write = true;
    m.parameters.source_blend = 1;
    m.parameters.destination_blend = 0;
    m.parameters.render_queue = 2000;
    property = m.source_json();
  }
  for (auto &m : j["materials"]) {
    m["alphaMode"] = "OPAQUE";
    m["doubleSided"] = false;
  }
  auto root = j["nodes"].size();
  j["nodes"].push_back({{"name", "Independent original model placement"},
                        {"children", j["scenes"][0]["nodes"]},
                        {"scale", {.8f, .8f, .8f}},
                        {"translation", {0, -.7f, 0}}});
  j["scenes"][0]["nodes"] = {root};
  if (mode == 1)
    for (auto &node : j["nodes"])
      if (node.contains("mesh"))
        node["scale"] = {-1.f, 1.f, 1.f};
  if (mode == 2) {
    j["nodes"][root]["rotation"] = {0.f, 0.f, std::sin(.11f), std::cos(.11f)};
    auto parent = j["nodes"].size();
    j["nodes"].push_back({{"name", "Original anisotropic parent"},
                          {"scale", {1.125f, .875f, 1.f}},
                          {"children", {root}}});
    j["scenes"][0]["nodes"] = {parent};
  }
  auto text = j.dump();
  while (text.size() % 4)
    text += ' ';
  std::vector<uint8_t> bytes;
  auto put = [&](uint32_t x) {
    for (int k = 0; k < 4; ++k)
      bytes.push_back(uint8_t(x >> (8 * k)));
  };
  put(0x46546c67);
  put(2);
  put(uint32_t(20 + text.size() + tail.size()));
  put(uint32_t(text.size()));
  put(0x4e4f534a);
  bytes.insert(bytes.end(), text.begin(), text.end());
  bytes.insert(bytes.end(), tail.begin(), tail.end());
  std::ofstream result(output, std::ios::binary);
  result.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}
} // namespace vrm_source_fixture
