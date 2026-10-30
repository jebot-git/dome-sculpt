// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon-color.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
bool enabled = false, hit = false;
size_t calls = 0;
int64_t remaining = -1;
} // namespace
void *operator new(size_t n) {
  if (enabled) {
    ++calls;
    if (remaining == 0) {
      hit = true;
      throw std::bad_alloc();
    }
    if (remaining > 0)
      --remaining;
  }
  if (void *p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
int main() try {
  auto source = PolygonMesh::cube();
  std::vector<PolygonCornerRgba> samples;
  for (size_t f = 0; f < source.faces().size(); ++f)
    for (auto &c : source.faces()[f].corners)
      samples.push_back({c.id, {float(f) / 8, .25f, .75f, .5f}});
  auto field = PolygonColorField::from_corners(source, samples);
  auto original = source.encode();
  auto colors = field.packed(source);
  size_t total = 0;
  for (auto op : {PolygonOperation::Extrude, PolygonOperation::Inset,
                  PolygonOperation::CutEdges, PolygonOperation::Weld}) {
    PolygonEdit e;
    e.operation = op;
    e.amount = .1f;
    e.cuts = 2;
    if (op == PolygonOperation::Weld)
      e.selection = {PolygonSelectionMode::Vertex,
                     {source.edges()[0].a, source.edges()[0].b}};
    else if (op == PolygonOperation::CutEdges)
      e.selection = {PolygonSelectionMode::Edge, {source.edges()[0].id}};
    else
      e.selection = {PolygonSelectionMode::Face, {source.faces()[0].id}};
    enabled = true;
    remaining = -1;
    calls = 0;
    auto measured = edit_colored_polygon(source, field, e);
    enabled = false;
    auto count = calls;
    if (!measured.colors || !measured.geometry.replacement || count < 100)
      throw std::runtime_error(
          "Actual colored geometry/field allocations were not measured");
    auto expected = measured.geometry.replacement->encode();
    auto values = measured.colors->packed(*measured.geometry.replacement);
    for (size_t at = 0; at < count; ++at) {
      enabled = true;
      remaining = int64_t(at);
      hit = false;
      try {
        edit_colored_polygon(source, field, e);
      } catch (const std::bad_alloc &) {
      }
      enabled = false;
      if (!hit || source.encode() != original || field.packed(source) != colors)
        throw std::runtime_error("Persistent owned allocation failure changed "
                                 "original geometry, IDs or linear RGBA");
      auto retry = edit_colored_polygon(source, field, e);
      if (!retry.geometry.replacement ||
          retry.geometry.replacement->encode() != expected ||
          retry.colors->packed(*retry.geometry.replacement) != values)
        throw std::runtime_error(
            "Colored geometry did not retry exactly after allocation refusal");
    }
    total += count;
    std::cout << "PASS operation " << int(op) << ": " << count
              << " actual persistent-denial boundaries preserve exact "
                 "geometry, source RGBA and retry\n";
  }
  std::cout << total
            << " persistent colored polygon allocation boundaries passed\n";
  return 0;
} catch (const std::exception &e) {
  enabled = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
