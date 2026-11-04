// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/document.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
thread_local bool tracking = false;
std::atomic_size_t calls{0}, boundary{SIZE_MAX}, gate_at{1};
std::atomic_bool hit{false};
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
    if (!changed.wait_for(l, std::chrono::seconds(120),
                          [&] { return entered; }))
      throw std::runtime_error(
          "Real worker helper did not reach allocation gate");
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
void check(bool v, const char *why) {
  if (!v)
    throw std::runtime_error(why);
}
Scene fixture() {
  Scene s;
  append_humanoid(s, HumanoidControls{}, "Worker source", {}, 64);
  auto p =
      std::find_if(s.objects().begin(), s.objects().end(), [](const auto &o) {
        return o.name.find("Body_00_SKIN #3") != std::string::npos;
      });
  check(p != s.objects().end(), "Actual Body primitive exists");
  s.select(p->id);
  return s;
}
std::unique_ptr<Document> setup() {
  static const auto original = fixture();
  auto d = std::make_unique<Document>();
  d->replace(original.snapshot());
  auto o = d->copy_scene().active();
  d->object_properties(o.id, "Original source A", o.visible, o.locked,
                       o.transform, o.material);
  d->object_properties(o.id, "Original source B", o.visible, o.locked,
                       o.transform, o.material);
  check(d->undo() && d->wait_mesh(std::chrono::seconds(120)),
        "Real source with prior undo/redo is ready");
  return d;
}
SkinAuthoringOptions options(Document &d) {
  auto o = d.frame().state->skin_options;
  o.mode = SkinAuthoringMode::SourceVisible;
  o.reference = d.frame().state->active_object;
  return o;
}
} // namespace
[[gnu::noinline]] void *operator new(size_t n) {
  if (tracking) {
    auto at = ++calls;
    if (at == gate_at.load())
      gate.stop();
    if (at >= boundary.load()) {
      hit = true;
      throw std::bad_alloc();
    }
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
extern "C" bool
source_real(Scene &, std::span<const uint64_t>, const SkinAuthoringOptions &,
            const std::function<bool()> &,
            SourceVisibleSkinReport
                *) asm("__real__ZN4dome26author_source_visible_skinERNS_"
                       "5SceneESt4spanIKmLm18446744073709551615EERKNS_"
                       "20SkinAuthoringOptionsERKSt8functionIFbvEEPNS_"
                       "23SourceVisibleSkinReportE");
extern "C" bool
source_wrap(Scene &, std::span<const uint64_t>, const SkinAuthoringOptions &,
            const std::function<bool()> &,
            SourceVisibleSkinReport
                *) asm("__wrap__ZN4dome26author_source_visible_skinERNS_"
                       "5SceneESt4spanIKmLm18446744073709551615EERKNS_"
                       "20SkinAuthoringOptionsERKSt8functionIFbvEEPNS_"
                       "23SourceVisibleSkinReportE");
extern "C" bool source_wrap(Scene &s, std::span<const uint64_t> ids,
                            const SkinAuthoringOptions &o,
                            const std::function<bool()> &cancel,
                            SourceVisibleSkinReport *r) {
  tracking = true;
  auto changed = source_real(s, ids, o, cancel, r);
  tracking = false;
  return changed;
}
extern "C" void error_real(Document *, void *, std::exception_ptr) asm(
    "__real__ZN4dome8Document24file_worker_error_lockedERNS0_7FileJobENSt15__"
    "exception_ptr13exception_ptrE");
extern "C" void error_wrap(Document *, void *, std::exception_ptr) asm(
    "__wrap__ZN4dome8Document24file_worker_error_lockedERNS0_7FileJobENSt15__"
    "exception_ptr13exception_ptrE");
extern "C" void error_wrap(Document *d, void *j, std::exception_ptr e) {
  error_real(d, j, std::move(e));
  tracking = false;
}
namespace dome {
struct DocumentTransactionTestAccess {
  static auto tokens(const Document &d) {
    std::lock_guard l(d.model_mutex_);
    return std::tuple{
        d.content_,          d.next_content_,         d.saved_content_,
        d.undo_.size(),      d.redo_.size(),          d.state_.revision,
        d.state_.generation, d.state_.edit_authority, d.state_.held_object};
  }
  static auto work(const Document &d) { return d.asset_work_.load(); }
};
} // namespace dome
int main(int argc, char **argv) try {
  check(argc == 3, "Choose output and measure/cancel/stale/prep:N");
  std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out);
  const std::string mode = argv[2];
  auto d = setup();
  Release release;
  auto before = d->copy_scene().encode();
  auto path = out / "prior.dsculpt";
  d->copy_scene().save(path);
  auto tokens = DocumentTransactionTestAccess::tokens(*d);
  auto mesh = d->frame().surface;
  calls = 0;
  boundary = SIZE_MAX;
  gate_at = 1;
  hit = false;
  if (mode.starts_with("prep:"))
    boundary = std::stoull(mode.substr(5));
  if (mode.starts_with("cancel:"))
    gate_at = std::stoull(mode.substr(7));
  if (mode.starts_with("stale:"))
    gate_at = std::stoull(mode.substr(6));
  gate.arm();
  d->bind_skin(options(*d));
  gate.wait();
  auto work = DocumentTransactionTestAccess::work(*d);
  auto saving = d->save_async(path);
  nlohmann::json replacement;
  if (mode.starts_with("cancel:"))
    d->cancel_background_work();
  if (mode.starts_with("stale:")) {
    Scene newer;
    newer.active().name = "New generation remains authoritative";
    replacement = newer.encode();
    d->replace(std::move(newer));
  }
  gate.release();
  bool failed = false;
  try {
    saving.get();
  } catch (const std::exception &) {
    failed = true;
  }
  check(d->wait_asset(std::chrono::seconds(180)), "Actual worker drains");
  const auto count = calls.load();
  if (mode == "measure") {
    check(!failed && d->frame().state->error.empty() &&
              d->copy_scene().encode() != before,
          "Real full-group preparation publishes changed target");
    check(Scene::load(path).encode() == d->copy_scene().encode(),
          "Real prepared endpoint Save exact");
  } else if (mode.starts_with("stale:")) {
    check(failed && d->copy_scene().encode() == replacement &&
              Scene::load(path).encode() == before,
          "Stale full preparation cannot overwrite new generation or old file");
  } else {
    check(failed && d->copy_scene().encode() == before &&
              Scene::load(path).encode() == before &&
              DocumentTransactionTestAccess::tokens(*d) == tokens &&
              d->frame().surface == mesh,
          "Preparation failure/cancel preserves Scene/history/mesh/old file");
    check(work->done && work->failure && !work->completed,
          "Accepted failure endpoint is exact");
    if (mode.starts_with("prep:"))
      check(hit, "Actual selected preparation allocation denied");
  }
  std::cout << "PASS mode=" << mode << " allocations=" << count
            << " hit=" << hit << " source_exact=1 endpoint=1\n";
  return 0;
} catch (const std::exception &e) {
  tracking = false;
  gate.release();
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
