// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene-file.hpp"
#include "dome/document.hpp"
#include "dome/emission-native.hpp"
#include "dome/material-bindings.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
unsigned checks = 0;
void check(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
  ++checks;
  std::cout << "PASS " << message << std::endl;
}
bool refuses(auto fn) {
  try { fn(); } catch (const std::exception &e) {
    std::cout << "REFUSAL " << e.what() << std::endl;
    return true;
  }
  return false;
}
std::vector<uint8_t> read(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), {}};
}
void complete(std::future<void> work) { work.get(); }
Scene fixture() {
  Scene scene;
  Mesh mesh;
  mesh.positions = {{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};
  mesh.normals.assign(4, {0,0,1});
  mesh.indices = {0,1,2,0,2,3};
  mesh.uv = {{{0,0}},{{1,0}},{{1,1}},{{0,1}}};
  auto &object = scene.active();
  object.name = "Editable physical source";
  object.mesh_active = true;
  object.painted = std::make_shared<PaintedMesh>(std::move(mesh), 64);
  object.material.emissive = {10000,.005f,.0003f};
  auto binding = std::make_shared<SourceMaterialBindings>();
  binding->has_mtoon = false;
  binding->physical_emission = object.material.emissive;
  object.material_bindings = std::move(binding);
  Brush brush;
  brush.from = brush.to = {0,0,0};
  brush.normal = {0,0,1};
  brush.radius = .2f;
  brush.strength = .7f;
  PaintSettings settings;
  settings.channel = PaintChannel::Emission;
  settings.emission_units = PaintEmissionUnits::Physical;
  settings.emission = {12,.005f,10000};
  settings.front_only = false;
  object.painted->begin_stroke();
  object.painted->stamp(brush, settings);
  check(object.painted->end_stroke(), "Native package fixture contains actual sparse physical paint");
  auto hidden = scene.duplicate(object.id);
  scene.object(hidden).visible = false;
  scene.object(hidden).name = "Hidden editable object";
  scene.validate();
  return scene;
}
}
int main(int argc, char **argv) try {
  if (argc != 2) return 2;
  const std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out);
  auto scene = fixture();
  const auto exact = scene.encode();
  const auto project = out/"original.dsculpt";
  scene.save(project);
  const auto original_project = read(project);
  auto ordinary = load_scene_file(project);
  check(ordinary.kind == SceneFileKind::Project && ordinary.scene.encode() == exact,
        "Ordinary native project retains exact content and project kind");
  Document exporter;
  exporter.replace(scene.snapshot());
  const auto package = out/"editable.glb";
  complete(exporter.export_asset(package));
  const auto original_package = read(package);
  auto restored = load_scene_file(package);
  check(restored.kind == SceneFileKind::AuthoringPackage && restored.scene.encode() == exact,
        "Actual Document HDR export opens as exact editable package, including hidden objects");
  check(Scene::load(package).encode() == exact,
        "Shared Scene loader restores authenticated native package content");
  const auto disguised = out/"package.dsculpt";
  std::filesystem::copy_file(package, disguised, std::filesystem::copy_options::overwrite_existing);
  check(load_scene_file(disguised).kind == SceneFileKind::AuthoringPackage,
        "Actual container content determines save policy independently of filename extension");
  Document target;
  complete(target.open_async(project));
  check(target.frame().state->project_path == project && !target.frame().state->dirty,
        "Ordinary asynchronous Open keeps the existing saved-project identity");
  complete(target.open_async(package));
  check(target.copy_scene().encode() == exact && target.frame().state->project_path.empty() &&
        target.frame().state->dirty && !target.frame().state->asset_busy,
        "Actual asynchronous package Open restores an unsaved project without adopting the interchange path");
  const auto saved = out/"restored.dsculpt";
  complete(target.save_async(saved));
  check(Scene::load(saved).encode() == exact && !target.frame().state->dirty &&
        target.frame().state->project_path == saved && read(package) == original_package,
        "Subsequent native Save writes an independent exact project and preserves the original GLB");
  auto corrupt = original_package;
  corrupt.back() ^= 1;
  const auto bad = out/"corrupt.vrm";
  { std::ofstream stream(bad,std::ios::binary); stream.write(reinterpret_cast<const char*>(corrupt.data()),corrupt.size()); }
  const auto before = target.copy_scene().encode();
  check(refuses([&]{complete(target.open_async(bad));}) && target.copy_scene().encode() == before &&
        target.frame().state->project_path == saved && !target.frame().state->asset_busy,
        "Corrupt package Open preserves the current scene, project identity and original file");
  Scene plain;
  plain.active().mesh_active = true;
  plain.active().painted = std::make_shared<PaintedMesh>(scene.object(1).painted->mesh(),64);
  Document raw;
  raw.replace(plain.snapshot());
  const auto model = out/"ordinary-model.glb";
  complete(raw.export_asset(model));
  check(refuses([&]{load_scene_file(model);}) &&
        refuses([&]{complete(target.open_async(model));}) && target.copy_scene().encode() == before &&
        target.frame().state->project_path == saved,
        "A standard model without native data stays on the Import workflow and cannot replace the project");
  size_t visits = 0;
  check(load_scene_file(package,[&]{++visits;return false;}).scene.encode() == exact && visits > 3,
        "Bounded package reader carries cancellation through file, authentication and native decoding work");
  for (const auto endpoint : {size_t(1), visits/2, visits-1}) {
    size_t seen = 0;
    check(refuses([&]{load_scene_file(package,[&]{return ++seen >= endpoint;});}) &&
          read(package) == original_package && target.copy_scene().encode() == before,
          "Early, intermediate and final package cancellation never mutates the package or current document");
  }
  const auto large = out/"over-limit.dsculpt";
  { std::ofstream stream(large,std::ios::binary); stream.seekp(256*1024*1024); stream.put('x'); }
  check(refuses([&]{load_scene_file(large);}) && target.copy_scene().encode() == before,
        "An oversized sparse input is refused before file-buffer allocation");
  check(read(project) == original_project && read(package) == original_package && scene.encode() == exact,
        "Open, failure, Save and cancellation leave both source files and the original owner byte-exact");
  std::cout << "Native scene file: " << checks << " checks\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
