// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/skin-authoring.hpp"
#define main native_reference_fixture_main
#define fixture native_reference_fixture
#include "geometry-weight-reference.cpp"
#undef fixture
#undef main
#include "dome/document.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
using namespace dome;
namespace {
thread_local bool tracking = false;
size_t calls = 0, boundary = SIZE_MAX;
bool hit = false;
void check(bool value, const char *why) {
  if (!value)
    throw std::runtime_error(why);
}
struct Gate {
  std::mutex mutex;
  std::condition_variable changed;
  bool armed = false, entered = false;
  void arm() {
    std::lock_guard l(mutex);
    armed = true;
    entered = false;
  }
  void stop() {
    std::unique_lock l(mutex);
    if (!armed)
      return;
    entered = true;
    changed.notify_all();
    changed.wait(l, [&] { return !armed; });
  }
  void wait() {
    std::unique_lock l(mutex);
    if (!changed.wait_for(l, std::chrono::seconds(60), [&] { return entered; }))
      throw std::runtime_error(
          "Preceding actual Save did not reach fsync gate");
  }
  void release() {
    std::lock_guard l(mutex);
    armed = false;
    changed.notify_all();
  }
} gate;
struct Release {
  ~Release() {
    tracking = false;
    gate.release();
  }
};
auto settings(const SkinAuthoringOptions &o) {
  return std::tuple{o.reference,
                    o.mode,
                    o.limits.max_influences,
                    o.limits.prune,
                    o.seed_distance,
                    o.seed_margin,
                    o.falloff,
                    o.transfer_distance,
                    o.maximum_work,
                    o.locks,
                    o.source_visible.maximum_work,
                    o.source_visible.maximum_workspace,
                    o.native_visible.maximum_work,
                    o.native_visible.maximum_workspace};
}
Scene fixture() {
  auto s = native_reference_fixture();
  SkinAuthoringOptions options;
  options.mode = SkinAuthoringMode::NativeVisible;
  options.reference = 1;
  check(author_native_visible_skin(s, std::array<uint64_t, 1>{2}, options),
        "Real native baseline binds original unskinned target before lock "
        "faults");
  return s;
}
std::unique_ptr<Document> setup() {
  static const auto source = fixture();
  auto d = std::make_unique<Document>();
  d->replace(source.snapshot());
  check(d->wait_mesh(), "Initial mesh is ready before caller faults");
  return d;
}
SkinAuthoringOptions candidate(const Document &d) {
  auto scene = d.copy_scene();
  auto o = d.frame().state->skin_options;
  o.mode = SkinAuthoringMode::NativeVisible;
  o.reference = 1;
  o.limits = {2, .0002f};
  o.seed_distance = .23f;
  o.seed_margin = .011f;
  o.transfer_distance = .089f;
  o.falloff = 3;
  o.native_visible.maximum_work = 490'000'000ULL;
  o.native_visible.maximum_workspace = 192u * 1024u * 1024u;
  o.locks.assign(scene.active().deformation->skeleton->bones.size(), 1);
  return o;
}
} // namespace
[[gnu::noinline]] void *operator new(size_t n) {
  if (tracking && ++calls >= boundary) {
    hit = true;
    throw std::bad_alloc();
  }
  if (auto p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
[[gnu::noinline]] void *operator new[](size_t n) { return ::operator new(n); }
[[gnu::noinline]] void operator delete(void *p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void *p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void *p, size_t) noexcept {
  std::free(p);
}
[[gnu::noinline]] void operator delete[](void *p, size_t) noexcept {
  std::free(p);
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  struct stat s{};
  if (!fstat(fd, &s) && S_ISREG(s.st_mode))
    gate.stop();
  return __real_fsync(fd);
}
namespace dome {
struct DocumentTransactionTestAccess {
  static auto tokens(const Document &d) {
    std::lock_guard l(d.model_mutex_);
    return std::tuple{d.content_,           d.next_content_,
                      d.saved_content_,     d.undo_.size(),
                      d.redo_.size(),       d.state_.revision,
                      d.state_.generation,  d.state_.edit_authority,
                      d.state_.held_object, d.state_.selected_objects};
  }
  static auto leases(const Document &d) {
    std::lock_guard l(d.model_mutex_);
    return std::tuple{
        d.native_source_scene_,       d.asset_work_.load(),
        d.state_.asset_busy,          d.control_before_.has_value(),
        d.control_source_.generation, d.control_source_.revision,
        d.control_source_.object,     d.control_source_.edit_authority};
  }
  static auto controls(const Document &d) {
    std::lock_guard l(d.model_mutex_);
    return std::tuple{settings(d.state_.skin_options), d.state_.skin_use_locks,
                      d.state_.weights.locks};
  }
  static auto input(const Document &d) {
    std::lock_guard l(d.model_mutex_);
    auto p = d.input_.menu_anchor();
    return std::tuple{d.input_.menu_open(),
                      d.input_.menu_page(),
                      d.input_.menu_index(),
                      d.input_.menu_device(),
                      p.x,
                      p.y,
                      p.z};
  }
  static size_t queued(const Document &d) {
    std::lock_guard l(d.file_mutex_);
    return d.files_.size();
  }
};
} // namespace dome
int main(int argc, char **argv) try {
  check(argc == 2, "Choose caller admission output directory");
  std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out);
  size_t measured = 0;
  {
    auto d = setup();
    Release release;
    gate.arm();
    auto prior = d->save_async(out / "measure-before.dsculpt");
    gate.wait();
    auto options = candidate(*d);
    calls = 0;
    boundary = SIZE_MAX;
    hit = false;
    tracking = true;
    d->bind_skin(options);
    tracking = false;
    measured = calls;
    check(measured > 10 && measured < 100000 &&
              DocumentTransactionTestAccess::queued(*d) == 1 &&
              d->frame().state->asset_busy,
          "Measured direct caller has one actual accepted job");
    d->cancel_background_work();
    gate.release();
    prior.get();
    check(d->wait_asset(), "Measured job cancellation drains");
  }
  std::cout << "MEASURE actual_caller_allocations=" << measured << std::endl;
  auto d = setup();
  Release release;
  const auto before = d->copy_scene().encode();
  const auto file = out / "prior.dsculpt";
  d->copy_scene().save(file);
  gate.arm();
  auto prior = d->save_async(out / "preceding.dsculpt");
  gate.wait();
  auto options = candidate(*d);
  auto frame = d->frame();
  auto tokens = DocumentTransactionTestAccess::tokens(*d);
  auto leases = DocumentTransactionTestAccess::leases(*d);
  auto controls = DocumentTransactionTestAccess::controls(*d);
  auto input = DocumentTransactionTestAccess::input(*d);
  for (size_t at = 1; at <= measured; ++at) {
    calls = 0;
    boundary = at;
    hit = false;
    bool failed = false;
    tracking = true;
    try {
      d->bind_skin(options);
    } catch (const std::bad_alloc &) {
      failed = true;
    }
    tracking = false;
    if (!hit || !failed || d->frame().state != frame.state ||
        d->frame().surface != frame.surface || d->frame().state->asset_busy ||
        DocumentTransactionTestAccess::queued(*d) != 0) {
      std::cerr << "FAULT boundary=" << at << " hit=" << hit
                << " failed=" << failed
                << " busy=" << d->frame().state->asset_busy
                << " queued=" << DocumentTransactionTestAccess::queued(*d)
                << std::endl;
      throw std::runtime_error("Caller failure published phantom accepted work "
                               "or changed ready frame");
    }
    check(DocumentTransactionTestAccess::tokens(*d) == tokens &&
              DocumentTransactionTestAccess::leases(*d) == leases &&
              DocumentTransactionTestAccess::controls(*d) == controls &&
              DocumentTransactionTestAccess::input(*d) == input,
          "Caller failure preserves exact configuration/history/leases/input");
    check(d->copy_scene().encode() == before,
          "Caller failure preserves complete Scene/source data");
    if ((at & 127) == 0)
      std::cout << "FAULT_PASS " << at << '/' << measured << std::endl;
  }
  check(Scene::load(file).encode() == before,
        "All caller failures preserve prior file");
  d->bind_skin(options);
  auto save = d->save_async(file);
  check(DocumentTransactionTestAccess::queued(*d) == 2,
        "Successful retry and dependent Save queue behind preceding fsync");
  gate.release();
  prior.get();
  save.get();
  check(d->wait_asset() && d->frame().state->error.empty() &&
            d->copy_scene().encode() == before &&
            Scene::load(file).encode() == before,
        "Accepted all-locked retry and exact dependent Save finish");
  auto expected = options;
  expected.locks.clear();
  check(settings(d->frame().state->skin_options) == settings(expected) &&
            d->frame().state->skin_use_locks &&
            d->frame().state->weights.locks == options.locks &&
            !d->frame().state->can_undo,
        "Successful retry atomically adopts exact typed options/locks without "
        "no-op history");
  std::cout << "PASS actual caller persistent failure prefixes1.." << measured
            << " exact Scene/history/options/locks/leases/input/frame/queue, "
               "no phantom Save, retry and accepted endpoint\n";
  return 0;
} catch (const std::exception &e) {
  tracking = false;
  gate.release();
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
