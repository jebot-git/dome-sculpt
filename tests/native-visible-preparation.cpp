// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/skin-authoring.hpp"
#define main native_reference_main
#include "geometry-weight-reference.cpp"
#undef main
#include <cstdlib>
#include <new>
namespace {
thread_local bool tracking = false;
size_t calls = 0, boundary = SIZE_MAX;
bool hit = false;
void require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
auto values(const NativeVisibleSkinReport &r) {
  return std::tuple{r.work,
                    r.planned_workspace,
                    r.parts,
                    r.changed_parts,
                    r.rows,
                    r.permitted_controls,
                    r.supported_controls,
                    r.measure_zero_origins};
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
int main() try {
  const auto input = fixture();
  const auto encoded = input.encode();
  SkinAuthoringOptions options;
  options.mode = SkinAuthoringMode::NativeVisible;
  options.native_visible.contact_handling =
      NativeContactHandling::FiniteLineIntegral;
  options.reference = 1;
  auto measured = input.snapshot();
  calls = 0;
  tracking = true;
  bool changed =
      author_native_visible_skin(measured, std::array<uint64_t, 1>{2}, options);
  tracking = false;
  const size_t count = calls;
  require(changed && count > 100 && count < 100000,
          "Actual small native helper measured complete preparation");
  const auto expected = measured.encode();
  std::cout << "MEASURE actual_native_helper_allocations=" << count
            << std::endl;
  for (size_t at = 1; at <= count; ++at) {
    auto scene = input.snapshot();
    const auto paint = scene.object(2).painted,
               source_paint = scene.object(1).painted;
    const auto deformation = scene.object(2).deformation,
               source_deformation = scene.object(1).deformation;
    NativeVisibleSkinReport report{101, 102, 103, 104, 105, 106, 107};
    const auto old_report = values(report);
    calls = 0;
    boundary = at;
    hit = false;
    bool failed = false;
    tracking = true;
    try {
      author_native_visible_skin(scene, std::array<uint64_t, 1>{2}, options, {},
                                 &report);
    } catch (const std::bad_alloc &) {
      failed = true;
    }
    tracking = false;
    if (!hit || !failed || scene.encode() != encoded ||
        values(report) != old_report || scene.object(2).painted != paint ||
        scene.object(1).painted != source_paint ||
        scene.object(2).deformation != deformation ||
        scene.object(1).deformation != source_deformation) {
      std::cerr << "boundary=" << at << " hit=" << hit << " failed=" << failed
                << '\n';
      throw std::runtime_error(
          "Real native preparation failure changed Scene/owners/report");
    }
    if ((at & 255) == 0)
      std::cout << "PREFIX_PASS " << at << '/' << count << std::endl;
  }
  boundary = SIZE_MAX;
  for (size_t stop : {size_t(1), size_t(16), size_t(64)}) {
    auto scene = input.snapshot();
    size_t checks = 0;
    bool failed = false;
    NativeVisibleSkinReport report{101, 102, 103, 104, 105, 106, 107};
    const auto old = values(report);
    try {
      author_native_visible_skin(
          scene, std::array<uint64_t, 1>{2}, options,
          [&] { return ++checks >= stop; }, &report);
    } catch (const std::runtime_error &) {
      failed = true;
    }
    require(failed && checks >= stop && scene.encode() == encoded &&
                values(report) == old,
            "Real preparation cancellation preserves complete source/target "
            "and report");
  }
  auto retry = input.snapshot();
  require(
      author_native_visible_skin(retry, std::array<uint64_t, 1>{2}, options) &&
          retry.encode() == expected,
      "All real failure prefixes retain exact successful retry");
  auto budget = input.snapshot();
  auto low = options;
  low.native_visible.maximum_work = 1;
  bool refused = false;
  try {
    author_native_visible_skin(budget, std::array<uint64_t, 1>{2}, low);
  } catch (const std::length_error &) {
    refused = true;
  }
  require(refused && budget.encode() == encoded,
          "Native caller planning refuses insufficient work before adoption");
  std::cout << "PASS all " << count
            << " real small-helper persistent allocation prefixes +3 measured "
               "cancellation edges +retry +work admission; no large/avatar "
               "exhaustive claim\n";
  return 0;
} catch (const std::exception &e) {
  tracking = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
