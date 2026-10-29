// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
int main() try {
  auto require = [](bool v, const char *s) {
    if (!v)
      throw std::runtime_error(s);
    std::cout << "PASS " << s << '\n';
  };
  auto rejects = [&](nlohmann::json j, const char *s) {
    bool failed = false;
    try {
      Scene::decode(j);
    } catch (const std::exception &) {
      failed = true;
    }
    require(failed, s);
  };
  Document doc;
  doc.seed();
  auto first = doc.copy_scene();
  auto original = first.active().volume->voxels();
  auto id = doc.add_object();
  require(doc.wait_mesh() && doc.frame().surface->ranges.size() == 2,
          "Two independent objects are meshed into the shared preview");
  auto scene = doc.copy_scene();
  auto &o = scene.active();
  auto t = o.transform;
  t.position = {.5f, .1f, 0};
  t.scale = .75f;
  Material blue;
  blue.name = "Blue metal";
  blue.color = {.04f, .12f, .8f, 1};
  blue.metallic = .85f;
  blue.roughness = .2f;
  doc.object_properties(id, "Metal piece", true, false, t, blue);
  scene = doc.copy_scene();
  auto file = std::filesystem::temp_directory_path() /
              ("dome-scene-" + std::to_string(getpid()) + ".dsculpt");
  doc.save(file);
  require(!doc.frame().state->dirty,
          "Saving establishes a clean scene history position");
  auto loaded = Scene::load(file);
  require(loaded.objects().size() == 2 && loaded.active().id == id &&
              loaded.active().name == "Metal piece",
          "Scene persistence restores object identities, names and selection");
  require(loaded.active().material.color == blue.color &&
              loaded.active().material.metallic == blue.metallic &&
              loaded.active().transform.position.x == .5f &&
              loaded.active().transform.scale == .75f,
          "Material values and object transforms survive a native save/reopen");
  doc.object_properties(id, "Metal piece", false, true, t, blue);
  require(!doc.begin_desktop() && doc.wait_mesh() &&
              doc.frame().surface->ranges.size() == 1,
          "Hidden and locked objects cannot be sculpted and hidden surfaces "
          "leave the preview");
  require(doc.undo() && doc.frame().state->objects.back().visible,
          "Scene undo restores object visibility and locking");
  require(!doc.frame().state->dirty, "Undoing metadata changes back to the "
                                     "saved scene clears the unsaved marker");
  require(doc.begin_desktop(),
          "Visible unlocked active objects acquire the brush");
  Brush b;
  b.tool = Tool::Carve;
  b.from = b.to = {.5f, .1f, .18f};
  b.radius = .1f;
  doc.stamp_desktop(b);
  doc.end_desktop();
  auto edited = doc.copy_volume().voxels();
  require(
      edited != loaded.active().volume->voxels() &&
          doc.copy_scene().objects().front().volume->voxels() == original,
      "A transformed object receives the brush without modifying its neighbor");
  require(doc.undo() &&
              doc.copy_volume().voxels() == loaded.active().volume->voxels(),
          "Global history undoes a transformed sculpt stroke exactly");
  require(doc.redo() && doc.copy_volume().voxels() == edited,
          "Global history redoes the transformed sculpt stroke");
  doc.duplicate_object();
  auto copy = doc.frame().state->active_object;
  doc.delete_object();
  require(doc.copy_scene().objects().size() == 2 && doc.undo() &&
              doc.copy_scene().active().id == copy,
          "Deleted objects and their stable identity can be restored");
  require(doc.undo() && doc.copy_scene().objects().size() == 2,
          "Duplication participates in the same scene history as strokes");
  require(doc.undo() && doc.copy_scene().object(id).volume->voxels() ==
                            loaded.active().volume->voxels(),
          "Undo crosses object creation/deletion and returns to the earlier "
          "sculpt state");
  doc.select_object(id);
  doc.duplicate_object();
  require(!doc.redo(),
          "A new object command discards the previous global redo branch");
  auto before = doc.copy_scene();
  auto detached = before.snapshot();
  b.from = b.to = {0, 0, -.18f};
  before.active().volume->begin_stroke();
  before.active().volume->stamp(b);
  before.active().volume->end_stroke();
  require(detached.active().volume->voxels() !=
              before.active().volume->voxels(),
          "Worker snapshots own detached immutable object geometry");
  auto j = loaded.encode();
  j["objects"][1]["id"] = j["objects"][0]["id"];
  rejects(j, "Duplicate stable object IDs are rejected");
  j = loaded.encode();
  j["objects"][1]["scale"] = 0;
  rejects(j, "Singular object transforms are rejected");
  j = loaded.encode();
  j["objects"][1]["rotation"] = {0, 0, 0, 2};
  rejects(j, "Non-unit object rotations are rejected");
  j = loaded.encode();
  j["objects"][1]["material"]["roughness"] = -1;
  rejects(j, "Invalid material channel ranges are rejected");
  j = loaded.encode();
  j["active"] = 999;
  rejects(j, "Missing active objects are rejected");
  auto legacy = loaded.encode();
  legacy["version"] = 2;
  for (auto &object : legacy["objects"]) {
    object.erase("mesh_active");
    object.erase("painted");
  }
  auto migrated = Scene::decode(legacy);
  require(migrated.objects().size() == 2 &&
              migrated.active().material.color == blue.color &&
              migrated.active().volume->voxels() ==
                  loaded.active().volume->voxels() &&
              !migrated.active().painted && !migrated.active().mesh_active,
          "Version-two scenes retain material and source geometry without "
          "painting assets");
  first.active().volume->save(file);
  loaded = Scene::load(file);
  require(loaded.objects().size() == 1 &&
              loaded.active().volume->voxels() == original,
          "Version-one volume projects upgrade to a scene without geometry "
          "changes");
  std::filesystem::remove(file);
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
