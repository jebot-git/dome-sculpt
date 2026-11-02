// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
namespace dome::post_uv_key_fixture {
inline Scene dense_model() {
  Mesh mesh;
  constexpr unsigned n = 17;
  for (unsigned y = 0; y < n; ++y) for (unsigned x = 0; x < n; ++x) {
    mesh.positions.push_back({(float(x) / (n - 1) - .5f) * 1.6f,
                              (float(y) / (n - 1) - .5f) * 1.6f, 0});
    mesh.normals.push_back({0, 0, 1});
    mesh.uv.push_back({float(x) / (n - 1), float(y) / (n - 1)});
  }
  for (unsigned y = 0; y < n - 1; ++y) for (unsigned x = 0; x < n - 1; ++x) {
    auto a = y * n + x, b = a + 1, c = a + n, d = c + 1;
    mesh.indices.insert(mesh.indices.end(), {a, b, d, a, d, c});
  }
  Scene scene;
  scene.active().mesh_active = true;
  scene.active().painted = std::make_shared<PaintedMesh>(mesh, 64);
  DeformationState deform(mesh);
  deform.morphs = MorphSet(mesh);
  auto positions = mesh.positions;
  positions[(n / 2) * n + n / 2].z = .01f;
  deform.morphs->set_target(mesh, "Initial grid key", positions);
  deform.skeleton = Skeleton{{{"root", -1, {}, 1}, {"bend", 0, {}, 1}}};
  deform.skin = SkinWeights{};
  for (const auto &position : mesh.positions) {
    float weight = (position.x + .8f) / 1.6f;
    deform.skin->vertices.push_back({{0, .2f + .6f * weight}, {1, .8f - .6f * weight}});
  }
  scene.set_deformation(scene.active().id, deform);
  auto high_id = scene.add("Readonly normal source", Volume{});
  auto &high = scene.object(high_id);
  high.visible = false;
  high.locked = true;
  high.mesh_active = true;
  Mesh plane;
  plane.positions = {{-1.2f, -1.2f, .1f}, {1.2f, -1.2f, .1f},
                     {1.2f, 1.2f, .1f}, {-1.2f, 1.2f, .1f}};
  plane.normals.assign(4, normalized(Vec3{.1f, .15f, 1}));
  plane.uv = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  plane.indices = {0, 1, 2, 0, 2, 3};
  high.painted = std::make_shared<PaintedMesh>(plane, 64);
  scene.select(1);
  Document document;
  document.replace(scene);
  ObjectBakeSettings settings;
  settings.prepare_normals = true;
  settings.front_mm = 500;
  settings.back_mm = 50;
  settings.ao_samples = 0;
  document.bake_from_object(high_id, settings);
  auto ready = [&] {
    if (!document.wait_asset() || !document.wait_mesh() || !document.frame().state->error.empty())
      throw std::runtime_error("Post-UV GUI fixture: " + document.frame().state->error);
  };
  ready();
  for (int pass = 0; pass < 2; ++pass) {
    document.begin_uv();
    if (!document.wait_uv()) throw std::runtime_error("Fixture UV initialize timeout");
    document.edit_uv(UvAction::Scale, .85f);
    if (!document.wait_uv()) throw std::runtime_error("Fixture UV scale timeout");
    document.end_uv(true);
    ready();
  }
  scene = document.copy_scene();
  auto current = *scene.active().deformation;
  current.pose = {{}, {}};
  current.pose[0].scale = {1.05f, .95f, 1.1f};
  current.pose[1].scale = {.95f, 1.05f, .9f};
  current.morph_weights = {.35f};
  scene.set_deformation(scene.active().id, current);
  return scene;
}
} // namespace dome::post_uv_key_fixture
