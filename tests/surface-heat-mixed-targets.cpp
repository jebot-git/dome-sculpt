// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/skin-authoring.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace dome;

namespace {
void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
  std::cout << "PASS " << message << std::endl;
}

uint64_t add_accessory(Scene &scene, uint64_t reference) {
  const auto &ref = scene.object(reference);
  const auto group = ref.group;
  const auto transform = ref.transform;
  const auto affine = ref.affine_edit;
  const auto &rig = *ref.deformation;
  const auto head = rig.skeleton->segments().at(rig.humanoid->joints.at("head"));
  const Vec3 center = head[0] * .25f + head[1] * .75f;
  constexpr float radius = .005f;
  Mesh patch;
  patch.positions = {center + Vec3{-radius, -radius, 0}, center + Vec3{radius, -radius, 0},
                     center + Vec3{radius, radius, 0}, center + Vec3{-radius, radius, 0}};
  patch.normals.assign(4, Vec3{0, 0, 1});
  patch.uv = {{{.1f, .1f}}, {{.9f, .1f}}, {{.9f, .9f}}, {{.1f, .9f}}};
  patch.indices = {0, 1, 2, 0, 2, 3};
  const auto id = scene.add("Native head accessory", Volume{}, transform);
  auto &object = scene.object(id);
  object.group = group;
  object.affine_edit = affine;
  object.painted = std::make_shared<PaintedMesh>(std::move(patch), 64);
  object.mesh_active = true;
  return id;
}
} // namespace

int main(int argc, char **argv) try {
  if (argc != 3) return 2;
  const std::filesystem::path output = argv[2];
  std::filesystem::create_directories(output);
  setenv("XDG_STATE_HOME", (output / "state").c_str(), 1);
  setenv("XDG_CONFIG_HOME", (output / "config").c_str(), 1);

  auto scene = Scene::load(argv[1]);
  uint64_t reference = 0;
  for (const auto &object : scene.objects()) {
    if (object.appearance && object.deformation && object.deformation->skeleton) {
      if (reference || object.deformation->skin || !object.deformation->humanoid)
        throw std::runtime_error("Expected one fitted unskinned source reference");
      reference = object.id;
    }
  }
  require(reference != 0, "genuine retained reference with a fitted unskinned humanoid");
  const auto accessory = add_accessory(scene, reference);
  scene.validate();
  const std::array selected{accessory, reference};
  const std::array reversed{reference, accessory};
  SkinAuthoringOptions options;
  options.mode = SkinAuthoringMode::SurfaceHeat;
  options.reference = reference;

  auto expected = scene;
  for (auto id : selected) {
    auto isolated = scene;
    const std::array target{id};
    require(author_surface_heat_skin(isolated, target, options, {}),
            "isolated public bind produced the expected independent field");
    expected.object(id).deformation = isolated.object(id).deformation;
    expected.object(id).rig_source = isolated.object(id).rig_source;
  }
  const auto expected_values = expected.encode();
  const auto before = scene.encode();
  for (const auto &order : {selected, reversed}) {
    auto mixed = scene;
    require(author_surface_heat_skin(mixed, order, options, {}) && mixed.encode() == expected_values,
            "mixed public bind matches separate fields and changes only the explicit recipients");
  }
  size_t polls = 0;
  bool cancelled = false;
  try {
    author_surface_heat_skin(scene, selected, options, [&] { return ++polls == 64; });
  } catch (const std::runtime_error &error) {
    cancelled = std::string_view(error.what()).find("cancelled") != std::string_view::npos;
  }
  require(cancelled && polls == 64 && scene.encode() == before,
          "cancellation during mixed binding preserves the whole scene");

  Document document;
  document.replace(scene);
  document.select_objects(selected);
  require(document.wait_mesh(std::chrono::seconds(120)), "mixed selection reaches the current Document surface");
  const auto before_document = document.copy_scene();
  before_document.save(output / "unbound.dsculpt");
  expected.select(before_document.active().id);
  const auto file = output / "bound.dsculpt";
  document.bind_skin_selected(options);
  document.save_async(file).get();
  require(document.wait_asset() && document.frame().state->error.empty() &&
              document.copy_scene().encode() == expected.encode() && Scene::load(file).encode() == expected.encode(),
          "one Document bind publishes both fields and its ordered Save reopens exactly");
  document.undo();
  require(document.copy_scene().encode() == before_document.encode() && !document.frame().state->can_undo,
          "one Undo restores the complete pre-bind scene");
  document.redo();
  require(document.copy_scene().encode() == expected.encode(), "Redo restores both bound recipients");
  // The XR controller makes the newly added member active. The desktop chooser
  // keeps the current reference active, so publish both independent endpoints.
  expected.select(accessory);
  expected.save(output / "bound-active-accessory.dsculpt");
  const auto &patch = scene.object(accessory).painted->mesh().positions;
  const auto center = (patch[0] + patch[2]) * .5f;
  const auto contact = scene.world_matrix(accessory).point({center.x, center.y, center.z});
  std::ofstream contact_file(output / "accessory-contact.json");
  contact_file << nlohmann::json(contact) << '\n';
  return 0;
} catch (const std::exception &error) {
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
