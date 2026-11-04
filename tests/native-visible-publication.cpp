// SPDX-License-Identifier: GPL-3.0-or-later
#include "document-authority.hpp"
#define main original_reference_main
#define fixture native_reference_fixture
#include "geometry-weight-reference.cpp"
#undef fixture
#undef main
#include "dome/document.hpp"
#include "dome/rig-rest.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
std::atomic_bool enabled{false}, hit{false};
std::atomic_size_t calls{0}, boundary{SIZE_MAX};
thread_local bool tracking = false;
std::atomic<Document *> current{nullptr};
void require(bool v, const char *why) {
  if (!v)
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
    if (!changed.wait_for(l, std::chrono::seconds(120),
                          [&] { return entered; }))
      throw std::runtime_error("Actual source helper gate missing");
  }
  void release() {
    std::lock_guard l(mutex);
    armed = false;
    changed.notify_all();
  }
} gate, post_gate;
struct Release {
  ~Release() {
    enabled = false;
    tracking = false;
    gate.release();
    post_gate.release();
  }
};
Scene fixture() { return native_reference_fixture(); }
std::unique_ptr<Document> setup() {
  static const auto base = fixture();
  auto d = std::make_unique<Document>();
  d->replace(base.snapshot());
  current = d.get();
  for (const auto &name : {"Original source A", "Original source B"}) {
    auto o = d->copy_scene().active();
    d->object_properties(o.id, name, o.visible, o.locked, o.transform,
                         o.material);
  }
  require(d->undo() && d->wait_mesh(std::chrono::seconds(120)),
          "Actual prior history/source ready");
  return d;
}
SkinAuthoringOptions options(const Document &d) {
  auto o = d.frame().state->skin_options;
  o.mode = SkinAuthoringMode::NativeVisible;
  o.reference = 1;
  return o;
}
} // namespace
[[gnu::noinline]] void *operator new(size_t n) {
  if (tracking && enabled && ++calls >= boundary.load()) {
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
extern "C" bool
source_real(Scene &, std::span<const uint64_t>, const SkinAuthoringOptions &,
            const std::function<bool()> &,
            NativeVisibleSkinReport
                *) asm("__real__ZN4dome26author_native_visible_skinERNS_"
                       "5SceneESt4spanIKmLm18446744073709551615EERKNS_"
                       "20SkinAuthoringOptionsERKSt8functionIFbvEEPNS_"
                       "23NativeVisibleSkinReportE");
extern "C" bool
source_wrap(Scene &, std::span<const uint64_t>, const SkinAuthoringOptions &,
            const std::function<bool()> &,
            NativeVisibleSkinReport
                *) asm("__wrap__ZN4dome26author_native_visible_skinERNS_"
                       "5SceneESt4spanIKmLm18446744073709551615EERKNS_"
                       "20SkinAuthoringOptionsERKSt8functionIFbvEEPNS_"
                       "23NativeVisibleSkinReportE");
extern "C" bool source_wrap(Scene &s, std::span<const uint64_t> ids,
                            const SkinAuthoringOptions &o,
                            const std::function<bool()> &cancel,
                            NativeVisibleSkinReport *r) {
  tracking = false;
  gate.stop();
  auto changed = source_real(s, ids, o, cancel, r);
  post_gate.stop();
  tracking = enabled.load();
  return changed;
}
extern "C" void
commit_real(void *, bool) asm("__real__ZN4dome8Document11Transaction6commitEb");
extern "C" void
commit_wrap(void *, bool) asm("__wrap__ZN4dome8Document11Transaction6commitEb");
extern "C" void commit_wrap(void *self, bool checkpoint) {
  tracking = false;
  commit_real(self, checkpoint);
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
        d.content_,           d.next_content_,          d.saved_content_,
        d.undo_.size(),       d.redo_.size(),           d.state_.revision,
        d.state_.generation,  d.recovery_content_,      d.state_.edit_authority,
        d.state_.held_object, d.state_.selected_objects};
  }
  static auto admitted(const Document &d) {
    std::lock_guard l(d.model_mutex_);
    return std::tuple{d.state_.generation, d.state_.revision,
                      d.scene_.active().id, d.state_.edit_authority};
  }
  static auto work(const Document &d) { return d.asset_work_.load(); }
};
} // namespace dome
namespace {
std::future<void> accept(Document &d, const std::filesystem::path &path) {
  gate.arm();
  d.bind_skin(options(d));
  gate.wait();
  return d.save_async(path);
}
} // namespace
int main(int argc, char **argv) try {
  require(argc == 2 || argc == 4,
          "Choose publication output and optional inclusive first/last");
  std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out);
  auto measured = setup();
  Release release;
  const auto original = measured->copy_scene().encode();
  auto saved = accept(*measured, out / "real-prepared.dsculpt");
  calls = 0;
  boundary = SIZE_MAX;
  hit = false;
  enabled = true;
  gate.release();
  saved.get();
  enabled = false;
  require(measured->wait_asset() && measured->frame().state->error.empty(),
          "Real helper/publication/Save succeeds");
  const auto count = calls.load();
  auto expected = measured->copy_scene().encode();
  require(count > 100,
          "Measured real post-helper publication allocation count");
  auto restored = Scene::load(out / "real-prepared.dsculpt");
  restore_rig_source(restored, restored.active().id, false);
  require(restored.encode() == original,
          "Real saved changed bind restores complete original source");
  auto selected = measured->copy_scene().export_surfaces();
  std::erase_if(selected, [](const auto &s) { return s.id != 2; });
  export_glb(selected, out / "real-current.glb");
  require(std::filesystem::file_size(out / "real-current.glb") > 1000,
          "Real preparation baseline emits current standard GLB");
  measured.reset();
  std::cout << "MEASURE post_helper_allocations=" << count << std::endl;
  {
    auto stale = setup();
    Release cleanup;
    const auto path = out / "stale-prior.dsculpt";
    const auto input = stale->copy_scene().encode();
    stale->copy_scene().save(path);
    post_gate.arm();
    auto result = accept(*stale, path);
    auto work = DocumentTransactionTestAccess::work(*stale);
    gate.release();
    post_gate.wait();
    auto next = fixture();
    next.object(2).name = "Genuine newer native generation";
    const auto newer = next.encode();
    stale->replace(std::move(next));
    post_gate.release();
    bool failed = false;
    try {
      result.get();
    } catch (const std::exception &) {
      failed = true;
    }
    require(failed && stale->wait_asset() &&
                stale->copy_scene().encode() == newer &&
                Scene::load(path).encode() == input &&
                !stale->frame().state->can_undo,
            "Actual prepared native result cannot replace a newer generation "
            "or save stale/prior fallback");
    require(
        work->done && work->failure && !work->completed,
        "Actual stale accepted native Save owns the original failed endpoint");
    std::cout << "PASS actual post-helper newer-generation refusal and "
                 "dependent Save failure\n";
  }
  size_t first = 1, last = count;
  if (argc == 4) {
    first = std::stoull(argv[2]);
    last = std::min(count, size_t(std::stoull(argv[3])));
  }
  require(first && first <= last && last <= count,
          "Actual measured publication range");
  for (size_t at = first; at <= last; ++at) {
    auto d = setup();
    Release cleanup;
    auto input = d->copy_scene().encode();
    auto path = out / "prior.dsculpt";
    d->copy_scene().save(path);
    auto tokens = DocumentTransactionTestAccess::tokens(*d);
    auto mesh = d->frame().surface;
    auto result = accept(*d, path);
    auto work = DocumentTransactionTestAccess::work(*d);
    calls = 0;
    boundary = at;
    hit = false;
    enabled = true;
    gate.release();
    bool failed = false;
    try {
      result.get();
    } catch (const std::bad_alloc &) {
      failed = true;
    }
    enabled = false;
    require(d->wait_asset() && hit && failed,
            "Actual post-helper allocation denial and original Save failure");
    require(d->copy_scene().encode() == input &&
                Scene::load(path).encode() == input &&
                DocumentTransactionTestAccess::tokens(*d) == tokens &&
                d->frame().surface == mesh,
            "Failed publication preserves complete Scene/prior "
            "file/history/content/revision/generation/authority/mesh");
    require(work->done && work->failure && !work->completed,
            "Persistent-denial accepted endpoint failure is exact");
    auto retry = accept(*d, path);
    gate.release();
    retry.get();
    require(d->wait_asset() && d->frame().state->error.empty() &&
                d->copy_scene().encode() == expected,
            "Same actual native admission retries authenticated preparation");
    require(d->undo() && d->copy_scene().encode() == input && d->redo() &&
                d->copy_scene().encode() == expected,
            "Actual retry adds exactly one source-retaining Undo/Redo");
    if ((at & 63) == 0)
      std::cout << "PUBLICATION_PASS " << at << '/' << count << std::endl;
  }
  std::cout << "PASS exhaustive selected range=" << first << ".." << last
            << " total=" << count
            << " real_preparation_each_prefix=1 recipient_identity=1 save=1 "
               "history=1 "
               "restore=1\n";
  return 0;
} catch (const std::exception &e) {
  enabled = false;
  tracking = false;
  gate.release();
  post_gate.release();
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
