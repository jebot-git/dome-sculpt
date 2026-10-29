// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <fstream>
#include <future>
#include <iostream>
#include <unistd.h>
using namespace dome;
int main() try {
  auto require = [](bool v, const char *name) {
    if (!v)
      throw std::runtime_error(name);
    std::cout << "PASS " << name << '\n';
  };
  Document doc;
  doc.seed();
  require(doc.wait_mesh(), "Mesher publishes an initial immutable surface");
  auto first = doc.frame();
  require(!first.state->dirty && !first.surface->mesh.indices.empty(),
          "A clean document has a rendered surface");
  auto held = first.surface;
  require(doc.begin_desktop(),
          "Desktop acquires an exclusive sculpt transaction");
  Brush b;
  b.to = b.from = {0, 0, .35};
  b.radius = .1;
  b.tool = Tool::Carve;
  doc.stamp_desktop(b);
  doc.end_desktop();
  require(doc.wait_mesh(),
          "Background mesher catches up after a desktop stroke");
  auto edited = doc.frame();
  require(edited.state->dirty && edited.surface->revision > held->revision,
          "Model revisions and mesh snapshots advance together");
  require(held->mesh.indices == first.surface->mesh.indices,
          "An acquired render snapshot remains immutable across edits");
  require(doc.undo() && doc.wait_mesh(),
          "Shared undo regenerates the earlier surface");
  require(doc.frame().surface->mesh.positions.size() ==
              first.surface->mesh.positions.size(),
          "Shared undo returns the original geometry");
  SpatialFrame f;
  f.sequence = 1;
  f.time = 1;
  f.focused = f.head_valid = true;
  f.head.position = {0, 1.6f, 0};
  f.hands[1].tracked.valid = true;
  f.hands[1].tracked.grip.position = {0, 1.48f, -.5f};
  doc.submit(f);
  require(doc.wait_input(), "Spatial input worker consumes a baseline sample");
  ++f.sequence;
  f.time += .01;
  f.hands[1].tracked.trigger = 1;
  doc.submit(f);
  require(doc.wait_input(), "Spatial worker applies a pressure-driven stroke");
  require(!doc.begin_desktop(),
          "Desktop cannot steal an active controller transaction");
  auto live = doc.frame();
  require(live.state->stroking,
          "A tracked stroke is visible to both frontends");
  auto responsive = std::async(std::launch::async, [&] {
    for (int i = 0; i < 10000; ++i)
      doc.frame();
  });
  require(responsive.wait_for(std::chrono::seconds(1)) ==
              std::future_status::ready,
          "Render snapshot reads do not wait for sculpt or mesh work");
  ++f.sequence;
  f.time += .01;
  f.focused = false;
  doc.submit(f);
  require(doc.wait_input() && !doc.frame().state->stroking,
          "Focus loss cancels the in-progress spatial stroke");
  require(!doc.frame().state->dirty,
          "Cancelling transient edits does not dirty the restored saved state");
  auto room_before = doc.frame().state->canvas;
  doc.rebase_space(CanvasTransform{{.2f, 0, 0}, {}, 1});
  auto room_after = doc.frame().state->canvas;
  require(std::abs(room_after.position.x - (room_before.position.x - .2f)) <
              1e-5f,
          "Reference-space translation rebases the canvas rather than moving "
          "the artwork in the room");
  require(!doc.frame().state->stroking &&
              doc.frame().state->input_epoch > live.state->input_epoch,
          "Reference-space changes cancel transient edits and invalidate "
          "queued old-space input");
  auto old_generation = doc.frame().state->generation;
  doc.replace(Volume{});
  require(doc.wait_mesh(),
          "Replacing the document publishes its new mesh generation");
  auto empty = doc.frame();
  require(empty.state->generation > old_generation &&
              empty.surface->mesh.indices.empty(),
          "Old geometry cannot appear in a new document generation");
  auto directory = std::filesystem::temp_directory_path() /
                   ("dome-document-" + std::to_string(getpid()));
  std::filesystem::create_directory(directory);
  Volume::Voxels newest;
  {
    Document writing;
    writing.seed();
    auto target = directory / "saved.dsculpt";
    writing.save(target);
    auto original = Scene::load(target).active().volume->voxels();
    require(writing.begin_desktop(), "A saved document can start a new stroke");
    writing.stamp_desktop(b);
    writing.end_desktop();
    std::filesystem::permissions(directory,
                                 std::filesystem::perms::owner_read |
                                     std::filesystem::perms::owner_exec);
    writing.request_save();
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (writing.frame().state->error.empty() &&
           std::chrono::steady_clock::now() < until)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    std::filesystem::permissions(directory, std::filesystem::perms::owner_all);
    require(!writing.frame().state->error.empty() &&
                writing.frame().state->dirty,
            "A background save failure keeps the changed document dirty");
    require(Scene::load(target).active().volume->voxels() == original,
            "A failed background save preserves the previous project");
    writing.request_save();
    until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (writing.frame().state->dirty &&
           std::chrono::steady_clock::now() < until)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    require(!writing.frame().state->dirty &&
                writing.frame().state->error.empty(),
            "Retrying a failed save publishes success and clears its error");
    writing.request_save(true);
    b.from = b.to = {.15f, 0, .3f};
    require(writing.begin_desktop(),
            "Editing continues while a glTF export is queued");
    writing.stamp_desktop(b);
    writing.end_desktop();
    writing.request_save();
    b.from = b.to = {-.15f, 0, .3f};
    require(writing.begin_desktop(),
            "A second edit can follow an asynchronous save");
    writing.stamp_desktop(b);
    writing.end_desktop();
    newest = writing.copy_volume().voxels();
    writing.save(target);
    require(!writing.frame().state->dirty,
            "A synchronous save waits behind older file jobs");
  }
  require(Scene::load(directory / "saved.dsculpt").active().volume->voxels() ==
              newest,
          "An older queued XR save cannot overwrite a newer desktop save");
  std::filesystem::remove_all(directory);
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
