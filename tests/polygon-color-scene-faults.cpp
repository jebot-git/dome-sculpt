// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_polygon_color_scene_suite
#include "polygon-color-scene.cpp"
#undef main
#include <cstdlib>
#include <new>
namespace {
thread_local bool deny = false, fault_hit = false;
thread_local size_t allocation = 0, fail_at = SIZE_MAX;
void allocation_check() {
  if (deny && ++allocation >= fail_at) {
    fault_hit = true;
    throw std::bad_alloc();
  }
}
void require(bool b, const char *message) {
  if (!b)
    throw std::runtime_error(message);
}
} // namespace
[[gnu::noinline]] void *operator new(size_t n) {
  allocation_check();
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
  auto scene = fixture();
  const auto baseline = scene.encode();
  const auto authority = scene.active().polygon;
  const auto basis = authority->colors->source_owner();
  const auto paint_owner = scene.active().painted->mesh_owner();
  const auto source_model = authority->origin->appearance->asset;
  size_t total = 0;
  for (bool decoding : {false, true}) {
    allocation = 0;
    fail_at = SIZE_MAX;
    deny = true;
    if (decoding) {
      auto result = Scene::decode(baseline);
    } else {
      auto result = scene.encode();
    }
    deny = false;
    const auto measured = allocation;
    require(measured > 100,
            "Actual Scene14 path exposes meaningful allocation boundaries");
    for (size_t at = 1; at <= measured; ++at) {
      allocation = 0;
      fail_at = at;
      fault_hit = false;
      deny = true;
      bool caught = false;
      try {
        if (decoding) {
          auto result = Scene::decode(baseline);
        } else {
          auto result = scene.encode();
        }
      } catch (const std::bad_alloc &) {
        caught = true;
      }
      deny = false;
      require(fault_hit && caught, "Every observed Scene14 allocation failure "
                                   "must actually be reached and unwind");
      require(scene.active().polygon == authority &&
                  authority->colors->source_owner() == basis &&
                  scene.active().painted->mesh_owner() == paint_owner &&
                  authority->origin->appearance->asset == source_model,
              "Allocation refusal cannot replace any authoritative "
              "current/source/paint owner");
      require(scene.encode() == baseline,
              "Every actual refusal leaves exact immutable original and "
              "current source data");
      require(Scene::decode(baseline).encode() == baseline,
              "Successful Scene14 retry remains deterministic after persistent "
              "denial");
    }
    total += measured;
    std::cout << measured << (decoding ? " decode" : " encode")
              << " actual persistent allocation boundaries preserve exact "
                 "Scene14 colors, paint and original source\n";
  }
  std::cout << total
            << " Scene14 encode/decode persistent-denial boundaries passed\n";
  return 0;
} catch (const std::exception &e) {
  deny = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
