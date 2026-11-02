// SPDX-License-Identifier: GPL-3.0-or-later
#define main post_uv_api_fixture_suite
#include "prepared-keys-after-uv.cpp"
#undef main
#include <cstdlib>
#include <new>

namespace allocation {
thread_local bool armed = false, hit = false;
thread_local size_t calls = 0, failure = 0;
}
void *operator new(size_t n) {
  if (allocation::armed && ++allocation::calls >= allocation::failure && allocation::failure) {
    allocation::hit = true;
    throw std::bad_alloc();
  }
  if (auto value = std::malloc(n ? n : 1)) return value;
  throw std::bad_alloc();
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }

int main() try {
  const auto source = prepared(false);
  const auto before = source.encode();
  ShapeKeyEdit edit;
  edit.action = ShapeKeyAction::Replace;
  edit.positions = source.active().painted->mesh().positions;
  edit.positions[0].z += .08f;
  auto probe = source;
  allocation::armed = true;
  author_shapekey(probe, source.active().id, edit);
  allocation::armed = false;
  const auto measured = allocation::calls;
  const auto accepted = probe.encode();
  if (measured < 100) throw std::runtime_error("Post-UV authoring allocation probe is vacuous");
  std::cout << "MEASURED " << measured << " ordinary-new boundaries\n" << std::flush;
  size_t refused = 0, completed = 0, reached = 0;
  for (size_t boundary = 1; boundary <= measured; ++boundary) {
    auto trial = source;
    allocation::calls = 0;
    allocation::failure = boundary;
    allocation::hit = false;
    bool failed = false;
    allocation::armed = true;
    try { author_shapekey(trial, source.active().id, edit); }
    catch (const std::bad_alloc &) { failed = true; }
    allocation::armed = false;
    if (!allocation::hit) continue;
    ++reached;
    if (failed) {
      if (trial.encode() != before)
        throw std::runtime_error("Persistent denial published a partial post-UV key or source receipt");
      ++refused;
    } else {
      trial.validate();
      if (trial.encode() != accepted)
        throw std::runtime_error("Best-effort cache denial changed accepted post-UV authority");
      ++completed;
    }
    if (source.encode() != before)
      throw std::runtime_error("Persistent denial mutated the retained complete original UV graph");
  }
  auto retry = Scene::decode(before);
  author_shapekey(retry, source.active().id, edit);
  if (retry.encode() != accepted)
    throw std::runtime_error("Post-UV key cannot retry exactly after persistent denial");
  std::cout << "PASS " << refused << " persistent refusals, " << completed
            << " complete cache outcomes, " << reached << '/' << measured
            << " reached/measured boundaries; exact source and retry\n";
  return 0;
} catch (const std::exception &error) {
  allocation::armed = false;
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
