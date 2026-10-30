// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon-authoring.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
bool enabled = false, hit = false;
size_t calls = 0, fail = 0;
void require(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
PolygonWorkspace setup() {
  PolygonWorkspace w(PolygonMesh::cube());
  w.select(w.view().topology->faces()[1].id);
  w.apply(PolygonOperation::Move, {.01f, 0, 0});
  w.apply(PolygonOperation::Move, {0, .01f, 0});
  require(w.undo(), "Fixture retains real local Redo");
  return w;
}
void run(PolygonWorkspace &w, bool end) {
  if (end)
    w.end_move(true);
  else
    w.apply(PolygonOperation::Extrude, {}, .2f);
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
  size_t total = 0;
  for (bool end : {false, true}) {
    auto measure = setup();
    if (end) {
      measure.begin_move();
      measure.move_to({.2f, 0, 0});
    }
    calls = fail = 0;
    enabled = true;
    run(measure, end);
    enabled = false;
    auto count = calls;
    require(count > 20,
            "Real workspace preparation/publication allocations measured");
    auto expected = measure.view().topology->encode();
    for (size_t at = 1; at <= count; ++at) {
      auto w = setup();
      auto original = w.view().topology;
      auto original_surface = w.view().surface;
      if (end) {
        w.begin_move();
        w.move_to({.2f, 0, 0});
      }
      auto view = &w.view();
      auto revision = view->revision;
      auto cursor = w.identity_cursor();
      auto selected = view->selection;
      auto display = view->display;
      calls = 0;
      fail = at;
      hit = false;
      enabled = true;
      try {
        run(w, end);
      } catch (const std::bad_alloc &) {
      }
      enabled = false;
      require(hit, "Every measured workspace boundary was injected");
      require(&w.view() == view && w.view().revision == revision &&
                  w.view().topology == original &&
                  w.view().surface == original_surface &&
                  w.view().selection == selected &&
                  w.view().display == display && w.identity_cursor() == cursor,
              "Failed workspace publication retains exact visible snapshot, "
              "geometry, selection and identity cursor");
      if (end) {
        require(w.view().moving && w.view().move_delta.x == .2f,
                "Rejected accepted-End retains its explicit GPU preview and "
                "Cancel ownership");
        w.end_move(false);
      }
      require(w.redo(), "Earlier Redo survives a rejected workspace edit");
      require(w.undo() && w.view().topology == original,
              "Earlier local history remains exact after rejection");
      if (end) {
        w.begin_move();
        w.move_to({.2f, 0, 0});
      }
      run(w, end);
      require(w.view().topology->encode() == expected,
              "Actual rejected command retries to the exact successful result");
      require(w.undo() && w.view().topology == original,
              "Retry is exactly one local history transition");
    }
    total += count;
    std::cout << "PASS workspace "
              << (end ? "accepted-move-End" : "topology-command")
              << ": every one of " << count
              << " allocation boundaries preserves snapshot/history/redo and "
                 "actual retry\n";
  }
  std::cout << total << " workspace allocation boundaries passed\n";
  return 0;
} catch (const std::exception &e) {
  enabled = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
