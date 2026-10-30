// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
bool enabled = false, hit = false;
size_t calls = 0, fail = 0;
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
  auto source = PolygonMesh::cube();
  auto before = source.encode();
  size_t total = 0;
  std::vector<PolygonEdit> edits = {
      {PolygonOperation::Extrude,
       {PolygonSelectionMode::Face, {source.faces()[0].id}},
       {},
       .2f},
      {PolygonOperation::Inset,
       {PolygonSelectionMode::Face, {source.faces()[0].id}},
       {},
       .1f},
      {PolygonOperation::CutEdges,
       {PolygonSelectionMode::Edge, {source.edges()[0].id}},
       {},
       0,
       2}};
  for (auto &edit : edits) {
    calls = fail = 0;
    enabled = true;
    auto measured = edit_polygon(source, edit);
    enabled = false;
    auto count = calls;
    if (count < 10 || !measured.replacement)
      throw std::runtime_error(
          "Real detached polygon allocations were not measured");
    auto expected = measured.replacement->encode();
    for (size_t at = 1; at <= count; ++at) {
      calls = 0;
      fail = at;
      hit = false;
      enabled = true;
      try {
        edit_polygon(source, edit);
      } catch (const std::bad_alloc &) {
      }
      enabled = false;
      if (!hit || source.encode() != before)
        throw std::runtime_error("An injected polygon allocation changed its "
                                 "original authority or ID counter");
      auto retry = edit_polygon(source, edit);
      if (!retry.replacement || retry.replacement->encode() != expected)
        throw std::runtime_error(
            "Rejected polygon edit did not retry deterministically");
    }
    total += count;
    std::cout << "PASS polygon operation " << int(edit.operation)
              << ": every one of " << count
              << " measured allocation boundaries preserves exact source and "
                 "successful retry\n";
  }
  if (source.encode() != before)
    throw std::runtime_error(
        "Polygon source changed after complete allocation matrix");
  std::cout << total << " polygon allocation boundaries passed\n";
  return 0;
} catch (const std::exception &e) {
  enabled = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
