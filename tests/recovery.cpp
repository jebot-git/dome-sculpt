// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/recovery.hpp"
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace dome;
using namespace std::chrono_literals;
namespace {
void check(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
template <class F> bool rejects(F &&f) {
  try { f(); return false; } catch (const std::exception &) { return true; }
}
struct Temp {
  std::filesystem::path path;
  Temp() {
    std::string pattern = (std::filesystem::temp_directory_path() / "dome-recovery-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end()); name.push_back(0);
    auto *created = mkdtemp(name.data()); if (!created) throw std::runtime_error("Cannot create test directory");
    path = created;
  }
  ~Temp() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};
struct Gate {
  std::mutex mutex;
  std::condition_variable changed;
  RecoveryPhase target = RecoveryPhase::Serialize;
  bool armed = true, entered = false, released = false;
  void operator()(RecoveryPhase phase) {
    std::unique_lock lock(mutex);
    if (!armed || phase != target) return;
    armed = false; entered = true; changed.notify_all();
    if (!changed.wait_for(lock, 10s, [&] { return released; })) throw std::runtime_error("Test worker gate timed out");
  }
  void wait() {
    std::unique_lock lock(mutex);
    if (!changed.wait_for(lock, 3s, [&] { return entered; })) throw std::runtime_error("Worker did not reach test gate");
  }
  void release() { std::lock_guard lock(mutex); released = true; changed.notify_all(); }
};
Scene fixture() {
  Scene scene;
  auto &v = *scene.active().volume;
  v.begin_stroke(); Brush brush; brush.radius = .12f; v.stamp(brush); v.end_stroke();
  auto detail = v.add_layer("Recovered detail");
  v.begin_stroke(); brush.tool = Tool::Carve; brush.from = brush.to = {.08f, 0, 0}; brush.radius = .06f; v.stamp(brush); v.end_stroke();
  v.layer_properties(detail, "Recovered detail", true, false, .65f);
  Mesh mesh; mesh.positions = {{-1,-1,0},{1,-1,0},{0,1,0}}; mesh.normals.assign(3, {0,0,1});
  mesh.indices = {0,1,2}; mesh.uv = {{{.05f,.05f}},{{.95f,.05f}},{{.5f,.95f}}};
  scene.active().painted = std::make_shared<PaintedMesh>(mesh, 64);
  auto &paint = *scene.active().painted;
  for (auto channel : {PaintChannel::Color, PaintChannel::Metallic, PaintChannel::Roughness, PaintChannel::Emission, PaintChannel::Mask}) {
    PaintSettings settings; settings.channel = channel;
    paint.begin_stroke(); brush.tool = Tool::Clay; brush.from = brush.to = {0,0,0}; brush.radius = .4f;
    paint.stamp(brush, settings); paint.end_stroke();
  }
  scene.active().mesh_active = true;
  scene.active().name = "Žluťoučký sculpt";
  scene.active().material.name = "Recovered PBR";
  scene.active().material.metallic = .7f;
  return scene;
}
void same(const Scene &actual, const Scene &expected) {
  check(actual.active().volume->voxels() == expected.active().volume->voxels() &&
        actual.active().volume->layers().size() == 2 &&
        actual.active().volume->layers()[1].opacity == expected.active().volume->layers()[1].opacity &&
        actual.active().volume->active_layer() == expected.active().volume->active_layer(),
        "Recovery retains editable sculpt layers, composition and selection");
  check(actual.active().painted->encode() == expected.active().painted->encode() &&
        actual.active().name == expected.active().name && actual.active().mesh_active &&
        actual.active().material.metallic == expected.active().material.metallic,
        "Recovery retains original UVs, all paint channels, Unicode and materials");
}
RecoveryRequest request(const RecoverySource &source, uint64_t revision, const Scene &scene) {
  return {source, revision, revision+100, scene.snapshot()};
}
size_t manifests(const std::filesystem::path &root) {
  size_t result = 0;
  for (const auto &e : std::filesystem::directory_iterator(root)) result += e.path().extension() == ".recovery";
  return result;
}
} // namespace
int main() try {
  Temp temp;
  auto scene = fixture();
  RecoverySource source{new_recovery_id(), 1, temp.path / "must-never-be-written/žluťoučký.dsculpt"};
  auto root = temp.path / "checkpoints";
  {
    RecoveryService service(root);
    check(!std::filesystem::exists(root), "Recovery construction does not perform file work");
    check(service.set_source(source) && service.enqueue(request(source, 1, scene)) == RecoveryEnqueue::Accepted,
          "A detached immutable scene is accepted asynchronously");
    check(service.flush() && service.status().error.empty() && service.status().committed == 1,
          "Flush waits for durable scene/metadata publication");
    auto found = RecoveryService::discover(root);
    check(found.errors.empty() && found.candidates.size() == 1 && found.candidates[0].revision == 1 && found.candidates[0].content == 101,
          "Restart discovery finds checked source, revision and content metadata");
    same(RecoveryService::load(found.candidates[0]), scene);
    struct stat st{}; lstat(root.c_str(), &st);
    check((st.st_mode & 0777) == 0700 && !std::filesystem::exists(source.source_path),
          "Recovery uses a private directory and never writes the source project path");
    for (auto &entry : std::filesystem::directory_iterator(root)) {
      lstat(entry.path().c_str(), &st);
      check((st.st_mode & 0777) == 0600, "Recovery files and lock records are private");
    }
    check(service.enqueue(request(source, 1, scene)) == RecoveryEnqueue::Stale,
          "An already committed revision cannot be replayed");
    for (uint64_t revision = 2; revision <= 4; ++revision) {
      service.enqueue(request(source, revision, scene)); check(service.flush(), "Ordered checkpoint finishes");
    }
    check(manifests(root) == 2, "Successful checkpoints retain a bounded previous fallback");
    check(service.discard(source, 3) && service.flush() && RecoveryService::discover(root).candidates[0].revision == 4,
          "A successful older user save cannot delete newer unsaved recovery");
    check(service.discard(source, 4) && service.flush() && RecoveryService::discover(root).candidates.empty(),
          "Explicit saved-revision cleanup removes only covered checkpoints");
    service.shutdown();
    check(service.enqueue(request(source, 5, scene)) == RecoveryEnqueue::Stopped, "Stopped services reject new work");
  }
  {
    auto gate = std::make_shared<Gate>();
    RecoveryOptions options; options.max_pending = 2; options.checkpoint = [gate](auto phase) { (*gate)(phase); };
    auto bounded = temp.path / "bounded";
    RecoveryService service(bounded, options); service.set_source(source);
    auto expected = scene.snapshot(); service.enqueue(request(source, 1, scene)); gate->wait();
    scene.active().name = "New live edits";
    scene.active().volume->layer_properties(scene.active().volume->active_layer(), "New edit", true, false, .25f);
    auto start = std::chrono::steady_clock::now();
    for (uint64_t revision = 2; revision <= 100; ++revision) {
      service.enqueue(request(source, revision, expected));
      if (service.status().pending > 2) throw std::runtime_error("Recovery queue exceeded bound");
    }
    check(service.status().coalesced == 97 && std::chrono::steady_clock::now()-start < 1s,
          "A blocked file worker coalesces bounded snapshots without blocking enqueue");
    check(!service.flush(5ms), "Flush has a bounded timeout while storage is busy");
    gate->release(); check(service.flush(), "Coalesced work drains in original order");
    auto found = RecoveryService::discover(bounded);
    check(found.candidates[0].revision == 100 && manifests(bounded) == 2,
          "The latest accepted immutable snapshot wins after coalescing");
    same(RecoveryService::load(found.candidates[0]), expected);
  }
  {
    auto gate = std::make_shared<Gate>(); gate->target = RecoveryPhase::Commit;
    RecoveryOptions options; options.checkpoint = [gate](auto phase) { (*gate)(phase); };
    auto stale = temp.path / "stale";
    RecoveryService service(stale, options); service.set_source(source);
    service.enqueue(request(source, 9, scene)); gate->wait();
    auto replacement = source; replacement.generation = 2;
    check(service.set_source(replacement), "A document replacement invalidates in-flight old generation work");
    check(service.enqueue(request(source, 10, scene)) == RecoveryEnqueue::Stale && !service.set_source(source),
          "Old source submissions and backwards generations are rejected");
    service.enqueue(request(replacement, 1, scene)); gate->release(); check(service.flush(), "Replacement recovery drains");
    auto found = RecoveryService::discover(stale);
    check(found.candidates.size() == 1 && found.candidates[0].source.generation == 2 && found.candidates[0].revision == 1 && manifests(stale) == 1,
          "An old job at the commit boundary cannot publish over a new generation");
    RecoverySource other{new_recovery_id(), 1, {}};
    service.set_source(other);
    check(!service.discard(replacement, UINT64_MAX), "A stale save completion cannot discard the replacement document");
  }
  {
    auto broken = temp.path / "retry";
    auto moved = temp.path / "retry-moved";
    std::atomic_bool break_storage{false}, fail_prune{false};
    RecoveryOptions options; options.checkpoint = [&](auto phase) {
      if (phase == RecoveryPhase::Serialize && break_storage.exchange(false)) {
        std::filesystem::rename(broken, moved); std::ofstream(broken) << "not a directory";
      }
      if (phase == RecoveryPhase::Prune && fail_prune.exchange(false)) throw std::runtime_error("Injected cleanup failure");
    };
    RecoveryService service(broken, options); service.set_source(source);
    service.enqueue(request(source, 1, scene)); service.flush();
    break_storage = true; service.enqueue(request(source, 2, scene)); service.flush();
    check(service.status().failed == 1 && !service.status().error.empty(),
          "A real filesystem write failure is durable status, not a false flush success");
    std::filesystem::remove(broken); std::filesystem::rename(moved, broken);
    check(RecoveryService::discover(broken).candidates[0].revision == 1, "A failed write preserves the last valid recovery");
    check(service.enqueue(request(source, 2, scene)) == RecoveryEnqueue::Accepted && service.flush() && service.status().error.empty(),
          "An unchanged failed revision can retry successfully without another edit");
    fail_prune = true; service.enqueue(request(source, 3, scene)); service.flush();
    check(service.status().failed == 2 && !service.status().error.empty() && manifests(broken) == 3 &&
          RecoveryService::discover(broken).candidates[0].revision == 3,
          "A cleanup failure reports error and preserves new and previous durable records");
    service.enqueue(request(source, 4, scene)); service.flush();
    check(service.status().error.empty() && manifests(broken) == 2, "A later successful checkpoint repairs retention and error status");
  }
  {
    auto crash = temp.path / "crash";
    { RecoveryService service(crash); service.set_source(source); service.enqueue(request(source, 1, scene)); service.flush(); }
    auto child = fork(); if (child < 0) throw std::runtime_error("fork failed");
    if (child == 0) {
      RecoveryOptions options; options.checkpoint = [](auto phase) { if (phase == RecoveryPhase::Commit) _exit(77); };
      RecoveryService service(crash, options); service.set_source(source); service.enqueue(request(source, 2, scene)); service.flush(); _exit(78);
    }
    int status = 0; waitpid(child, &status, 0);
    check(WIFEXITED(status) && WEXITSTATUS(status) == 77 && RecoveryService::discover(crash).candidates[0].revision == 1,
          "Process death before atomic metadata publication leaves previous recovery discoverable");
    { RecoveryService service(crash); service.set_source(source); service.enqueue(request(source, 2, scene)); service.flush(); }
    auto newest = RecoveryService::discover(crash).candidates[0];
    auto data = newest.root / newest.metadata; data.replace_extension(".dsculpt");
    { std::fstream bytes(data, std::ios::in|std::ios::out|std::ios::binary); char value; bytes.read(&value, 1); value ^= 1; bytes.seekp(0); bytes.write(&value, 1); }
    auto found = RecoveryService::discover(crash);
    check(!found.errors.empty() && found.candidates[0].revision == 1 && rejects([&] { RecoveryService::load(newest); }),
          "Corrupt newest data reports an error, rejects load and falls back to a valid previous checkpoint");
    std::filesystem::create_symlink(temp.path / "never-read", crash / "symlink.recovery");
    mkfifo((crash / "fifo.recovery").c_str(), 0600);
    check(RecoveryService::discover(crash).errors.size() >= 3, "Symlink and FIFO recovery records are rejected without blocking");
    auto alias = temp.path / "alias"; std::filesystem::create_directory_symlink(crash, alias);
    check(!RecoveryService::discover(alias).errors.empty(), "A symlink recovery directory is rejected");
    RecoveryOptions limited; limited.max_scan_entries = 1;
    check(RecoveryService::discover(crash, limited).truncated, "Restart discovery has a bounded directory scan and explicit truncation");
  }
  {
    auto limited = temp.path / "size";
    RecoveryOptions options; options.max_file_bytes = 64;
    RecoveryService service(limited, options); service.set_source(source); service.enqueue(request(source, 1, scene)); service.flush();
    check(service.status().failed == 1 && !service.status().error.empty() && RecoveryService::discover(limited).candidates.empty(),
          "Oversized recovery files fail explicitly without publishing partial metadata");
    check(rejects([&] { service.set_source({"../escape", 1, {}}); }), "Untrusted source IDs cannot escape the recovery directory");
  }
  std::cout << "Recovery native tests complete\n";
} catch (const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
