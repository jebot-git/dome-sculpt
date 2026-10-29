// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/retopo.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <numeric>
using namespace dome;
void require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
template <class Fn> void rejects(Fn fn, const char *message) {
  bool bad = false;
  try {
    fn();
  } catch (const std::exception &) {
    bad = true;
  }
  require(bad, message);
}
Mesh cube(float r) {
  Mesh m;
  m.positions = {{-r, -r, -r}, {r, -r, -r}, {r, r, -r}, {-r, r, -r},
                 {-r, -r, r},  {r, -r, r},  {r, r, r},  {-r, r, r}};
  for (auto p : m.positions) {
    m.normals.push_back(normalized(p));
    m.uv.push_back({p.y, p.z});
  }
  m.indices = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
               1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
  m.uv_pins.assign(8, 0);
  m.uv_pins[6] = 1;
  m.uv_seams.assign(m.indices.size(), 0);
  m.uv_seams[0] = 1;
  return m;
}
void join(Mesh &a, const Mesh &b) {
  auto base = uint32_t(a.positions.size());
  a.positions.insert(a.positions.end(), b.positions.begin(), b.positions.end());
  a.normals.insert(a.normals.end(), b.normals.begin(), b.normals.end());
  a.uv.insert(a.uv.end(), b.uv.begin(), b.uv.end());
  a.uv_pins.insert(a.uv_pins.end(), b.uv_pins.begin(), b.uv_pins.end());
  a.uv_seams.insert(a.uv_seams.end(), b.uv_seams.begin(), b.uv_seams.end());
  for (auto i : b.indices)
    a.indices.push_back(base + i);
}
int main() try {
  RetopoMesh patch;
  std::array<TopologyID, 9> ids;
  for (int y = 0; y < 3; y++)
    for (int x = 0; x < 3; x++)
      ids[y * 3 + x] = patch.add_vertex({{float(x - 1), float(y - 1), 0},
                                         {float(x) / 2, float(y) / 2},
                                         false});
  patch.move_vertex(ids[4], {.2f, .1f, .1f});
  std::vector<TopologyID> faces;
  for (int y = 0; y < 2; y++)
    for (int x = 0; x < 2; x++) {
      std::array face = {ids[y * 3 + x], ids[y * 3 + x + 1],
                         ids[(y + 1) * 3 + x + 1], ids[(y + 1) * 3 + x]};
      faces.push_back(patch.add_face(face, 7));
    }
  auto surface = patch.triangulate();
  require(
      surface.mesh.indices.size() == 24 && surface.mesh.positions.size() == 9 &&
          surface.source_faces.size() == 8 && surface.materials[0] == 7,
      "Editable quad patch triangulates with stable source IDs and materials");
  auto encoded = patch.encode();
  auto restored = RetopoMesh::decode(encoded);
  require(
      restored.encode() == encoded && restored.next_id() == patch.next_id(),
      "Native persistence preserves exact topology, attributes and ID cursor");
  auto snapshot = patch;
  auto before = patch.encode();
  std::array bad = {ids[0], ids[1], ids[4]};
  rejects([&] { patch.add_face(bad); },
          "Inconsistent shared-edge orientation rejects atomically");
  require(patch.encode() == before,
          "Failed face creation preserves IDs and all topology");
  rejects(
      [&] { patch.move_vertex(ids[0], patch.vertices().at(ids[1]).position); },
      "Collapsing an incident face rejects vertex motion");
  require(patch.encode() == before,
          "Failed vertex movement preserves the exact snapshot");
  auto edge_id = patch.edges().begin()->first;
  rejects([&] { patch.remove_edge(edge_id); },
          "Incident face edges cannot be removed accidentally");
  rejects([&] { patch.remove_vertex(ids[0]); },
          "Referenced vertices cannot be deleted");
  Mesh plane;
  plane.positions = {{-2, -2, 0}, {2, -2, 0}, {2, 2, 0}, {-2, 2, 0}};
  plane.normals.assign(4, {0, 0, 1});
  plane.indices = {0, 1, 2, 0, 2, 3};
  SurfaceQuery source(plane);
  rejects([&] { patch.project(std::span(&ids[4], 1), source, .01f); },
          "Surface projection enforces maximum snap distance");
  require(patch.encode() == before,
          "Failed projection preserves the exact patch");
  patch.project(std::span(&ids[4], 1), source, .2f);
  require(std::abs(patch.vertices().at(ids[4]).position.z) < 1e-7f,
          "Bounded source projection snaps a selected vertex");
  patch.relax(source, {.iterations = 1,
                       .strength = 1,
                       .maximum_snap_distance = .2f,
                       .cancelled = {}});
  require(length(patch.vertices().at(ids[4]).position) < 1e-6f &&
              patch.vertices().at(ids[0]).position.x == -1,
          "Jacobi relaxation averages the interior while preserving all "
          "boundaries");
  patch.move_vertex(ids[4], {.2f, .1f, 0});
  patch.set_pinned(ids[4], true);
  before = patch.encode();
  patch.relax(source, {.strength = 1, .cancelled = {}});
  require(patch.encode() == before,
          "Pinned retopology vertices survive relaxation exactly");
  rejects([&] { patch.relax(source, {.cancelled = [] { return true; }}); },
          "Relax cancellation publishes no partial mutation");
  require(patch.encode() == before,
          "Cancelled relaxation leaves the full value snapshot unchanged");
  patch.set_seam(edge_id, true);
  surface = patch.triangulate();
  require(std::find(surface.mesh.uv_seams.begin(), surface.mesh.uv_seams.end(),
                    1) != surface.mesh.uv_seams.end(),
          "Explicit topology seams propagate to triangle corner flags");
  require(snapshot.encode() == encoded,
          "Independent value snapshots remain unchanged after later modeling");
  RetopoMesh standalone;
  auto a = standalone.add_vertex({{0, 0, 0}}),
       b = standalone.add_vertex({{1, 0, 0}});
  auto wire = standalone.add_edge(a, b);
  auto cursor = standalone.next_id();
  standalone.remove_edge(wire);
  standalone.remove_vertex(b);
  require(standalone.add_vertex({{0, 1, 0}}) >= cursor,
          "Deleted topology IDs are never recycled");
  RetopoMesh concave;
  std::array<TopologyID, 4> quad;
  for (size_t i = 0; i < 4; i++)
    quad[i] = concave.add_vertex({std::array<Vec3, 4>{
        {{0, 0, 0}, {1, 0, 0}, {.2f, .2f, 0}, {0, 1, 0}}}[i]});
  concave.add_face(quad);
  require(concave.triangulate().mesh.indices.size() == 6,
          "Concave quad uses a valid interior diagonal");
  auto deleting = snapshot;
  deleting.remove_face(faces[0]);
  auto deleting_before = deleting.encode();
  rejects([&] { deleting.remove_face(faces[3]); },
          "Face deletion cannot split a vertex into disconnected face fans");
  require(deleting.encode() == deleting_before,
          "Rejected face deletion retains the complete topology snapshot");
  auto cancelling = snapshot;
  auto cancelling_before = cancelling.encode();
  int cancellation_calls = 0;
  std::array selection = {ids[4], ids[0]};
  rejects(
      [&] {
        cancelling.project(selection, source, .2f,
                           [&] { return ++cancellation_calls > 1; });
      },
      "Projection can cancel after modifying a draft vertex");
  require(cancelling.encode() == cancelling_before,
          "Mid-projection cancellation never publishes the modified draft");
  auto probe = snapshot;
  int relax_calls = 0;
  probe.relax(source,
              {.strength = 1, .maximum_snap_distance = .2f, .cancelled = [&] {
                 ++relax_calls;
                 return false;
               }});
  cancellation_calls = 0;
  rejects(
      [&] {
        cancelling.relax(
            source,
            {.strength = 1, .maximum_snap_distance = .2f, .cancelled = [&] {
               return ++cancellation_calls >= relax_calls;
             }});
      },
      "Relax cancellation is checked immediately before publication");
  require(cancelling.encode() == cancelling_before,
          "Late relax cancellation preserves all IDs and coordinates");
  auto invalid = nlohmann::json::parse(encoded);
  invalid["vertices"][1][0] = invalid["vertices"][0][0];
  rejects([&] { RetopoMesh::decode(invalid.dump()); },
          "Duplicate persisted IDs cannot replace topology silently");
  invalid = nlohmann::json::parse(encoded);
  invalid["next"] = 1;
  rejects([&] { RetopoMesh::decode(invalid.dump()); },
          "Persisted allocation cursor must exceed all live IDs");
  rejects(
      [&] {
        RetopoMesh::decode(std::string(100, '[') + std::string(100, ']'));
      },
      "Excessive JSON nesting rejects before recursive parse");
  RetopoMesh fan;
  std::array<TopologyID, 5> fv;
  for (size_t i = 0; i < 5; i++)
    fv[i] = fan.add_vertex({std::array<Vec3, 5>{
        {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {-1, 0, 0}, {0, -1, 0}}}[i]});
  std::array first = {fv[0], fv[1], fv[2]}, second = {fv[0], fv[3], fv[4]};
  fan.add_face(first);
  auto fan_before = fan.encode();
  rejects([&] { fan.add_face(second); },
          "Vertex-only disconnected face fans reject nonmanifold topology");
  require(fan.encode() == fan_before,
          "Rejected face fan preserves the allocation cursor and snapshot");
  rejects([&] { fan.move_vertex(fv[2], {0, -1, 0}); },
          "Vertex movement cannot invert an existing triangle");
  auto mesh = cube(.2f);
  PlaneCutOptions options;
  options.normal = {2, 0, 0};
  options.offset = .03f;
  options.cap_material = 9;
  std::vector<uint32_t> materials(12);
  std::iota(materials.begin(), materials.end(), 1);
  auto cut = plane_cut(mesh, options, materials);
  require(inspect_mesh(cut.mesh).closed_manifold() && cut.cap_triangles == 6,
          "Plane trimming preserves a closed cube with triangulated cap");
  bool metadata = true;
  double cap_area = 0;
  for (size_t t = 0; t < cut.source_triangles.size(); t++) {
    auto source_id = cut.source_triangles[t];
    if (source_id == UINT32_MAX) {
      metadata &= cut.materials[t] == 9;
      auto a = cut.mesh.positions[cut.mesh.indices[t * 3]],
           b = cut.mesh.positions[cut.mesh.indices[t * 3 + 1]],
           c = cut.mesh.positions[cut.mesh.indices[t * 3 + 2]];
      cap_area += length(cross(b - a, c - a)) * .5;
      metadata &= cut.mesh.normals[cut.mesh.indices[t * 3]].x < -.99f;
    } else
      metadata &= cut.materials[t] == materials[source_id];
  }
  require(metadata && std::abs(cap_area - .16) < 1e-6,
          "New caps carry requested material, planar area and outward normals");
  bool attrs = true;
  for (size_t i = 0; i < cut.mesh.positions.size(); i++) {
    auto p = cut.mesh.positions[i];
    attrs &= p.x >= .03f - 1e-6f;
    if (cut.mesh.normals[i].x > -.99f) {
      attrs &= std::abs(cut.mesh.uv[i][0] - p.y) < 1e-6f &&
               std::abs(cut.mesh.uv[i][1] - p.z) < 1e-6f;
    }
  }
  require(attrs,
          "Cut side vertices preserve interpolated original UV attributes");
  bool barycentric = true;
  for (size_t t = 0; t < cut.source_triangles.size(); t++)
    for (size_t k = 0; k < 3; k++) {
      auto bary = cut.source_corner_barycentrics[t * 3 + k];
      if (cut.source_triangles[t] == UINT32_MAX) {
        barycentric &= bary == std::array<float, 3>{};
        continue;
      }
      Vec3 reconstructed{};
      for (size_t j = 0; j < 3; j++)
        reconstructed +=
            mesh.positions[mesh.indices[cut.source_triangles[t] * 3 + j]] *
            bary[j];
      barycentric &=
          length(reconstructed -
                 cut.mesh.positions[cut.mesh.indices[t * 3 + k]]) < 1e-6f;
    }
  require(barycentric, "Cut corner barycentrics preserve provenance for "
                       "external vertex attributes");
  options.cap = false;
  auto open = plane_cut(mesh, options);
  require(inspect_mesh(open.mesh).boundary_edges == 8 &&
              open.cap_triangles == 0,
          "Uncapped trim exposes the expected cut boundary");
  options.cap = true;
  options.offset = -.3f;
  auto unchanged = plane_cut(mesh, options);
  require(unchanged.mesh.indices == mesh.indices &&
              unchanged.mesh.uv_pins == mesh.uv_pins &&
              unchanged.mesh.uv_seams == mesh.uv_seams,
          "Nonintersecting retained cut returns exact source mesh attributes");
  options.offset = .3f;
  rejects(
      [&] { plane_cut(mesh, options); },
      "Cutting away the entire source reports an explicit empty-result error");
  options.offset = .2f;
  rejects(
      [&] { plane_cut(mesh, options); },
      "Plane-through-vertex ambiguity rejects instead of generating cracks");
  options.offset = .03f;
  options.max_triangles = 2;
  rejects([&] { plane_cut(mesh, options); },
          "Plane cut output triangle budget is enforced");
  options.max_triangles = 2'000'000;
  options.max_contour_vertices = 3;
  rejects([&] { plane_cut(mesh, options); },
          "Plane cut contour resource budget is enforced");
  options.max_contour_vertices = 4096;
  options.cancelled = [] { return true; };
  rejects([&] { plane_cut(mesh, options); },
          "Plane cutting honors worker cancellation");
  options.cancelled = {};
  auto hole = mesh;
  auto inner = cube(.1f);
  for (size_t t = 0; t < inner.indices.size(); t += 3)
    std::swap(inner.indices[t], inner.indices[t + 1]);
  join(hole, inner);
  rejects([&] { plane_cut(hole, options); },
          "Cap contours with nested holes reject rather than sealing a cavity "
          "incorrectly");
  auto disconnected = mesh;
  auto other = mesh;
  for (auto &p : other.positions)
    p.y += 1;
  join(disconnected, other);
  require(inspect_mesh(plane_cut(disconnected, options).mesh).components == 2,
          "Disconnected solid contours receive independent closed caps");
  Mesh prism;
  std::array<Vec3, 6> ring = {{{0, 0, 0},
                               {.4f, 0, 0},
                               {.4f, .2f, 0},
                               {.2f, .2f, 0},
                               {.2f, .4f, 0},
                               {0, .4f, 0}}};
  for (float z : {-.2f, .2f})
    for (auto p : ring) {
      p.z = z;
      prism.positions.push_back(p);
      prism.normals.push_back({0, 0, z > 0 ? 1.f : -1.f});
    }
  for (auto tri : std::array<std::array<uint32_t, 3>, 4>{
           {{0, 1, 3}, {1, 2, 3}, {0, 3, 5}, {3, 4, 5}}}) {
    prism.indices.insert(
        prism.indices.end(),
        {tri[0], tri[2], tri[1], tri[0] + 6, tri[1] + 6, tri[2] + 6});
  }
  for (uint32_t i = 0; i < 6; i++) {
    auto j = (i + 1) % 6;
    prism.indices.insert(prism.indices.end(), {i, j, j + 6, i, j + 6, i + 6});
  }
  options.normal = {0, 0, 1};
  options.offset = .03f;
  auto concave_cut = plane_cut(prism, options);
  double concave_area = 0;
  for (size_t t = 0; t < concave_cut.source_triangles.size(); t++)
    if (concave_cut.source_triangles[t] == UINT32_MAX) {
      auto a = concave_cut.mesh.positions[concave_cut.mesh.indices[t * 3]],
           b = concave_cut.mesh.positions[concave_cut.mesh.indices[t * 3 + 1]],
           c = concave_cut.mesh.positions[concave_cut.mesh.indices[t * 3 + 2]];
      concave_area += length(cross(b - a, c - a)) * .5;
    }
  require(inspect_mesh(concave_cut.mesh).closed_manifold() &&
              std::abs(concave_area - .12) < 1e-6,
          "Concave cut caps preserve contour area and every collinear boundary "
          "vertex");
  std::cout << "Retopology and plane cut checks complete\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
