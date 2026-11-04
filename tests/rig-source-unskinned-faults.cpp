// SPDX-License-Identifier: GPL-3.0-or-later
#define main rig_source_fixture_main
#include "rig-source-unskinned.cpp"
#undef main
namespace {
thread_local bool allocation_tracking = false;
thread_local size_t allocation_calls = 0, allocation_boundary = SIZE_MAX;
thread_local bool allocation_hit = false;
} // namespace
[[gnu::noinline]] void *operator new(size_t n) {
  if (allocation_tracking && ++allocation_calls >= allocation_boundary) {
    allocation_hit = true;
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
int main() try {
  size_t tested = 0;
  for (bool morph : {false, true}) {
    const auto original = fixture(morph);
    const auto expected = original.encode();
    const auto current = changed_current(bind(original));
    const auto before = current.encode();
    auto measure = current.snapshot();
    allocation_calls = 0;
    allocation_boundary = SIZE_MAX;
    allocation_tracking = true;
    restore_rig_source(measure, 2, false);
    allocation_tracking = false;
    const auto count = allocation_calls;
    check(count > 0 && measure.encode() == expected,
          "Measure actual complete unskinned-source restoration allocations");
    for (size_t at = 1; at <= count; ++at) {
      auto candidate = current.snapshot();
      allocation_calls = 0;
      allocation_boundary = at;
      allocation_hit = false;
      allocation_tracking = true;
      bool caught = false;
      try {
        restore_rig_source(candidate, 2, false);
      } catch (const std::bad_alloc &) {
        caught = true;
      }
      allocation_tracking = false;
      if (!caught || !allocation_hit || candidate.encode() != before)
        throw std::runtime_error(
            "Actual persistent restoration failure lost exact source at=" +
            std::to_string(at));
      restore_rig_source(candidate, 2, false);
      if (candidate.encode() != expected)
        throw std::runtime_error(
            "Actual source restoration retry changed data");
      ++tested;
    }
    std::cout << "FAULT_PASS morph=" << morph << " prefixes=" << count
              << std::endl;
  }
  std::cout
      << "PASS actual_unskinned_restore_persistent_prefixes=" << tested
      << " exact_scene=1 retry=1 allocation_scope=ordinary_new_newarray\n";
  return 0;
} catch (const std::exception &e) {
  allocation_tracking = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
