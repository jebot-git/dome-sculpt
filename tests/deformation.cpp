// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
namespace {
void require(bool yes, const char *message) {
  if (!yes)
    throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
void rejects(auto fn, const char *message) {
  bool failed = false;
  try {
    fn();
  } catch (const std::exception &) {
    failed = true;
  }
  require(failed, message);
}
Mesh fixture() {
  Mesh m;
  m.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0},
                 {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
  m.normals.assign(6, {0, 0, 1});
  m.uv = {{{0, 0}}, {{.4f, 0}}, {{0, 1}}, {{.6f, 0}}, {{1, 1}}, {{.6f, 1}}};
  m.indices = {0, 1, 2, 3, 4, 5};
  m.uv_pins = {1, 0, 0, 1, 0, 0};
  m.uv_seams = {1, 0, 1, 0, 0, 1};
  return m;
}
} // namespace
int main() try {
  auto mesh = fixture();
  DeformationState state(mesh);
  state.skeleton = Skeleton{{{"root", -1, {{.2f, .3f, 0}, {}, {1, 1, 1}}, .5f},
                             {"child", 0, {{0, .5f, 0}, {}, {1, 1, 1}}, .2f}}};
  state.skin = SkinWeights{};
  state.skin->vertices.assign(mesh.positions.size(), {{0, 1}});
  state.morphs = MorphSet(mesh);
  auto edited = mesh.positions;
  for (auto &p : edited)
    p.z += .05f;
  state.morphs->set_target(mesh, "forward", edited);
  state.morph_weights = {.5f};
  state.pose = {state.skeleton->bones[0].rest, state.skeleton->bones[1].rest};
  state.pose[0].translation.y += .25f;
  state.expressions = AvatarExpressions{{{"aa", {{"forward", .7f}}}}};
  state.validate(mesh);
  auto preview = state.preview(mesh);
  require(length(preview.positions[0] - Vec3{0, .25f, .025f}) < 1e-6f,
          "Preview applies morph then pose without baking rest geometry");
  require(preview.indices == mesh.indices && preview.uv == mesh.uv &&
              preview.uv_pins == mesh.uv_pins &&
              preview.uv_seams == mesh.uv_seams,
          "Preview retains vertex splits, UVs, pins and seams");
  auto restored = DeformationState::decode(state.encode(mesh), mesh);
  require(restored.encode(mesh) == state.encode(mesh),
          "Native deformation record roundtrips skeleton, skin, morph, pose "
          "and expressions exactly");
  auto stale = mesh;
  stale.positions[0].z = .01f;
  rejects([&] { state.validate(stale); },
          "Changed rest positions reject stale deformation source identity");
  stale = mesh;
  std::swap(stale.indices[0], stale.indices[1]);
  rejects([&] { state.validate(stale); },
          "Changed triangle ordering rejects stale deformation topology");
  auto invalid = state;
  invalid.pose[0].rotation.w = 2;
  rejects([&] { invalid.validate(mesh); },
          "Nonunit pose rotations fail validation");
  invalid = state;
  invalid.skeleton.reset();
  rejects([&] { invalid.validate(mesh); },
          "Skin and pose cannot outlive their skeleton");
  invalid = state;
  invalid.morphs.reset();
  rejects([&] { invalid.validate(mesh); },
          "Expression and preview values cannot outlive morph data");
  auto j = state.encode(mesh);
  j["source"] = 0;
  rejects([&] { DeformationState::decode(j, mesh); },
          "Native decode checks source identity before authoring allocation");
  j = state.encode(mesh);
  j["skin"]["vertices"][0] = nlohmann::json::array();
  rejects([&] { DeformationState::decode(j, mesh); },
          "Native decode rejects unweighted vertices");
  j = state.encode(mesh);
  j["pose"] = nlohmann::json::array();
  for (int i = 0; i < 257; ++i)
    j["pose"].push_back({});
  rejects([&] { DeformationState::decode(j, mesh); },
          "Native decode bounds pose arrays");
  Scene scene;
  scene.active().painted = std::make_shared<PaintedMesh>(mesh, 64);
  scene.active().mesh_active = true;
  scene.set_deformation(scene.active().id, state);
  auto snapshot = scene.snapshot();
  auto copy = scene;
  require(snapshot.active().deformation == scene.active().deformation,
          "Snapshots share immutable deformation safely");
  state.pose[0].translation.y = 8;
  require(scene.active().deformation->pose[0].translation.y != 8,
          "Publishing deformation detaches caller's mutable authoring value");
  auto changed = *scene.active().deformation;
  changed.morph_weights = {.9f};
  scene.set_deformation(scene.active().id, changed);
  require(snapshot.active().deformation->morph_weights[0] == .5f &&
              copy.active().deformation->morph_weights[0] == .5f,
          "COW replacement preserves snapshot/history authoring state");
  auto surfaces = scene.surfaces();
  require(surfaces[0].mesh.positions[0].z == 0 &&
              surfaces[0].preview_mesh().positions[0].z > .04f,
          "ObjectSurface retains rest mesh and exposes a separate preview");
  auto path = std::filesystem::temp_directory_path() /
              ("dome-deformation-" + std::to_string(getpid()) + ".dsculpt");
  scene.save(path);
  auto loaded = Scene::load(path);
  require(
      scene.encode() == loaded.encode(),
      "Version-four scene save/reopen preserves complete deformation state");
  auto id = scene.duplicate(scene.active().id);
  require(scene.object(id).deformation == scene.objects().front().deformation,
          "Object duplication safely shares immutable rig authoring data");
  scene.clear_deformation(id);
  require(!scene.object(id).deformation && scene.objects().front().deformation,
          "Clearing duplicate rig does not clear original rig");
  auto legacy = loaded.encode();
  legacy["version"] = 3;
  for (auto &o : legacy["objects"])
    o.erase("deformation");
  require(!Scene::decode(legacy).active().deformation,
          "Version-three mesh scenes migrate without invented deformation");
  auto bad = loaded.encode();
  bad["objects"][0]["mesh_active"] = false;
  rejects([&] { Scene::decode(bad); },
          "Deformation on an inactive rest mesh is rejected");
  loaded.active().painted = std::make_shared<PaintedMesh>(stale, 64);
  rejects([&] { loaded.validate(); },
          "Scene validation detects reparameterized/replaced source indices");
  std::filesystem::remove(path);
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
