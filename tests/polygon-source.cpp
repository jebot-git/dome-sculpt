// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/humanoid-creator.hpp"
#include "polygon-source-fixture.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <numeric>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
  ++checks;
  std::cout << "PASS " << message << '\n';
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
  polygon_test::Fixture f;
  auto old = f.rest;
  auto bytes = f.extras.dump();
  auto result = prepare_polygon_topology(f.rest, &f.appearance);
  check(result.authored_ngons && result.topology.faces().size() == 2 &&
            result.topology.vertices().size() == 6,
        "Verified source loops restore two real adjacent quads with shared "
        "control points");
  check(
      result.topology.edges().size() == 7 &&
          result.topology.faces()[0].corners.size() == 4 &&
          result.topology.faces()[1].corners.size() == 4,
      "Source triangulation diagonals do not become persistent polygon edges");
  auto packed = result.topology.triangulate();
  check(
      result.source_vertices.size() == packed.mesh.positions.size(),
      "Every packed polygon corner retains an exact old native vertex witness");
  for (size_t i = 0; i < result.source_vertices.size(); ++i) {
    auto n = result.source_vertices[i];
    check(packed.mesh.positions[i].x == f.rest.positions[n].x &&
              packed.mesh.positions[i].y == f.rest.positions[n].y &&
              packed.mesh.uv[i] == f.rest.uv[n],
          "Rest positions and seam-side UVs remain exact across N-gon "
          "reconstruction");
  }
  check(std::equal(f.rest.positions.begin(), f.rest.positions.end(),
                   old.positions.begin(),
                   [](Vec3 a, Vec3 b) {
                     return a.x == b.x && a.y == b.y && a.z == b.z;
                   }) &&
            f.rest.indices == old.indices && f.rest.uv == old.uv &&
            f.extras.dump() == bytes,
        "Detached preparation never mutates source geometry, UV or immutable "
        "metadata");
  for (size_t i = 6; i < 12; ++i)
    f.rest.normals[i] = {0, 1, 0};
  auto hard = prepare_polygon_topology(f.rest, &f.appearance);
  check(std::count_if(hard.topology.edges().begin(),
                      hard.topology.edges().end(),
                      [](auto &e) { return e.hard; }) == 1,
        "Legitimate cross-face normal seams preserve a hard shared edge "
        "without splitting the N-gons");
  f.rest = old;
  f.asset->primitives[0].extras_json =
      "{\"dome_source_polygons_version\":1,\"dome_source_polygons_version\":1}";
  check(refuses([&] { prepare_polygon_topology(f.rest, &f.appearance); }),
        "Duplicate original polygon version keys refuse before authority "
        "creation");
  f.asset->primitives[0].extras_json = "{\"dome_source_polygons_version\":1.0}";
  check(refuses([&] { prepare_polygon_topology(f.rest, &f.appearance); }),
        "Floating-point original polygon versions cannot masquerade as "
        "integral authority");
  f.sync();
  PolygonLimits face_budget;
  face_budget.maximum_faces = 1;
  check(refuses([&] {
          prepare_polygon_topology(f.rest, &f.appearance, face_budget);
        }),
        "Original face resource budget is enforced before topology allocation");
  auto fallback = prepare_polygon_topology(f.rest);
  check(
      !fallback.authored_ngons && fallback.topology.faces().size() == 4,
      "Absent source witness keeps explicit triangles without guessing quads");
  auto bad = f.extras;
  f.extras["dome_source_polygons_version"] = 2;
  f.sync();
  check(refuses([&] { prepare_polygon_topology(f.rest, &f.appearance); }),
        "Unknown source polygon version refuses instead of silently "
        "triangulating authority");
  f.extras = bad;
  f.extras["dome_source_polygons"][1]["first_triangle"] = 1;
  f.sync();
  check(refuses([&] { prepare_polygon_topology(f.rest, &f.appearance); }),
        "Overlapping source face triangle ranges refuse");
  f.extras = bad;
  f.extras["dome_source_polygons"][0]["vertices"] = {0, 2, 1, 5};
  f.sync();
  check(refuses([&] { prepare_polygon_topology(f.rest, &f.appearance); }),
        "Incorrect original boundary orientation refuses");
  f.extras = bad;
  f.extras["dome_source_polygons"].erase(1);
  f.sync();
  check(refuses([&] { prepare_polygon_topology(f.rest, &f.appearance); }),
        "Incomplete original face coverage refuses");
  f.extras = bad;
  f.sync();
  f.rest.uv[3][0] += .1f;
  check(refuses([&] { prepare_polygon_topology(f.rest, &f.appearance); }),
        "Repeated same original corner with incompatible UV refuses");
  f.rest = old;
  f.rest.normals[4] = {0, 1, 0};
  check(refuses([&] { prepare_polygon_topology(f.rest, &f.appearance); }),
        "Repeated same original corner with incompatible normals refuses");
  f.rest = old;
  std::swap(f.rest.indices[0], f.rest.indices[1]);
  check(refuses([&] { prepare_polygon_topology(f.rest, &f.appearance); }),
        "Changed native triangle correspondence cannot authorize old N-gon "
        "grouping");
  f.rest = old;
  f.rest.positions[6].x += .01f;
  f.rest.positions[9].x += .01f;
  check(refuses([&] { prepare_polygon_topology(f.rest, &f.appearance); }),
        "Shared original vertex with differing native positions refuses source "
        "welding");
  f.rest = old;
  f.rest.positions.push_back(f.rest.positions[0]);
  f.rest.normals.push_back(f.rest.normals[0]);
  f.rest.uv.push_back(f.rest.uv[0]);
  f.appearance.source_vertices.push_back(0);
  f.rest.indices[0] = 12;
  check(prepare_polygon_topology(f.rest, &f.appearance).topology.encode() ==
            result.topology.encode(),
        "Identical native splits of one original accessor preserve the exact "
        "source N-gons");
  f.rest.uv[12][0] += .01f;
  check(refuses([&] { prepare_polygon_topology(f.rest, &f.appearance); }),
        "Incompatible native splits cannot masquerade as an original shared "
        "accessor");
  f.rest = old;
  f.appearance.source_vertices.pop_back();
  PolygonLimits limits;
  size_t visits = 0;
  limits.cancelled = [&] {
    ++visits;
    return false;
  };
  prepare_polygon_topology(f.rest, &f.appearance, limits);
  size_t total = visits;
  for (size_t fail = 1; fail <= total; ++fail) {
    visits = 0;
    limits.cancelled = [&] { return ++visits == fail; };
    if (!refuses(
            [&] { prepare_polygon_topology(f.rest, &f.appearance, limits); }))
      throw std::runtime_error(
          "Source preparation cancellation checkpoint was not observed");
  }
  check(total > 0, "Every measured source preparation cancellation is observed "
                   "without publishing a partial authority");
  std::cout << total << " source cancellation checkpoints passed\n";
  limits = {};
  limits.maximum_work = 3;
  check(
      refuses([&] { prepare_polygon_topology(f.rest, &f.appearance, limits); }),
      "Bounded source preparation refuses an insufficient aggregate work "
      "budget");
  check(prepare_polygon_topology(f.rest, &f.appearance).topology.encode() ==
            result.topology.encode(),
        "Canceled and refused preparations retry to the exact original N-gon "
        "authority");
  std::cout << checks << " polygon source checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
