// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon-color.hpp"
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
  ++checks;
  std::cout << "PASS " << m << std::endl;
}
template <class F> bool refuses(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
bool close(PolygonRgba a, PolygonRgba b) {
  for (size_t c = 0; c < 4; ++c)
    if (std::abs(a[c] - b[c]) > 1e-6)
      return false;
  return true;
}
PolygonColorField colors(const PolygonMesh &m, bool uniform = false) {
  std::vector<PolygonCornerRgba> samples;
  std::map<PolygonId, Vec3> positions;
  for (auto &v : m.vertices())
    positions[v.id] = v.position;
  for (size_t f = 0; f < m.faces().size(); ++f)
    for (auto &c : m.faces()[f].corners) {
      auto p = positions.at(c.vertex);
      samples.push_back(
          {c.id, uniform ? PolygonRgba{.2f, .4f, .6f, .25f}
                         : PolygonRgba{float(f) / 8, (p.x + .5f), p.y + .5f,
                                       .25f + (p.z + .5f) * .5f}});
    }
  return PolygonColorField::from_corners(m, samples);
}
PolygonColoredEdit apply(const PolygonMesh &m, const PolygonColorField &f,
                         PolygonOperation op, PolygonSelectionMode mode,
                         std::vector<PolygonId> ids, Vec3 delta = {},
                         float amount = 0, unsigned cuts = 1) {
  PolygonEdit edit{op, {mode, std::move(ids)}, delta, amount, cuts};
  auto result = edit_colored_polygon(m, f, edit);
  auto ordinary = edit_polygon(m, edit);
  check(
      result.geometry.replacement.has_value() ==
              ordinary.replacement.has_value() &&
          (!ordinary.replacement || ordinary.replacement->encode() ==
                                        result.geometry.replacement->encode()),
      "Optional color provenance preserves the exact ordinary topology "
      "operation and IDs");
  if (!result.colors || !result.geometry.replacement)
    throw std::runtime_error("Expected colored edit");
  result.colors->validate(*result.geometry.replacement);
  return result;
}
} // namespace
int main() try {
  auto cube = PolygonMesh::cube();
  auto field = colors(cube);
  const auto original = cube.encode();
  const auto values = field.packed(cube);
  check(values.size() == 24 && field.sources().size() == 24,
        "Geometry-owned RGBA keeps independent face-corner samples at eight "
        "shared vertices");
  std::vector<PolygonId> vertices;
  for (auto &v : cube.vertices())
    vertices.push_back(v.id);
  auto moved = apply(cube, field, PolygonOperation::Move,
                     PolygonSelectionMode::Vertex, vertices, {.2f, .3f, .4f});
  check(moved.colors->packed(*moved.geometry.replacement) == values,
        "Pure geometry motion preserves exact linear corner RGBA without "
        "resampling source textures");
  auto first = cube.faces()[0].id;
  auto ex = apply(cube, field, PolygonOperation::Extrude,
                  PolygonSelectionMode::Face, {first}, {}, .25f);
  for (auto &c : cube.faces()[0].corners)
    check(ex.colors->sample(c.id) == field.sample(c.id),
          "Extruded top retains exact corner RGBA including straight alpha");
  for (auto &f : ex.geometry.replacement->faces())
    if (f.id >= cube.next_id())
      for (auto &c : f.corners) {
        auto color = ex.colors->sample(c.id);
        check(color[0] == 0 && color[3] == .25f,
              "Extrusion side colors come from its boundary face side, not "
              "another shared-vertex seam");
      }
  auto inset = apply(cube, field, PolygonOperation::Inset,
                     PolygonSelectionMode::Face, {first}, {}, .125f);
  std::map<PolygonId, Vec3> pos;
  for (auto &v : inset.geometry.replacement->vertices())
    pos[v.id] = v.position;
  for (auto &c : inset.geometry.replacement->faces()[0].corners) {
    auto p = pos.at(c.vertex);
    check(close(inset.colors->sample(c.id), {0, p.x + .5f, p.y + .5f, .25f}),
          "Inset interpolates all four linear channels from the actual "
          "containing source triangle");
  }
  auto edge = cube.edges()[0];
  auto cut = apply(cube, field, PolygonOperation::CutEdges,
                   PolygonSelectionMode::Edge, {edge.id}, {}, 0, 2);
  size_t cut_corners = 0;
  for (size_t f = 0; f < cut.geometry.replacement->faces().size(); ++f)
    for (auto &c : cut.geometry.replacement->faces()[f].corners)
      if (c.id >= cube.next_id()) {
        auto vertex = std::find_if(cut.geometry.replacement->vertices().begin(),
                                   cut.geometry.replacement->vertices().end(),
                                   [&](auto &v) { return v.id == c.vertex; });
        auto p = vertex->position;
        check(
            close(cut.colors->sample(c.id), {float(f) / 8, p.x + .5f, p.y + .5f,
                                             .25f + (p.z + .5f) * .5f}),
            "Edge cut interpolates geometry-distance RGBA separately on each "
            "incident face seam");
        ++cut_corners;
      }
  check(cut_corners == 4, "Two cuts on a closed shared edge produce four "
                          "independent corner attributes");
  auto &face = cube.faces()[0];
  auto connected = apply(cube, field, PolygonOperation::ConnectVertices,
                         PolygonSelectionMode::Vertex,
                         {face.corners[0].vertex, face.corners[2].vertex});
  check(connected.colors->corners().size() == 26,
        "Connect duplicates endpoint attributes without merging face-side "
        "identities");
  auto welded = apply(cube, field, PolygonOperation::Weld,
                      PolygonSelectionMode::Vertex, {edge.a, edge.b});
  size_t blended = 0;
  for (auto &row : welded.colors->corners())
    if (row.provenance.size() > 1) {
      PolygonRgba expected{};
      for (auto [id, w] : row.provenance) {
        auto old = field.sample(id);
        for (size_t k = 0; k < 4; ++k)
          expected[k] += old[k] * float(w);
      }
      check(close(welded.colors->sample(row.corner), expected),
            "Weld averages collapsed corners on their own face without "
            "crossing color seams");
      ++blended;
    }
  check(blended == 2,
        "Edge weld blends only the two collapsed face-corner pairs");
  std::vector<PolygonId> faces;
  for (auto &f : cube.faces())
    faces.push_back(f.id);
  auto flipped = apply(cube, field, PolygonOperation::FlipFaces,
                       PolygonSelectionMode::Face, faces);
  for (auto &c : field.corners())
    check(flipped.colors->sample(c.corner) == field.sample(c.corner),
          "Flipping preserves exact independent RGBA source values");
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
      bf.push_back(std::move(ring));
    }
  auto two = PolygonMesh::from_faces(bv, bf);
  std::vector<PolygonCornerRgba> endpoints;
  for (size_t f = 0; f < two.faces().size(); ++f)
    for (auto &c : two.faces()[f].corners)
      endpoints.push_back(
          {c.id, {float(f) / 16, .25f, .5f, f < 6 ? 0.f : 1.f}});
  auto two_colors = PolygonColorField::from_corners(two, endpoints);
  auto bridged =
      apply(two, two_colors, PolygonOperation::Bridge,
            PolygonSelectionMode::Face, {two.faces()[1].id, two.faces()[6].id});
  size_t lower = 0, upper = 0;
  for (auto &f : bridged.geometry.replacement->faces())
    if (f.id >= two.next_id())
      for (auto &c : f.corners) {
        auto value = bridged.colors->sample(c.id);
        if (value[3] == 0) {
          ++lower;
          check(value[0] == 1.f / 16,
                "Bridge lower boundary preserves its own original face color");
        } else {
          ++upper;
          check(value[3] == 1 && value[0] == 6.f / 16,
                "Bridge upper boundary preserves its independent original "
                "color and alpha");
        }
      }
  check(lower == 8 && upper == 8, "Bridge provides straight-alpha endpoints on "
                                  "four interpolated side quads");
  auto empty = apply(cube, field, PolygonOperation::DeleteFaces,
                     PolygonSelectionMode::Face, faces);
  check(empty.colors->corners().empty() &&
            empty.colors->sources().size() == 24 &&
            empty.colors->packed(*empty.geometry.replacement).empty(),
        "Delete-all retains immutable original RGBA basis while exposing no "
        "stale current color surface");
  PolygonEdit noop{PolygonOperation::Move,
                   {PolygonSelectionMode::Face, {first}}};
  auto unchanged = edit_colored_polygon(cube, field, noop);
  check(!unchanged.geometry.replacement && !unchanged.colors,
        "Exact no-op creates neither a new topology nor color authority");
  auto invalid = field.sources();
  invalid[0].value[3] = -1;
  check(
      refuses([&] { PolygonColorField::from_corners(cube, invalid); }),
      "Invalid original alpha is refused instead of clamped or premultiplied");
  auto tri =
      PolygonMesh::from_triangles(PolygonMesh::grid().triangulate().mesh);
  auto uniform = colors(tri, true);
  auto middle =
      std::find_if(tri.edges().begin(), tri.edges().end(), [&](auto &e) {
        size_t uses = 0;
        for (auto &f : tri.faces())
          for (size_t i = 0; i < f.corners.size(); ++i)
            uses += std::minmax(f.corners[i].vertex,
                                f.corners[(i + 1) % f.corners.size()].vertex) ==
                    std::minmax(e.a, e.b);
        return uses == 2;
      });
  auto dissolved = apply(tri, uniform, PolygonOperation::Dissolve,
                         PolygonSelectionMode::Edge, {middle->id});
  check(dissolved.geometry.replacement->faces().size() == 1,
        "Constant-field dissolve preserves current RGBA while removing a "
        "genuine interior edge");
  auto seam = colors(tri);
  check(refuses([&] {
          edit_colored_polygon(tri, seam,
                               {PolygonOperation::Dissolve,
                                {PolygonSelectionMode::Edge, {middle->id}}});
        }),
        "Unproven nonconstant dissolve refuses color seam or field loss before "
        "accepting topology");
  PolygonEdit edit{PolygonOperation::Inset,
                   {PolygonSelectionMode::Face, {first}},
                   {},
                   .125f};
  size_t calls = 0;
  PolygonLimits measured;
  measured.cancelled = [&] {
    ++calls;
    return false;
  };
  auto result = edit_colored_polygon(cube, field, edit, measured);
  check(result.colors.has_value() && calls > 100,
        "Actual geometry and color interpolation expose cancellation "
        "checkpoints");
  for (size_t stop = 1; stop <= calls; ++stop) {
    size_t n = 0;
    PolygonLimits limit;
    limit.cancelled = [&] { return ++n == stop; };
    if (!refuses([&] { edit_colored_polygon(cube, field, edit, limit); }) ||
        cube.encode() != original || field.packed(cube) != values)
      throw std::runtime_error(
          "Cancellation changed original color/topology authority");
  }
  check(true, "Every measured cancellation boundary preserves exact source "
              "geometry, all RGBA samples and deterministic retry");
  PolygonLimits low;
  low.maximum_provenance = 1;
  check(refuses([&] { edit_colored_polygon(cube, field, edit, low); }) &&
            cube.encode() == original,
        "Contributor resource refusal preserves complete original source");
  check(field.packed(cube) == values &&
            edit_colored_polygon(cube, field, edit)
                    .colors->packed(*result.geometry.replacement) ==
                result.colors->packed(*result.geometry.replacement),
        "Successful retry is deterministic after all cancellation/resource "
        "refusals");
  std::cout << checks << " polygon RGBA checks / " << calls
            << " cancellation boundaries passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
