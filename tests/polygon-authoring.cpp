// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon-authoring.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
  ++checks;
  std::cout << "PASS " << m << '\n';
}
template <class F> bool refuses(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
} // namespace
int main() try {
  auto cube = PolygonMesh::cube();
  auto original = cube.encode();
  PolygonWorkspace w(cube);
  auto original_surface = w.view().surface;
  check(!w.view().changed && !w.view().can_undo && !w.view().can_redo,
        "New polygon workspace has immutable source and empty local history");
  check(w.select_ray({0, 0, 3}, {0, 0, -1}, 10, .03f) &&
            w.view().selection->ids ==
                std::vector<PolygonId>{cube.faces()[1].id},
        "Actual BVH ray picks the original N-gon rather than a display "
        "triangle ID");
  check(!w.view().changed && !w.view().can_undo,
        "Face selection changes no geometry or model history");
  auto before = w.view().topology;
  w.apply(PolygonOperation::Extrude, {}, .2f);
  auto accepted = w.view().topology;
  check(accepted->faces().size() == 10 && w.view().changed && w.view().can_undo,
        "Worker workspace performs real face extrusion and makes one local "
        "undoable edit");
  auto reserved = w.identity_cursor();
  check(
      w.undo() && w.view().topology == before &&
          w.view().surface == original_surface && !w.view().changed &&
          w.view().can_redo,
      "Local Undo restores exact original topology and immutable render owner");
  check(w.redo() && w.view().topology == accepted,
        "Local Redo restores the same prepared topology owner");
  check(w.undo(), "Return to source before creating a new history branch");
  w.apply(PolygonOperation::Inset, {}, .1f);
  check(w.view().topology->next_id() > reserved && !w.view().can_redo,
        "New Undo branch retires discarded draft identities and clears its old "
        "Redo");
  for (auto &v : w.view().topology->vertices())
    if (v.id >= cube.next_id() && v.id < reserved)
      throw std::runtime_error(
          "Discarded preview vertex identity was recycled");
  check(true, "Workspace allocation cursor never recycles visible draft "
              "vertices across branching");
  w.undo();
  w.mode(PolygonSelectionMode::Vertex);
  check(
      w.select_ray({.49f, .49f, 3}, {0, 0, -1}, 10, .03f) &&
          w.view().selection->ids == std::vector<PolygonId>{7},
      "Vertex picking selects only a nearby component on the visible hit face");
  check(!w.select_ray({0, 0, 3}, {0, 0, -1}, 10, .01f),
        "Vertex picking does not snap distant components through the face "
        "interior");
  w.select(6, true);
  check(w.view().selection->ids.size() == 2,
        "Additive component selection preserves earlier selected vertices");
  w.select(6, true, true);
  check(w.view().selection->ids == std::vector<PolygonId>{7},
        "Toggle selection removes only the specified component");
  w.begin_move();
  w.move_to({.1f, 0, 0});
  w.move_to({.2f, 0, 0});
  auto preview = w.view();
  auto base_vertex = std::find_if(preview.topology->vertices().begin(),
                                  preview.topology->vertices().end(),
                                  [](auto &v) { return v.id == 7; });
  check(base_vertex != preview.topology->vertices().end() &&
            base_vertex->position.x == .5f && preview.move_delta.x == .2f &&
            preview.surface == original_surface,
        "Continuous motion retains immutable CPU geometry while publishing one "
        "total GPU displacement");
  check(w.view().moving && !w.view().can_redo,
        "Unaccepted move is an explicit cancellable local gesture");
  w.end_move(true);
  auto moved = w.view().topology;
  auto vertex = std::find_if(moved->vertices().begin(), moved->vertices().end(),
                             [](auto &v) { return v.id == 7; });
  check(vertex != moved->vertices().end() && vertex->position.x == .7f,
        "Accepted GPU preview prepares the exact final CPU authority once at "
        "End");
  check(!w.view().moving && w.view().can_undo && w.undo() &&
            w.view().topology->encode() == original,
        "Accepted continuous move is exactly one local Undo transition");
  check(w.redo() && w.view().topology == moved,
        "Continuous move Redo reuses exact immutable result");
  w.undo();
  w.begin_move();
  w.move_to({.05f, 0, 0});
  w.end_move(false);
  check(w.view().topology->encode() == original && !w.view().moving,
        "Cancel restores exact pre-gesture source without accepting its latest "
        "preview");
  w.begin_move();
  w.move_to({.1f, 0, 0});
  w.move_to({0, 0, 0});
  w.end_move(true);
  check(
      w.view().topology->encode() == original && !w.view().can_undo,
      "A gesture returning to its initial position creates no history command");
  w.begin_move();
  w.move_to({0, 0, -1});
  check(refuses([&] { w.end_move(true); }),
        "Collapsed or folded component moves fail before exposing invalid "
        "geometry");
  w.end_move(false);
  check(w.view().topology->encode() == original,
        "Rejected move remains cancellable to exact original topology");
  w.mode(PolygonSelectionMode::Edge);
  check(w.select_ray({.49f, 0, 3}, {0, 0, -1}, 10, .03f) &&
            w.view().selection->ids.size() == 1,
        "Edge ray picking finds a visible edge in object-local metric radius");
  auto selected = w.view().selection->ids[0];
  auto edge = std::find_if(cube.edges().begin(), cube.edges().end(),
                           [&](auto &e) { return e.id == selected; });
  check(edge != cube.edges().end() && edge->a == 6 && edge->b == 7,
        "Edge pick identity matches independently known geometric endpoints");
  w.apply(PolygonOperation::CutEdges, {}, 0, 2);
  check(w.view().topology->vertices().size() == 10 && w.view().can_undo,
        "Selected shared edge cuts run through the actual workspace command");
  w.undo();
  w.mode(PolygonSelectionMode::Face);
  w.select_all();
  w.apply(PolygonOperation::DeleteFaces);
  check(w.view().topology->faces().empty() &&
            !w.select_ray({0, 0, 3}, {0, 0, -1}, 10, .03f),
        "An empty editable object has no stale BVH or selectable old faces");
  check(w.undo() && w.view().topology->encode() == original,
        "Deleting every face remains exactly undoable");
  PolygonLimits cancel;
  cancel.cancelled = [] { return true; };
  auto keep = w.view().topology;
  check(refuses([&] {
          w.apply(PolygonOperation::Move, {.1f, 0, 0}, 0, 1, cancel);
        }) &&
            w.view().topology == keep,
        "Cancelled worker preparation retains the complete prior workspace "
        "authority");
  check(
      cube.encode() == original,
      "Entire component editing session leaves its source argument immutable");
  std::cout << checks << " polygon workspace checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
