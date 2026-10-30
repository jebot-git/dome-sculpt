// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "current-colors.hpp"
#include "polygon-color-source.hpp"
#include "source-color-fixtures.hpp"
namespace polygon_current_fixture {
using namespace dome;
inline ObjectSurface surface(bool seam = false) {
  PolygonMesh topology;
  PolygonColorField colors;
  if (seam) {
    Mesh source;
    source.positions = {
        {-.5f, -.5f, 0}, {.5f, -.5f, 0}, {.5f, .5f, 0}, {-.5f, .5f, 0}};
    source.normals.assign(4, {0, 0, 1});
    source.uv = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    source.indices = {0, 1, 2, 0, 2, 3};
    topology = PolygonMesh::from_triangles(source);
    std::vector<PolygonCornerRgba> rows;
    for (size_t f = 0; f < topology.faces().size(); ++f)
      for (auto &corner : topology.faces()[f].corners)
        rows.push_back({corner.id, {.25f, .5f, .75f, float(f)}});
    colors = PolygonColorField::from_corners(topology, rows);
  } else {
    auto original = source_color_fixture::model();
    auto source = prepare_polygon_source_colors(*original.active().painted,
                                                original.active().appearance);
    auto edit =
        edit_colored_polygon(source.geometry.topology, source.field,
                             {PolygonOperation::CutEdges,
                              {PolygonSelectionMode::Edge,
                               {source.geometry.topology.edges().front().id}},
                              {},
                              0,
                              2});
    topology = std::move(*edit.geometry.replacement);
    colors = std::move(*edit.colors);
  }
  auto pair = std::make_shared<PolygonColorSurface>();
  pair->topology = std::make_shared<const PolygonMesh>(std::move(topology));
  pair->field = std::make_shared<const PolygonColorField>(std::move(colors));
  ObjectSurface out;
  out.id = 1;
  out.name = "Native colored topology";
  out.mesh = pair->topology->triangulate().mesh;
  out.material.color = {1, 1, 1, 1};
  out.material.unlit = true;
  out.material.alpha = Material::AlphaMode::Opaque;
  out.polygon_colors = std::move(pair);
  return out;
}
} // namespace polygon_current_fixture
