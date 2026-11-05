// SPDX-License-Identifier: GPL-3.0-or-later
#define main prior_affine_main
#include "retopo-affine.cpp"
#undef main
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <new>
#include <unistd.h>
namespace worker_fault {
thread_local bool worker = false;
std::atomic<bool> enabled{false}, hit{false};
std::atomic<bool> persistent{false};
std::atomic<size_t> calls{0}, boundary{0};
void visit() {
  if (worker && enabled.load()) {
    const auto n = calls.fetch_add(1) + 1;
    if (boundary.load() && n >= boundary.load()) {
      // One-shot failure leaves error reporting available. Persistent failure
      // is a separate scope; no allocator fallback substitutes for this fault.
      if (!persistent.load()) enabled.store(false);
      hit.store(true); throw std::bad_alloc();
    }
  }
}
void *allocate(size_t n, size_t alignment = 0) {
  visit(); void *p = nullptr;
  if (alignment) { if (posix_memalign(&p, alignment, n ? n : 1)) p = nullptr; }
  else p = std::malloc(n ? n : 1);
  if (!p) throw std::bad_alloc(); return p;
}
void arm(size_t at) { calls = 0; boundary = at; hit = false; enabled = true; }
}
void *operator new(size_t n) { return worker_fault::allocate(n); }
void *operator new[](size_t n) { return worker_fault::allocate(n); }
void *operator new(size_t n, std::align_val_t a) { return worker_fault::allocate(n, size_t(a)); }
void *operator new[](size_t n, std::align_val_t a) { return worker_fault::allocate(n, size_t(a)); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
void operator delete(void *p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void *p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void *p, size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t, std::align_val_t) noexcept { std::free(p); }
extern "C" void real_worker(Document *) asm("__real__ZN4dome8Document11retopo_loopEv");
extern "C" void wrapped_worker(Document *) asm("__wrap__ZN4dome8Document11retopo_loopEv");
extern "C" void wrapped_worker(Document *d) {
  worker_fault::worker = true; real_worker(d); worker_fault::worker = false;
}
namespace queued_fault {
std::atomic<bool> armed{false}, entered{false}, released{false}, publication{false}, hit{false};
std::atomic<bool> acceptance{false};
struct Release { ~Release() { released = true; } };
}
extern "C" void real_edit(RetopoWorkspace &, RetopoAction, Vec3, float, const std::function<bool()> &)
  asm("__real__ZN4dome15RetopoWorkspace4editENS_12RetopoActionENS_4Vec3EfRKSt8functionIFbvEE");
extern "C" void wrapped_edit(RetopoWorkspace &, RetopoAction, Vec3, float, const std::function<bool()> &)
  asm("__wrap__ZN4dome15RetopoWorkspace4editENS_12RetopoActionENS_4Vec3EfRKSt8functionIFbvEE");
extern "C" void wrapped_edit(RetopoWorkspace &w, RetopoAction a, Vec3 p, float mm, const std::function<bool()> &cancelled) {
  const bool gate = a == RetopoAction::Face && queued_fault::armed.exchange(false);
  if (gate) {
    queued_fault::entered = true;
    while (!queued_fault::released.load()) std::this_thread::yield();
  }
  real_edit(w, a, p, mm, cancelled);
  if (gate) queued_fault::publication = true;
  if (worker_fault::worker && a == RetopoAction::EndGesture && queued_fault::acceptance.exchange(false))
    queued_fault::publication = true;
}
extern "C" void real_publish(Document *, bool, bool) asm("__real__ZN4dome8Document14publish_lockedEbb");
extern "C" void wrapped_publish(Document *, bool, bool) asm("__wrap__ZN4dome8Document14publish_lockedEbb");
extern "C" void wrapped_publish(Document *d, bool force, bool checkpoint) {
  if (worker_fault::worker && queued_fault::publication.exchange(false)) {
    queued_fault::hit = true;
    throw std::bad_alloc(); // Real worker edit has finished; publication has not.
  }
  real_publish(d, force, checkpoint);
}
namespace {
constexpr Matrix matrix{-1.3,.1,0,0,.35,.8,.05,0,.1,.15,1.2,0,.15,-.1,.2,1};
void points(Document &d, const Scene &s) {
  d.replace(s.snapshot()); d.begin_retopology(); ready(d);
  for (auto p : std::array<Vec3,3>{{{-.2f,-.15f,-.055f},{.2f,-.15f,.025f},{0,.15f,.015f}}})
    place(d, point(matrix, evaluated(p, true)));
}
void held_point(Document &d) {
  check(d.begin_desktop(), "Real desktop holds a further draft point");
  Brush b; b.radius = .01f;
  b.from = b.to = point(matrix, evaluated({.22f,.17f,.061f}, true));
  d.stamp_desktop(b); ready(d);
}
void acceptance_cases(const std::filesystem::path &out, const Scene &scene) {
  std::filesystem::create_directories(out);
  const auto original = scene.encode();
  for (bool next_gesture : {false, true}) {
    Document d; points(d, scene); atlas_gate::Release release;
    d.edit_retopology(RetopoAction::Face); ready(d);
    const auto before = d.frame().state->retopology->topology->encode();
    held_point(d);
    const auto held = d.frame().state->retopology->topology->encode();
    check(held != before, "Held acceptance fixture has a real additional point");
    const auto path = out / (next_gesture ? "next-gesture.dsculpt" : "local-undo.dsculpt");
    { std::ofstream prior(path, std::ios::binary); prior << "prior exact file"; }
    atlas_gate::arm(); // The old ready-before-publication path cannot finish the file first.
    queued_fault::hit = false; queued_fault::acceptance = true;
    d.end_retopology(true); auto saved = d.save_async(path);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!queued_fault::hit.load() && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(queued_fault::hit, "Finalized held Apply reaches actual worker publication failure");
    atlas_gate::release();
    bool failed = false;
    try { saved.get(); } catch (const std::bad_alloc &) { failed = true; }
    check(failed, "Dependent Save fails when its accepted worker publication fails");
    check(d.wait_asset() && d.copy_scene().encode() == original,
          "Failed accepted publication preserves the original complete scene");
    std::ifstream prior(path, std::ios::binary);
    const std::string bytes{std::istreambuf_iterator<char>(prior), {}};
    check(bytes == "prior exact file", "Failed accepted publication preserves existing file bytes");
    check(d.frame().state->retopology && d.frame().state->retopology->topology->encode() == held,
          "Failed Apply retains the exact pre-Apply held draft");
    if (next_gesture) {
      held_point(d); d.cancel_desktop(); ready(d);
      check(d.frame().state->retopology->topology->encode() == held,
            "Next gesture cancellation retains the previously held draft");
    }
    check(d.undo(), "Retained draft remains locally undoable after failed Apply"); ready(d);
    check(d.frame().state->retopology->topology->encode() == before,
          "Local Undo removes the held point exactly after failed Apply");
    if (next_gesture) {
      check(d.redo(), "Finalized preceding gesture remains redoable"); ready(d);
      check(d.frame().state->retopology->topology->encode() == held,
            "Redo restores the exact retained draft");
    }
    d.end_retopology(true); auto retry = d.save_async(path); retry.get();
    check(d.wait_asset() && Scene::load(path).encode() == d.copy_scene().encode(),
          "Failed accepted worker publication retries through an exact ordered Save");
  }
  {
    Document d; points(d, scene); atlas_gate::Release release;
    d.edit_retopology(RetopoAction::Face); ready(d);
    const auto before = d.frame().state->retopology->topology->encode();
    held_point(d);
    const auto held = d.frame().state->retopology->topology->encode();
    atlas_gate::arm();
    d.end_retopology(true); auto saved = d.save_async(out / "file-cancel.dsculpt");
    check(atlas_gate::wait(), "Accepted held Apply reaches the actual detached file computation");
    d.cancel_background_work(); atlas_gate::release();
    bool cancelled = false;
    try { saved.get(); } catch (const std::exception &e) {
      cancelled = std::string_view(e.what()).find("cancel") != std::string_view::npos;
    }
    check(cancelled && d.wait_asset() && d.copy_scene().encode() == original,
          "Cancelling file publication fails dependent Save and preserves original source");
    check(d.frame().state->retopology && d.frame().state->retopology->topology->encode() == held,
          "File cancellation retains the exact successfully finalized draft");
    check(d.undo(), "Finalized held gesture remains undoable after file cancellation"); ready(d);
    check(d.frame().state->retopology->topology->encode() == before,
          "Undo after file cancellation removes only the finalized held point");
    check(d.redo(), "Finalized held gesture remains redoable after file cancellation"); ready(d);
    check(d.frame().state->retopology->topology->encode() == held,
          "Redo after file cancellation restores the exact finalized draft");
    d.end_retopology(true); auto retry = d.save_async(out / "file-cancel-retry.dsculpt"); retry.get();
    check(d.wait_asset() && Scene::load(out / "file-cancel-retry.dsculpt").encode() == d.copy_scene().encode(),
          "Cancelled accepted Apply retries through an exact ordered Save");
  }
}
}
int main(int argc, char **argv) try {
  std::set_terminate([] {
    constexpr char message[] = "UNHANDLED_WORKER_EXCEPTION\n";
    (void)::write(STDERR_FILENO, message, sizeof(message) - 1);
    std::_Exit(88); // No test core dump or active-session changes.
  });
  const bool tail = argc == 2 && std::string_view(argv[1]) == "--tail";
  const bool persistent = argc == 2 && std::string_view(argv[1]) == "--persistent";
  const bool acceptance = argc == 3 && std::string_view(argv[1]) == "--acceptance";
  const auto scene = source(matrix, true); const auto original = scene.encode();
  std::string endpoint; size_t count = 0;
  for (int repeat = 0; repeat < 3; ++repeat) {
    Document d; points(d, scene); worker_fault::arm(0);
    d.edit_retopology(RetopoAction::Face);
    const bool drained = d.wait_retopology(); worker_fault::enabled = false;
    check(drained && d.frame().state->error.empty(), "Uninjected real worker operation completes");
    if (repeat) check(count == worker_fault::calls.load(), "Worker allocation count is stable");
    count = worker_fault::calls;
    const auto result = d.frame().state->retopology->topology->encode();
    if (repeat) check(endpoint == result, "Uninjected worker topology is deterministic");
    endpoint = result;
  }
  check(count > 0, "Whole real worker allocation boundaries measured");
  std::cout << "WORKER_BASELINE " << count << std::endl;
  if (acceptance) {
    acceptance_cases(argv[2], scene);
    std::cout << "PASS " << checks << " accepted publication checks\n";
    return 0;
  }
  if (persistent) {
    Document d; points(d, scene);
    const auto before = d.frame().state->retopology->topology->encode();
    worker_fault::persistent = true; worker_fault::arm(1);
    d.edit_retopology(RetopoAction::Face);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (worker_fault::calls.load() < 2 && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    worker_fault::enabled = false;
    check(worker_fault::calls.load() >= 2 && worker_fault::hit,
          "Both real edit preparation and its error reporter are denied");
    // Only release the fault after its reporter allocation has also failed.
    // A fresh healthy no-op must recover the same live worker and old draft.
    d.edit_retopology(RetopoAction::PlaceMode); ready(d);
    check(d.frame().state->retopology->topology->encode() == before &&
              d.copy_scene().encode() == original,
          "Reporter exhaustion retains exact draft/source and the worker recovers");
    d.edit_retopology(RetopoAction::Face); ready(d);
    check(d.frame().state->retopology->topology->encode() == endpoint,
          "The surviving worker retries to the exact final topology");
    std::cout << "PASS " << checks << " persistent reporter recovery checks\n";
    return 0;
  }
  size_t refused = 0, optional = 0;
  const size_t first = tail && count > 32 ? count - 31 : 1;
  for (size_t at = first; at <= count; ++at) {
    Document d; points(d, scene);
    const auto view = d.frame().state->retopology;
    const auto before = view->topology->encode();
    worker_fault::arm(at); d.edit_retopology(RetopoAction::Face);
    const bool drained = d.wait_retopology(); worker_fault::enabled = false;
    check(drained && worker_fault::hit, "The selected real worker allocation actually fails");
    const auto actual = d.frame().state->retopology->topology->encode();
    if (!d.frame().state->error.empty()) {
      if (actual != before)
        throw std::runtime_error("Failed worker publication committed hidden topology at prefix " + std::to_string(at));
      ++refused;
      check(d.undo(), "Failed worker retains its prior draft history"); ready(d);
      check(d.frame().state->retopology->topology->vertices().size() == 2,
            "Undo removes the prior point, not a hidden failed face");
      check(d.redo(), "Prior history can redo after failed worker work"); ready(d);
      check(d.frame().state->retopology->topology->encode() == before,
            "Redo restores the exact pre-operation draft");
      d.edit_retopology(RetopoAction::Face); ready(d);
      check(d.frame().state->retopology->topology->encode() == endpoint,
            "The real worker retries to the complete exact endpoint");
    } else {
      ++optional; check(actual == endpoint, "Optional allocation failure yields only the full accepted endpoint");
    }
    check(d.copy_scene().encode() == original, "Every worker result retains the complete original scene");
    std::cout << "PASS worker prefix " << at << std::endl;
  }
  if (!tail) {
    Document d; points(d, scene);
    queued_fault::Release release;
    const auto before = d.frame().state->retopology->topology->encode();
    queued_fault::entered = queued_fault::released = queued_fault::hit = false;
    queued_fault::armed = true; d.edit_retopology(RetopoAction::Face);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!queued_fault::entered.load() && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!queued_fault::entered.load()) queued_fault::released = true;
    check(queued_fault::entered, "Actual first edit is held before publishing");
    d.edit_retopology(RetopoAction::Clear); // A genuine second accepted input.
    queued_fault::released = true; ready(d);
    check(queued_fault::hit && d.frame().state->retopology->selected.empty(),
          "Failed first publication cannot retire or skip the next queued event");
    check(d.frame().state->retopology->topology->encode() == before && d.copy_scene().encode() == original,
          "The following queued event keeps pre-failure topology and complete source");
  }
  std::cout << "PASS " << checks << " worker checks, refused=" << refused
            << " optional=" << optional << std::endl;
  return 0;
} catch (const std::exception &e) {
  worker_fault::enabled = false;
  queued_fault::released = true;
  std::cerr << "FAIL " << e.what() << '\n'; return 1;
}
