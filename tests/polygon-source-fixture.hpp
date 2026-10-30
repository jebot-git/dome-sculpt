// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/humanoid-creator.hpp"
#include "dome/polygon-source.hpp"
#include <nlohmann/json.hpp>
#include <numeric>
namespace polygon_test {
using namespace dome;
struct Fixture {
  Mesh rest;
  CreatorAppearance appearance;
  std::shared_ptr<ImportedAsset> asset = std::make_shared<ImportedAsset>();
  nlohmann::json extras;
  Fixture() {
    const Vec3 p[] = {{-1, 0, 0}, {0, 0, 0}, {0, 1, 0},
                      {-1, 1, 0}, {1, 0, 0}, {1, 1, 0}};
    const uint32_t vertices[] = {0, 1, 2, 0, 2, 3, 1, 4, 5, 1, 5, 2};
    const uint32_t corners[] = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    for (size_t i = 0; i < 12; ++i) {
      rest.positions.push_back(p[vertices[i]]);
      rest.normals.push_back({0, 0, 1});
      rest.uv.push_back(
          {p[vertices[i]].x / 4 + (i < 6 ? .25f : .5f), p[vertices[i]].y});
      rest.indices.push_back(i);
    }
    ImportedPrimitive primitive;
    primitive.source_mesh = 0;
    primitive.source_node = 0;
    primitive.source_primitive = 0;
    primitive.mesh = rest;
    asset->primitives.push_back(primitive);
    appearance.asset = asset;
    appearance.source_mesh = appearance.source_node =
        appearance.source_primitive = 0;
    appearance.source_vertices.resize(12);
    std::iota(appearance.source_vertices.begin(),
              appearance.source_vertices.end(), 0);
    extras = {{"dome_source_polygons_version", 1},
              {"dome_source_vertices", vertices},
              {"dome_source_corners", corners},
              {"dome_source_polygons",
               nlohmann::json::array({{{"source_face", 0},
                                       {"vertices", {0, 1, 2, 5}},
                                       {"first_triangle", 0},
                                       {"triangle_count", 2}},
                                      {{"source_face", 1},
                                       {"vertices", {6, 7, 8, 11}},
                                       {"first_triangle", 2},
                                       {"triangle_count", 2}}})}};
    sync();
  }
  void sync() { asset->primitives[0].extras_json = extras.dump(); }
};
} // namespace polygon_test
