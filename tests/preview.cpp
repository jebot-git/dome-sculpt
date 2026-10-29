// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-maps.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
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
  auto source_paint = PaintedMesh::from_maps(mesh, maps, 64);
  auto raw_preview = source_paint->preview(o.material);
  BakedTextures bake;
  bake.maps.color = {64,64,std::vector<uint8_t>(64*64*4)};
  bake.maps.metal_rough = bake.maps.emission = bake.maps.normal = bake.maps.color;
  for(size_t i=0;i<bake.maps.color.rgba.size();++i){
    bake.maps.color.rgba[i]=std::array<uint8_t,4>{20,50,220,255}[i%4];
    bake.maps.metal_rough.rgba[i]=std::array<uint8_t,4>{80,100,0,255}[i%4];
    bake.maps.normal.rgba[i]=std::array<uint8_t,4>{128,204,230,255}[i%4];
  }
  bake.coverage.assign(64*64,255);
  source_paint->install_bake(std::move(bake));
  scene.active().painted=source_paint;
  doc.replace(scene.snapshot());check(doc.wait_mesh(),"Source-backed baked preview completes on the mesh worker");
  auto baked_frame=doc.frame().surface;
  check(!baked_frame->ranges[0].textures->source_only &&
        baked_frame->ranges[0].textures->color.rgba[0]==20 &&
        baked_frame->ranges[0].textures->normal.rgba[1]==204 &&
        baked_frame->tangents.size()==baked_frame->mesh.positions.size() &&
        baked_frame->mesh.positions.size()==mesh.indices.size(),
        "A bake overrides raw-source eligibility and publishes canonical normal tangent corners");
  check(raw_preview->source_only && raw_preview->color.rgba.empty() &&
        baked_frame->ranges[0].textures->source_maps->color.image==maps->color.image,
        "Installing a bake preserves held raw-source markers and exact original image identity");
  auto before_clear=doc.copy_scene().encode();
  doc.edit_scene("Clear baked surface",[](Scene &s,const auto&){s.active().painted->clear_bake();});
  check(doc.wait_asset() && doc.wait_mesh() && doc.frame().state->error.empty() &&
        doc.frame().surface->ranges[0].textures->source_only,
        "Clearing a bake restores direct original-source GPU eligibility");
  check(doc.undo() && doc.wait_mesh() && doc.copy_scene().encode()==before_clear &&
        !doc.frame().surface->ranges[0].textures->source_only &&
        baked_frame->ranges[0].textures->normal.rgba[1]==204,
        "Global Undo restores baked preview selection without changing held GPU snapshots");
  return 0;
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
