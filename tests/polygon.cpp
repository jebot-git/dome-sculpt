// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon.hpp"
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool yes, const char *why) {
  if (!yes)
    throw std::runtime_error(why);
  ++checks;
  std::cout << "PASS " << why << '\n';
}
template <class F> bool refuses(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
PolygonMesh apply(const PolygonMesh &m, PolygonOperation op,
                  PolygonSelectionMode mode, std::vector<PolygonId> ids,
                  Vec3 delta = {}, float amount = 0, unsigned cuts = 1) {
  PolygonEdit e{op, {mode, std::move(ids)}, delta, amount, cuts};
  auto result = edit_polygon(m, e);
  if (!result.replacement)
    throw std::runtime_error("Expected an actual polygon edit");
  return std::move(*result.replacement);
}
long euler(const PolygonMesh &m) {
  return long(m.vertices().size()) - long(m.edges().size()) +
         long(m.faces().size());
}
double triangle_area(const Mesh &m) {
  double area = 0;
  for (size_t i = 0; i < m.indices.size(); i += 3) {
    auto a = m.positions[m.indices[i]], b = m.positions[m.indices[i + 1]],
         c = m.positions[m.indices[i + 2]];
    double x = (double(b.y) - a.y) * (double(c.z) - a.z) -
               (double(b.z) - a.z) * (double(c.y) - a.y),
           y = (double(b.z) - a.z) * (double(c.x) - a.x) -
               (double(b.x) - a.x) * (double(c.z) - a.z),
           z = (double(b.x) - a.x) * (double(c.y) - a.y) -
               (double(b.y) - a.y) * (double(c.x) - a.x);
    area += .5 * std::sqrt(x * x + y * y + z * z);
  }
  return area;
}
} // namespace
int main() try {
  auto cube = PolygonMesh::cube();
  auto original = cube.encode();
  check(cube.vertices().size() == 8 && cube.edges().size() == 12 &&
            cube.faces().size() == 6 && euler(cube) == 2,
        "Cube authority stores six real quadrilateral faces and a closed "
        "manifold");
  auto rendered = cube.triangulate();
  check(rendered.mesh.indices.size() == 36 &&
            rendered.triangle_faces.size() == 12 &&
            rendered.corners.size() == 24,
        "Triangulation is a separate derivative with exact component picking "
        "maps");
  check(std::abs(triangle_area(rendered.mesh) - 6) < 1e-12,
        "Independent triangle area matches the unit cube surface");
  check(PolygonMesh::decode(original).encode() == original,
        "Native polygon archive retains every stable identity, corner UV and "
        "source weight");
  std::vector<PolygonId> all;
  for (auto &v : cube.vertices())
    all.push_back(v.id);
  auto moved = apply(cube, PolygonOperation::Move, PolygonSelectionMode::Vertex,
                     all, {2, 3, 4});
  check(moved.vertices()[0].position.x == cube.vertices()[0].position.x + 2 &&
            moved.next_id() == cube.next_id(),
        "Moving selected vertices preserves component identities without "
        "consuming new IDs");
  check(moved.encode()["edges"] == original["edges"] &&
            moved.encode()["faces"] == original["faces"],
        "Move preserves exact N-gons, edges and per-corner UVs");
  auto face = cube.faces()[0].id;
  auto extruded = apply(cube, PolygonOperation::Extrude,
                        PolygonSelectionMode::Face, {face}, {}, .25f);
  check(extruded.vertices().size() == 12 && extruded.faces().size() == 10 &&
            euler(extruded) == 2,
        "Face extrusion creates a top and four side quads without opening the "
        "solid");
  check(extruded.faces()[0].id == face &&
            extruded.faces()[0].corners[0].id == cube.faces()[0].corners[0].id,
        "Extrusion retains selected face and corner identities on the editable "
        "top");
  auto top = extruded.faces()[0].corners[0].vertex;
  auto tv = std::find_if(extruded.vertices().begin(), extruded.vertices().end(),
                         [&](auto &v) { return v.id == top; });
  check(tv != extruded.vertices().end() && tv->position.z == -.75f &&
            tv->provenance == cube.vertices()[0].provenance,
        "New extrusion vertices retain exact source correspondence and "
        "object-local distance");
  auto region =
      apply(cube, PolygonOperation::Extrude, PolygonSelectionMode::Face,
            {face, cube.faces()[2].id}, {}, .1f);
  check(region.vertices().size() == 14 && region.faces().size() == 12 &&
            euler(region) == 2,
        "Adjacent region extrusion shares its six new vertices and creates "
        "only outer side walls");
  auto opposite =
      apply(cube, PolygonOperation::Extrude, PolygonSelectionMode::Face,
            {face, cube.faces()[1].id}, {}, .1f);
  check(opposite.vertices().size() == 16 && opposite.faces().size() == 14 &&
            euler(opposite) == 2,
        "Disconnected face regions extrude independently along their own "
        "directions");
  auto inset = apply(cube, PolygonOperation::Inset, PolygonSelectionMode::Face,
                     {face}, {}, .1f);
  check(inset.vertices().size() == 12 && inset.faces().size() == 10 &&
            euler(inset) == 2,
        "Inset preserves a true inner quad and four connected ring faces");
  check(std::abs(inset.faces()[0].corners[0].uv[0] - .1f) < 1e-6 &&
            std::abs(inset.faces()[0].corners[0].uv[1] - .1f) < 1e-6,
        "Inset interpolates old face UVs instead of replacing them with a "
        "zero-area side mapping");
  auto edge = cube.edges()[0].id;
  auto cut = apply(cube, PolygonOperation::CutEdges, PolygonSelectionMode::Edge,
                   {edge}, {}, 0, 2);
  check(
      cut.vertices().size() == 10 && cut.edges().size() == 14 &&
          euler(cut) == 2,
      "Two edge cuts insert shared vertices into both incident polygon rings");
  check(std::none_of(cut.edges().begin(), cut.edges().end(),
                     [&](auto &e) { return e.id == edge; }) &&
            cut.next_id() > cube.next_id(),
        "Removed edge identities are retired, never reused by child segments");
  auto quad = PolygonMesh::from_faces(
      std::vector<Vec3>{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}},
      {{0, 1, 2, 3}});
  auto qids = quad.faces()[0].corners;
  auto connected =
      apply(quad, PolygonOperation::ConnectVertices,
            PolygonSelectionMode::Vertex, {qids[0].vertex, qids[2].vertex});
  check(connected.faces().size() == 2 && connected.edges().size() == 5,
        "Connecting opposite vertices creates two genuine faces and one "
        "selectable diagonal");
  PolygonId diagonal = 0;
  for (auto &e : connected.edges())
    if (std::minmax(e.a, e.b) == std::minmax(qids[0].vertex, qids[2].vertex))
      diagonal = e.id;
  auto dissolved = apply(connected, PolygonOperation::Dissolve,
                         PolygonSelectionMode::Edge, {diagonal});
  check(dissolved.faces().size() == 1 &&
            dissolved.faces()[0].corners.size() == 4 &&
            dissolved.edges().size() == 4,
        "Dissolving the diagonal reconstructs one quadrilateral instead of "
        "retaining triangulated patches");
  auto seam = connected.encode();
  for (auto &c : seam["faces"][1]["corners"])
    if (c[1] == qids[0].vertex)
      c[2][0] = .3f;
  auto seammesh = PolygonMesh::decode(seam);
  check(refuses([&] {
          apply(seammesh, PolygonOperation::Dissolve,
                PolygonSelectionMode::Edge, {diagonal});
        }),
        "Dissolve refuses to discard differing corner UVs across a seam");
  auto hard = connected.encode();
  for (auto &e : hard["edges"])
    if (e[0] == diagonal)
      e[3] = true;
  auto hardmesh = PolygonMesh::decode(hard);
  check(refuses([&] {
          apply(hardmesh, PolygonOperation::Dissolve,
                PolygonSelectionMode::Edge, {diagonal});
        }),
        "Dissolve refuses silent removal of a protected hard edge");
  auto cut_hard = apply(hardmesh, PolygonOperation::CutEdges,
                        PolygonSelectionMode::Edge, {diagonal}, {}, 0, 2);
  check(std::count_if(cut_hard.edges().begin(), cut_hard.edges().end(),
                      [](auto &e) { return e.hard; }) == 3,
        "Cut child segments preserve the original hard-edge authority");
  check(std::abs(triangle_area(cut_hard.triangulate().mesh) - 1) < 1e-12,
        "Splitting a diagonal retains all collinear boundary corners without "
        "losing surface area");
  auto welded =
      apply(cube, PolygonOperation::Weld, PolygonSelectionMode::Vertex,
            {cube.edges()[0].a, cube.edges()[0].b});
  check(welded.vertices().size() == 7 && euler(welded) == 2,
        "Welding an edge updates every incident face and retains a closed "
        "manifold");
  auto target =
      std::find_if(welded.vertices().begin(), welded.vertices().end(),
                   [&](auto &v) { return v.id == cube.edges()[0].a; });
  check(target != welded.vertices().end() && target->provenance.size() == 2 &&
            target->provenance[0].second == .5 &&
            target->provenance[1].second == .5,
        "Weld records normalized source interpolation without silently "
        "dropping identities");
  std::vector<Vec3> bv;
  for (float dz : {-1.5f, 1.5f})
    for (auto &v : cube.vertices())
      bv.push_back(v.position + Vec3{0, 0, dz});
  std::vector<std::vector<uint32_t>> bf;
  for (unsigned offset : {0u, 8u})
    for (auto &f : cube.faces()) {
      std::vector<uint32_t> ring;
      for (auto &c : f.corners)
        ring.push_back(uint32_t(c.vertex - 1) + offset);
      bf.push_back(ring);
    }
  auto two = PolygonMesh::from_faces(bv, bf);
  auto bridged =
      apply(two, PolygonOperation::Bridge, PolygonSelectionMode::Face,
            {two.faces()[1].id, two.faces()[6].id});
  check(bridged.vertices().size() == 16 && bridged.faces().size() == 14 &&
            euler(bridged) == 2,
        "Face bridge joins two closed solids using four oriented quads and a "
        "closed manifold");
  auto opened = apply(cube, PolygonOperation::DeleteFaces,
                      PolygonSelectionMode::Face, {face});
  check(opened.faces().size() == 5 && euler(opened) == 1,
        "Deleting a face creates an intentional manifold boundary");
  std::vector<PolygonId> fids;
  for (auto &f : cube.faces())
    fids.push_back(f.id);
  auto empty = apply(cube, PolygonOperation::DeleteFaces,
                     PolygonSelectionMode::Face, fids);
  check(empty.vertices().empty() && empty.faces().empty() &&
            empty.edges().empty() && empty.next_id() == cube.next_id(),
        "Deleting all faces leaves an empty editable object while retaining "
        "retired IDs");
  auto flipped = apply(cube, PolygonOperation::FlipFaces,
                       PolygonSelectionMode::Face, fids);
  check(flipped.triangulate().mesh.normals[0].z == -rendered.mesh.normals[0].z,
        "Flipping an entire shell reverses its render normals without changing "
        "geometry");
  check(refuses([&] {
          apply(cube, PolygonOperation::FlipFaces, PolygonSelectionMode::Face,
                {face});
        }),
        "A flip that conflicts with adjacent edge orientation is rejected "
        "atomically");
  auto concave = PolygonMesh::from_faces(
      std::vector<Vec3>{
          {0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {1, 1, 0}, {1, 2, 0}, {0, 2, 0}},
      {{0, 1, 2, 3, 4, 5}});
  check(concave.faces()[0].corners.size() == 6 &&
            concave.triangulate().mesh.indices.size() == 12 &&
            std::abs(triangle_area(concave.triangulate().mesh) - 3) < 1e-12,
        "Concave N-gons use checked ear clipping with an independent area "
        "oracle");
  check(refuses([&] {
          apply(concave, PolygonOperation::Inset, PolygonSelectionMode::Face,
                {concave.faces()[0].id}, {}, .1f);
        }),
        "Unsupported concave inset is refused explicitly without triangulating "
        "away polygon authority");
  for (float scale : {1e-12f, 1.f, 1e12f}) {
    auto tiny = PolygonMesh::from_faces(
        std::vector<Vec3>{{0, 0, 0}, {scale, 0, 0}, {0, scale, 0}},
        {{0, 1, 2}});
    check(tiny.triangulate().mesh.indices.size() == 3,
          "Scale-independent predicates retain valid tiny and large triangles");
  }
  check(refuses([&] {
          PolygonMesh::from_faces(
              std::vector<Vec3>{{0, 0, 0}, {1, 0, 0}, {2, 0, 0}}, {{0, 1, 2}});
        }),
        "Exactly collinear faces remain invalid");
  check(
      refuses([&] {
        PolygonMesh::from_faces(
            std::vector<Vec3>{
                {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {-1, 0, 0}, {0, -1, 0}},
            {{0, 1, 2}, {0, 3, 4}});
      }),
      "Disconnected face fans cannot be welded through a single bowtie vertex");
  auto corrupt = original;
  corrupt["faces"][1]["corners"][0][0] = corrupt["faces"][0]["corners"][0][0];
  check(refuses([&] { PolygonMesh::decode(corrupt); }),
        "Persistence rejects duplicate selectable corner identities");
  corrupt = original;
  corrupt["extra"] = true;
  check(refuses([&] { PolygonMesh::decode(corrupt); }),
        "Persistence rejects unknown authority fields rather than losing newer "
        "data");
  corrupt = original;
  corrupt["next_id"] = UINT64_MAX;
  corrupt["vertices"][0][0] = -2;
  check(refuses([&] { PolygonMesh::decode(corrupt); }),
        "Archive IDs reject negative integers before unsigned conversion");
  corrupt = original;
  corrupt["version"] = 1.0;
  check(refuses([&] { PolygonMesh::decode(corrupt); }),
        "Archive versions require integer authority tags");
  auto micro = PolygonEdit{PolygonOperation::Move,
                           {PolygonSelectionMode::Vertex, all},
                           {1e-30f, 0, 0}};
  check(
      !edit_polygon(cube, micro).replacement,
      "Unrepresentable sub-ULP movement creates no history-worthy replacement");
  PolygonEdit noop;
  noop.selection = {PolygonSelectionMode::Face, {face}};
  check(
      !edit_polygon(cube, noop).replacement && cube.encode() == original,
      "Zero movement creates no replacement or history-worthy identity change");
  PolygonEdit ex{
      PolygonOperation::Extrude, {PolygonSelectionMode::Face, {face}}, {}, .2f};
  size_t callbacks = 0;
  PolygonLimits measured;
  measured.cancelled = [&] {
    ++callbacks;
    return false;
  };
  check(bool(edit_polygon(cube, ex, measured).replacement) && callbacks > 50,
        "Real validation and topology preparation expose bounded cancellation "
        "checkpoints");
  for (size_t stop = 1; stop <= callbacks; ++stop) {
    size_t calls = 0;
    PolygonLimits l;
    l.cancelled = [&] { return ++calls == stop; };
    if (!refuses([&] { edit_polygon(cube, ex, l); }) ||
        cube.encode() != original)
      throw std::runtime_error(
          "Cancellation failed to preserve exact polygon source");
  }
  check(true, "Every measured cancellation checkpoint preserves the exact "
              "authority and next-ID counter");
  PolygonLimits low;
  low.maximum_work = 10;
  check(refuses([&] { edit_polygon(cube, ex, low); }) &&
            cube.encode() == original,
        "Work exhaustion is explicit and leaves source topology exact");
  low = {};
  low.maximum_vertices = 9;
  check(refuses([&] { edit_polygon(cube, ex, low); }) &&
            cube.encode() == original,
        "Expansion respects the vertex budget before publication");
  low = {};
  low.maximum_face_corners = 4;
  PolygonEdit splitting{PolygonOperation::CutEdges,
                        {PolygonSelectionMode::Edge, {edge}},
                        {},
                        0,
                        2};
  check(refuses([&] { edit_polygon(cube, splitting, low); }) &&
            cube.encode() == original,
        "Cut expansion checks per-face corner capacity before allocating its "
        "new topology");
  size_t decode_callbacks = 0;
  PolygonLimits dl;
  dl.cancelled = [&] { return ++decode_callbacks == 3; };
  check(refuses([&] { PolygonMesh::decode(original, dl); }),
        "Archive preparation observes cancellation before constructing all "
        "component tables");
  auto grid = PolygonMesh::grid(4, 5, 2);
  check(grid.vertices().size() == 30 && grid.faces().size() == 20 &&
            std::abs(triangle_area(grid.triangulate().mesh) - 4) < 1e-12,
        "Grid primitive retains genuine quads, shared boundaries and "
        "independently measured physical area");
  size_t work10 = 0, work20 = 0;
  for (unsigned n : {10u, 20u}) {
    auto mesh = PolygonMesh::grid(n, n);
    std::vector<PolygonId> selection;
    for (auto &f : mesh.faces())
      selection.push_back(f.id);
    PolygonLimits limits;
    limits.cancelled = [&] {
      ++(n == 10 ? work10 : work20);
      return false;
    };
    auto result = edit_polygon(mesh,
                               {PolygonOperation::Move,
                                {PolygonSelectionMode::Face, selection},
                                {.01f, 0, 0}},
                               limits);
    if (!result.replacement)
      throw std::runtime_error("Grid movement fixture did not change geometry");
  }
  check(work20 > 3 * work10 && work20 < 5 * work10,
        "Indexed component access keeps measured validation and selection work "
        "proportional to grid size");
  auto translated = PolygonMesh::from_faces(
      std::vector<Vec3>{
          {1e8f, 1e8f, 1e8f}, {1e8f + 16, 1e8f, 1e8f}, {1e8f, 1e8f + 16, 1e8f}},
      {{0, 1, 2}});
  check(translated.triangulate().mesh.indices.size() == 3,
        "Translated valid triangles retain robust double-precision area "
        "classification");
  auto ticketed = ex;
  ticketed.first_id = 100000;
  auto branch = edit_polygon(cube, ticketed);
  check(branch.replacement && std::all_of(branch.created_vertices.begin(),
                                          branch.created_vertices.end(),
                                          [](auto id) { return id >= 100000; }),
        "Workspace high-water tickets prevent recycled IDs after local Undo "
        "and branching");
  check(cube.encode() == original,
        "All successful and failed detached workflows leave their original "
        "source untouched");
  std::cout << checks << " polygon kernel checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
