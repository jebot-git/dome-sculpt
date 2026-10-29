// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace dome;
using namespace std::chrono_literals;
namespace {
void check(bool ok, const char *label) {
  if (!ok) throw std::runtime_error(label);
  std::cout << "PASS " << label << '\n';
}
struct Gate {
  std::mutex mutex;
  std::condition_variable changed;
  bool entered = false, released = false;
  void worker() {
    std::unique_lock lock(mutex); entered = true; changed.notify_all();
    if (!changed.wait_for(lock, 10s, [&] { return released; }))
      throw std::runtime_error("Test gate timed out");
  }
  void wait() {
    std::unique_lock lock(mutex);
    if (!changed.wait_for(lock, 3s, [&] { return entered; }))
      throw std::runtime_error("Worker did not reach gate");
  }
  void release() { std::lock_guard lock(mutex); released = true; changed.notify_all(); }
  ~Gate() { release(); }
};
}
int main() try {
  Document doc;
  doc.seed();
  auto source = doc.frame().state;
  AssetSource expected{source->generation, source->revision, source->active_object};
  doc.seed();
  bool stale = false;
  try { doc.edit_scene("Stale editor", [](Scene &, const auto &) {}, expected); }
  catch (const std::logic_error &) { stale = true; }
  check(stale && !doc.frame().state->asset_busy,
        "Editors cannot submit against a replacement document with reused object IDs");
  auto main_thread = std::this_thread::get_id();
  std::thread::id worker_thread;
  doc.edit_scene("Create test object", [&](Scene &scene, const auto &) {
    worker_thread = std::this_thread::get_id();
    scene.add("Worker object", Volume{});
  });
  check(doc.wait_asset(), "Asset worker publishes completion");
  check(worker_thread != main_thread && doc.frame().state->objects.size() == 2,
        "Scene asset creation runs off the caller and commits one transaction");
  check(doc.undo() && doc.frame().state->objects.size() == 1 && doc.redo() &&
        doc.frame().state->objects.size() == 2,
        "Worker scene operations share exact global undo and redo");
  std::shared_ptr<Volume> escaped;
  doc.edit_scene("Detached result", [&](Scene &scene, const auto &) {
    escaped = scene.active().volume;
  });
  check(doc.wait_asset(), "No-op worker result completes");
  auto immutable = doc.copy_volume().voxels();
  escaped->begin_stroke();
  Brush escape_brush; escape_brush.radius = .1f;
  escaped->stamp(escape_brush); escaped->end_stroke();
  check(doc.copy_volume().voxels() == immutable,
        "Published assets detach mutable callback aliases");
  auto before = doc.copy_volume().voxels();
  doc.edit_scene("Fail transaction", [](Scene &scene, const auto &) {
    scene.add("Never published", Volume{});
    throw std::runtime_error("Expected asset failure");
  });
  check(doc.wait_asset() && doc.frame().state->objects.size() == 2 &&
        doc.copy_volume().voxels() == before && doc.frame().state->error == "Expected asset failure",
        "Failed operations expose errors without publishing partial scenes");
  auto gate = std::make_shared<Gate>();
  struct ReleaseGate { std::shared_ptr<Gate> gate; ~ReleaseGate() { gate->release(); } } release{gate};
  doc.edit_scene("Slow transaction", [gate](Scene &scene, const auto &cancelled) {
    scene.add("Stale object", Volume{});
    gate->worker();
    if (cancelled()) throw std::runtime_error("Source changed");
  });
  gate->wait();
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 10000; ++i) doc.frame();
  check(std::chrono::steady_clock::now() - start < 1s,
        "Rendering snapshots stay responsive during worker operations");
  doc.add_object(false); // Deliberate API-side concurrent source edit.
  gate->release();
  check(doc.wait_asset() && doc.frame().state->objects.size() == 3,
        "Concurrent scene changes reject a stale asset result");
  auto directory = std::filesystem::temp_directory_path() /
      ("dome-assets-" + std::to_string(getpid()));
  std::filesystem::create_directory(directory);
  struct Cleanup { std::filesystem::path path; ~Cleanup() {
    std::error_code ec; std::filesystem::remove_all(path, ec);
  }} cleanup{directory};
  auto path = directory / "triangle.obj";
  { std::ofstream file(path); file << "o Imported triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0.1 0.1\nvt 0.9 0.1\nvt 0.1 0.9\nf 1/1 2/2 3/3\n"; }
  doc.import_asset(path, 64);
  check(doc.wait_asset() && doc.frame().state->error.empty() &&
        doc.frame().state->objects.size() == 4 && doc.frame().state->mesh_active,
        "Native OBJ import creates a paintable object on the worker");
  doc.edit_scene("Paint imported fixture", [](Scene &scene, const auto &) {
    auto &paint = *scene.active().painted;
    paint.begin_stroke();
    Brush brush; brush.from = brush.to = {.2f,.2f,0}; brush.radius = .4f;
    brush.normal = {0,0,1};
    paint.stamp(brush, PaintSettings{});
    paint.end_stroke();
  });
  check(doc.wait_asset(), "Imported paint fixture completes");
  auto painted_scene = doc.copy_scene();
  auto paint_before = painted_scene.active().painted->composite(painted_scene.active().material)->color.rgba;
  bool protected_paint = false;
  try { doc.prepare_mesh(64); } catch (const std::logic_error &) { protected_paint = true; }
  auto retained = doc.copy_scene();
  check(protected_paint && retained.active().painted->composite(retained.active().material)->color.rgba == paint_before,
        "Regenerating a source mesh cannot silently discard existing paint");
  check(doc.undo(), "Paint fixture shares global history");
  check(doc.copy_scene().active().painted->mesh().indices.size() == 3 &&
        doc.undo() && doc.frame().state->objects.size() == 3,
        "Imported geometry is preserved and import undoes as one operation");
  auto bad = directory / "textured.obj";
  { Image image{1,1,{255,0,0,255}};
    auto bytes = image.png();
    std::ofstream texture(directory / "color.png", std::ios::binary);
    texture.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    std::ofstream material(directory / "input.mtl");
    material << "newmtl textured\nmap_Kd color.png\n";
    std::ofstream file(bad); file << "mtllib input.mtl\nusemtl textured\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"; }
  doc.import_asset(bad,64);
  check(doc.wait_asset() && doc.frame().state->objects.size() == 3 &&
        doc.frame().state->error.find("Textured material authoring") != std::string::npos,
        "Unsupported textured materials never produce lossy partial import");
  return 0;
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
