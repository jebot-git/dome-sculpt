// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake.hpp"
#include "affine-surface.hpp"
#include "current-colors.hpp"
#include "dome/import-authoring.hpp"
#include "dome/polygon-color.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/source-placement.hpp"
#include "polygon-conversion.hpp"
#include <unistd.h>
namespace cut_color_fixture {
using namespace dome;
inline Scene scene(bool seam = false, bool affine = false,uint32_t resolution=64) {
  auto description = PolygonMesh::cube(.4f).encode();
  const std::array<std::array<float, 2>, 4> square{
      {{.08f, .08f}, {.92f, .08f}, {.92f, .92f}, {.08f, .92f}}};
  for (size_t f = 0; f < description["faces"].size(); ++f)
    for (size_t c = 0; c < 4; ++c) {
      description["faces"][f]["corners"][c][2] = std::array<float, 2>{
          (float(f % 3) + square[c][0]) / 3, (float(f / 3) + square[c][1]) / 2};
      description["faces"][f]["corners"][c][3] = false;
    }
  auto topology =
      std::make_shared<const PolygonMesh>(PolygonMesh::decode(description));
  std::vector<PolygonCornerRgba> rows;
  std::map<PolygonId, Vec3> positions;
  for (const auto &vertex : topology->vertices())
    positions.emplace(vertex.id, vertex.position);
  for (size_t f = 0; f < topology->faces().size(); ++f)
    for (const auto &corner : topology->faces()[f].corners) {
      auto p = positions.at(corner.vertex);
      float x = (p.x + .2f) / .4f, y = (p.y + .2f) / .4f;
      rows.push_back(
          {corner.id,
           {.15f + .7f * x, .1f + .8f * y, .25f + .4f * (float(f) / 5),
            seam ? float(f % 2) : .2f + .8f * x}});
    }
  auto field = std::make_shared<const PolygonColorField>(
      PolygonColorField::from_corners(*topology, rows));
  ObjectSurface surface;
  surface.id = 1;
  surface.name = "Actual raw color cube";
  surface.material.color = {1, 1, 1, 1};
  surface.material.unlit = true;
  surface.mesh = topology->triangulate().mesh;
  surface.polygon_colors = std::make_shared<const PolygonColorSurface>(
      PolygonColorSurface{topology, field});
  auto path = std::filesystem::temp_directory_path() /
              ("dome-cut-color-source-" + std::to_string(getpid()) + ".glb");
  export_glb({surface}, path);
  ImportAuthoringOptions options;
  options.paint_resolution = resolution;
  auto imported = prepare_authoring_import(Scene{}, path, options);
  std::error_code error;
  std::filesystem::remove(path, error);
  auto result = std::move(imported.scene);
  if (result.active().id != 1)
    result.erase(1);
  auto original = result.active();
  auto source = prepare_polygon_workspace_source(original, true);
  adopt_polygon_edit(result, original.id, source.topology, source.colors,
                     original,
                     result.has_affine_placement(original.id)
                         ? CanvasTransform{}
                         : result.world_transform(original.id),
                     true);
  auto &object = result.active();
  object.name = "Colored cut source";
  object.material.unlit = true;
  object.material.alpha = Material::AlphaMode::Opaque;
  object.shape_source = capture_polygon_origin(result, object.id);
  if (affine) {
    auto group = result.add_group("Exact sheared parent", {}, object.group);
    result.group(group).affine_edit = std::make_shared<const NativeAffineEdit>(
        AffineTransform(std::array<double, 16>{
            -.9, .1, 0, 0, .25, 1.1, 0, 0, .05, .1, .8, 0, .07, -.03, .02, 1}));
    object.group = group;
    object.transform.position = {.02f, .01f, -.01f};
  }
  result.validate();
  return result;
}
inline void pose(Scene &scene) {
  const auto &mesh=scene.active().painted->mesh();
  DeformationState state(mesh);state.skeleton=Skeleton{{{"root",-1,{},.4f},{"upper",0,{{0,.1f,0},{},{1,1,1}},.2f}}};state.skin=SkinWeights{};
  for(auto p:mesh.positions)state.skin->vertices.push_back(p.y>0?std::vector<Influence>{{0,.25f},{1,.75f}}:std::vector<Influence>{{0,1}});
  state.pose.resize(2);state.pose[0].translation={.013f,-.009f,.007f};state.pose[1].translation={0,.1f,0};state.pose[1].rotation={.99875026f,0,0,.04997917f};
  state.morphs=MorphSet(mesh);auto target=mesh.positions;for(auto &p:target){p.x+=.04f*p.y;p.z+=.02f*p.x;}state.morphs->set_target(mesh,"Native artist offset",target);state.morph_weights={.6f};scene.set_deformation(scene.active().id,std::move(state));
}
inline BakeSource source(const Scene &scene) {
  const auto &object = scene.active();
  auto rest = object.painted->mesh();
  auto geometry = std::make_shared<const Mesh>(
      object.deformation ? object.deformation->preview(rest) : rest);
  PolygonColorSurface field{std::shared_ptr<const PolygonMesh>(
                                object.polygon, &object.polygon->topology),
                            object.polygon->colors};
  auto colors =
      detail::current_vertex_colors(rest, &field, object.appearance.get(),
                                    object.material_bindings.get(), nullptr);
  BakeSource result;
  result.materials[0].material = object.material;
  result.materials[0].maps = object.painted->preview(object.material);
  auto frame = detail::placement_frame(scene, object.id);
  const bool affine = scene.has_affine_placement(object.id),
             reversed = frame.orientation() == AffineOrientation::Reversing;
  const auto world =
      affine ? CanvasTransform{} : scene.world_transform(object.id);
  for (size_t at = 0; at < geometry->indices.size(); ++at) {
    auto c = at;
    if (reversed && at % 3)
      c = at / 3 * 3 + (at % 3 == 1 ? 2 : 1);
    auto v = geometry->indices[c];
    auto p = geometry->positions[v], n = geometry->normals[v];
    auto q = ob::transform(world, {p.x, p.y, p.z}),
         r = ob::rotate(world.orientation, {n.x, n.y, n.z});
    result.mesh.indices.push_back(uint32_t(result.mesh.positions.size()));
    result.mesh.positions.push_back(affine ? detail::placed_position(frame, p)
                                           : Vec3{q.x, q.y, q.z});
    result.mesh.normals.push_back(affine ? detail::placed_normal(frame, n)
                                         : Vec3{r.x, r.y, r.z});
    result.mesh.uv.push_back(geometry->uv[v]);
    result.colors.push_back(colors[v]);
  }
  return result;
}
} // namespace cut_color_fixture
