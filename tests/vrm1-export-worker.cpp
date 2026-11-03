// SPDX-License-Identifier: GPL-3.0-or-later
#include "cgltf.h"
#include "dome/document.hpp"
#include "import-controller-driver.hpp"
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks = 0;
std::atomic_bool armed = false;
std::mutex mutex;
std::condition_variable changed;
bool entered = false, released = false;
void check(bool ok, const char *m) {
  if (!ok)
    throw std::runtime_error(m);
  ++checks;
}
void release() {
  std::lock_guard lock(mutex);
  released = true;
  armed = false;
  changed.notify_all();
}
std::vector<uint8_t> bytes(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
void ready(Document &d) {
  check(d.wait_asset(std::chrono::seconds(90)) &&
            d.wait_mesh(std::chrono::seconds(90)),
        "Actual source import/export workers settle");
  if (!d.frame().state->error.empty())
    throw std::runtime_error(d.frame().state->error);
}
bool cancelled(std::future<void> &f) {
  if (f.wait_for(std::chrono::seconds(90)) != std::future_status::ready)
    throw std::runtime_error("Rich VRM cancellation completion timed out");
  try {
    f.get();
  } catch (const std::exception &e) {
    return std::string_view(e.what()).find("cancelled") !=
           std::string_view::npos;
  }
  return false;
}
} // namespace
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  int result = __real_fsync(fd);
  if (result || !armed.load())
    return result;
  char link[64], path[4096];
  snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
  auto count = readlink(link, path, sizeof(path));
  if (count <= 0 ||
      std::string_view(path, size_t(count)).find("cancelled.vrm.tmp-") ==
          std::string_view::npos)
    return result;
  bool expected = true;
  if (armed.compare_exchange_strong(expected, false)) {
    std::unique_lock lock(mutex);
    entered = true;
    changed.notify_all();
    changed.wait(lock, [] { return released; });
  }
  return result;
}
int main(int argc, char **argv) try {
  if (argc != 2)
    return 2;
  auto home = std::filesystem::temp_directory_path() /
              ("dome-vrm-worker-" + std::to_string(getpid()));
  std::filesystem::create_directories(home);
  setenv("HOME", home.c_str(), 1);
  setenv("XDG_CONFIG_HOME", home.c_str(), 1);
  setenv("XDG_STATE_HOME", home.c_str(), 1);
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      release();
      std::error_code ec;
      std::filesystem::remove_all(path, ec);
    }
  } cleanup{home};
  Document doc;
  struct Release {
    ~Release() { release(); }
  } guard;
  doc.replace(Scene{});
  ready(doc);
  auto baseline = doc.copy_scene().encode();
  doc.import_asset(argv[1], 64);
  ready(doc);
  auto source = doc.copy_scene().encode();
  check(doc.copy_scene().objects().size() == 4 &&
            doc.frame().state->can_export_avatar,
        "Generic native VRM1 import enables actual version-preserving avatar "
        "export for the entire source component");
  auto one = home / "cancelled.vrm", two = home / "queued.vrm",
       project = home / "ordered.dsculpt";
  for (auto &p : {one, two}) {
    std::ofstream f(p);
    f << "preserved output";
  }
  auto previous = bytes(one);
  armed = true;
  auto running = doc.export_asset(one, AssetExportFormat::Avatar);
  {
    std::unique_lock lock(mutex);
    if (!changed.wait_for(lock, std::chrono::seconds(90),
                          [] { return entered; })) {
      lock.unlock();
      if (running.wait_for(std::chrono::seconds(0)) ==
          std::future_status::ready)
        running.get();
      throw std::runtime_error(
          "Actual imported VRM did not reach durable temporary barrier");
    }
  }
  auto queued = doc.export_asset(two, AssetExportFormat::Avatar);
  auto save = doc.save_async(project);
  check(doc.frame().state->pending_exports == 2 &&
            doc.frame().state->can_cancel_asset,
        "Actual file worker exposes both accepted source-preserving VRM "
        "exports as cancellable snapshots");
  ImportDriver controller(doc);
  controller.open();
  controller.sector(8);
  controller.sector(9);
  controller.sector(6);
  check(doc.frame().state->menu_page == 12 &&
            doc.frame().state->menu_enabled[5],
        "Actual joystick navigation enables snapshot-export Cancel on the "
        "baking page for the imported source-version VRM1 avatar");
  controller.sector(5);
  release();
  check(cancelled(running) && cancelled(queued),
        "Explicit native Cancel reports cancellation for running and queued "
        "VRM completion promises");
  save.get();
  ready(doc);
  check(bytes(one) == previous && bytes(two) == previous &&
            doc.copy_scene().encode() == source &&
            Scene::load(project).encode() == source,
        "Cancel retains prior output, exact complete source/history and "
        "following ordered project Save");
  for (auto &entry : std::filesystem::directory_iterator(home))
    check(entry.path().filename().string().find(".tmp-") == std::string::npos,
          "Cancelled output leaves no temporary files");
  auto retry = doc.export_asset(one, AssetExportFormat::Avatar);
  if (retry.wait_for(std::chrono::seconds(90)) != std::future_status::ready)
    throw std::runtime_error("VRM retry timed out");
  retry.get();
  ready(doc);
  cgltf_options options{};
  cgltf_data *raw = nullptr;
  if (cgltf_parse_file(&options, one.c_str(), &raw) != cgltf_result_success)
    throw std::runtime_error("Retry emitted unparsable VRM");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> output(raw, cgltf_free);
  check(raw->meshes_count == 3,
        "Retry through actual native export worker retains actual skins and "
        "the genuine "
        "rigid source attachment rather than only active leaf group");
  check(doc.undo() && doc.copy_scene().encode() == baseline && doc.redo() &&
            doc.copy_scene().encode() == source,
        "Export and cancellation add no authoring history; original import "
        "remains exactly one Undo/Redo");
  std::cout
      << checks
      << " actual source-version VRM1 worker Cancel/retry/orderedSave/history "
         "checks passed\n";
  return 0;
} catch (const std::exception &e) {
  release();
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
