// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-source-fixture.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
using namespace dome;
namespace {
thread_local bool enabled = false, hit = false;
thread_local size_t calls = 0, fail = 0;
template <class T> bool same(const std::vector<T> &a, const std::vector<T> &b) {
  return a.size() == b.size() &&
         (a.empty() ||
          std::memcmp(a.data(), b.data(), a.size() * sizeof(T)) == 0);
}
bool same(const Mesh &a, const Mesh &b) {
  return same(a.positions, b.positions) && same(a.normals, b.normals) &&
         same(a.uv, b.uv) && same(a.indices, b.indices) &&
         same(a.uv_pins, b.uv_pins) && same(a.uv_seams, b.uv_seams);
}
} // namespace
void *operator new(size_t n) {
  if (enabled && ++calls == fail) {
    hit = true;
    throw std::bad_alloc();
  }
  if (auto p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
int main() try {
  polygon_test::Fixture fixture;
  auto old = fixture.rest, old_primitive = fixture.asset->primitives[0].mesh;
  auto old_vertices = fixture.appearance.source_vertices;
  auto old_bytes = fixture.asset->primitives[0].extras_json;
  calls = fail = 0;
  enabled = true;
  auto prepared = prepare_polygon_topology(fixture.rest, &fixture.appearance);
  enabled = false;
  const auto count = calls;
  auto expected = prepared.topology.encode();
  auto expected_vertices = prepared.source_vertices;
  if (count < 100)
    throw std::runtime_error(
        "Original source preparation allocations were not measured");
  for (size_t boundary = 1; boundary <= count; ++boundary) {
    calls = 0;
    fail = boundary;
    hit = false;
    bool rejected = false;
    enabled = true;
    try {
      prepare_polygon_topology(fixture.rest, &fixture.appearance);
    } catch (const std::bad_alloc &) {
      rejected = true;
    }
    enabled = false;
    if (!hit || !rejected || !same(fixture.rest, old) ||
        !same(fixture.asset->primitives[0].mesh, old_primitive) ||
        fixture.appearance.source_vertices != old_vertices ||
        fixture.asset->primitives[0].extras_json != old_bytes)
      throw std::runtime_error(
          "A real source preparation allocation failure changed the original "
          "mesh/UV/metadata authority");
    auto retry = prepare_polygon_topology(fixture.rest, &fixture.appearance);
    if (retry.topology.encode() != expected ||
        retry.source_vertices != expected_vertices || !retry.authored_ngons)
      throw std::runtime_error(
          "Rejected source preparation did not retry deterministically");
  }
  std::cout << "PASS " << count
            << " actual polygon-source allocation boundaries preserve exact "
               "rest attributes/source witness and deterministic retry\n";
  return 0;
} catch (const std::exception &e) {
  enabled = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
