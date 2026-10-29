// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-maps.hpp"
#include <iostream>
using namespace dome;
void check(bool ok, const char *label) {
  if (!ok) throw std::runtime_error(label);
  std::cout << "PASS " << label << '\n';
}
int main() try {
  Mesh mesh;
  mesh.positions = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};
  mesh.normals.assign(4, {0, 0, 1}); mesh.indices = {0, 1, 2, 0, 2, 3};
  mesh.uv = {{{0, 0}}, {{1, 0}}, {{1, 1}}, {{0, 1}}};
  auto maps = std::make_shared<PaintSourceMaps>();
  maps->color.image = std::make_shared<const Image>(Image{2, 1, {180, 40, 20, 255, 20, 40, 180, 255}});
  Scene scene;
  auto &o = scene.active(); o.mesh_active = true;
  o.material.color = {1, 1, 1, 1};
  o.painted = PaintedMesh::from_maps(mesh, maps, 64);
  auto builds = paint_composite_cache_stats().builds;
  Document doc; doc.replace(scene.snapshot());
  check(doc.wait_mesh(), "Source-backed scene publishes preview on the mesh worker");
  auto frame = doc.frame();
  check(frame.surface->ranges.size() == 1 && frame.surface->ranges[0].textures->source_only,
        "Actual Document mesh worker selects the raw original-source preview path");
  check(frame.surface->ranges[0].textures->source_maps->color.image == maps->color.image,
        "Document preview retains exact immutable source image identity");
  check(paint_composite_cache_stats().builds == builds,
        "Actual Document preview requires no CPU composite builds");
  auto exported = scene.surfaces();
  check(!exported[0].textures->source_only && !exported[0].textures->color.rgba.empty(),
        "Export surfaces retain explicit complete composites rather than GPU-only markers");
  auto painted = o.painted->snapshot();
  painted->begin_stroke(); Brush brush; brush.from = brush.to = {0, 0, 0}; brush.radius = .8f;
  brush.normal = {0, 0, 1};
  PaintSettings settings; settings.color = {0, 1, 0, 1};
  painted->stamp(brush, settings);
  check(painted->end_stroke(), "Fixture creates a real editable paint overlay");
  o.painted = painted;
  doc.replace(scene.snapshot()); check(doc.wait_mesh(), "Painted scene preview completes");
  check(!doc.frame().surface->ranges[0].textures->source_only,
        "Visible paint overrides use the composite path without losing the overlay");
  return 0;
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
