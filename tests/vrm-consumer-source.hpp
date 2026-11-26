// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "import-authoring-fixtures.hpp"
#include "dome/creation.hpp"
#include "dome/source-placement.hpp"
#include <algorithm>

namespace vrm_consumer_fixture {
using namespace dome;

inline import_fixture::Fixture source(const CreatorAppearance &appearance) {
  const auto &bytes = *appearance.original_bytes;
  auto word = [&](size_t offset) {
    uint32_t value = 0;
    for (unsigned byte = 0; byte < 4; ++byte)
      value |= uint32_t(bytes.at(offset + byte)) << (byte * 8);
    return value;
  };
  const size_t json_end = 20 + word(12);
  if (word(16) != 0x4e4f534a || word(json_end + 4) != 0x004e4942 ||
      json_end + 8 + word(json_end) != bytes.size())
    throw std::runtime_error("Expected a two-chunk VRoid fixture");
  import_fixture::Fixture fixture;
  fixture.j = nlohmann::json::parse(bytes.begin() + 20, bytes.begin() + json_end);
  fixture.bin.assign(bytes.begin() + json_end + 8, bytes.end());
  return fixture;
}

inline import_fixture::Fixture female_source() {
  Scene seed;
  const auto group = append_humanoid(seed, humanoid_baseline_defaults("vroid-beta-female"),
                                    "Source fixture", {}, 64);
  for (const auto &object : seed.objects())
    if (object.group == group)
      return source(*object.appearance);
  throw std::runtime_error("Missing CC0 VRoid source fixture");
}

inline ImportedAuthoring import_source(import_fixture::Fixture &fixture,
                                       const std::filesystem::path &path,
                                       CanvasTransform placement = {}) {
  fixture.save(path);
  Scene empty;
  empty.active().visible = false;
  ImportAuthoringOptions options;
  options.paint_resolution = 64;
  auto imported = prepare_authoring_import(empty, path, options);
  if (imported.objects.size() != 19)
    throw std::runtime_error("Expected all 19 skinned VRoid parts in imported fixture");
  const auto first = imported.objects.front();
  const auto &group = imported.scene.group(imported.scene.object(first).group);
  if (!group.source_placement)
    throw std::runtime_error("Imported VRM fixture is missing its original node graph");
  imported.scene.group(group.source_placement->source_instance).transform = placement;
  imported.scene.select(first);
  imported.scene.validate();
  return imported;
}
} // namespace vrm_consumer_fixture
