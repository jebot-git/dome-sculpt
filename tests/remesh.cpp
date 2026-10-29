// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/remesh.hpp"
#include <bit>
#include <iostream>
using namespace dome;
void require(bool ok, const char *s) {
  if (!ok)
    throw std::runtime_error(s);
  std::cout << "PASS " << s << '\n';
}
template <class F> void rejects(F f, const char *s) {
  bool bad = false;
  try {
    f();
  } catch (const std::exception &) {
    bad = true;
  }
  require(bad, s);
}
Mesh grid(int n, float spacing) {
  Mesh m;
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++) {
      m.positions.push_back(
          {(x - (n - 1) * .5f) * spacing, (y - (n - 1) * .5f) * spacing, 0});
      m.normals.push_back({0, 0, 1});
      m.uv.push_back({float(x) / (n - 1), float(y) / (n - 1)});
    }
  for (uint32_t y = 0; y < uint32_t(n - 1); y++)
    for (uint32_t x = 0; x < uint32_t(n - 1); x++) {
      auto a = y * n + x, b = a + 1, c = a + n, d = c + 1;
      m.indices.insert(m.indices.end(), {a, b, d, a, d, c});
    }
  return m;
}
Mesh cube(float r) {
  Mesh m;
  m.positions = {{-r, -r, -r}, {r, -r, -r}, {r, r, -r}, {-r, r, -r},
                 {-r, -r, r},  {r, -r, r},  {r, r, r},  {-r, r, r}};
  for (auto p : m.positions) {
    m.normals.push_back(normalized(p));
    m.uv.push_back({p.x, p.y});
  }
  m.indices = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
               1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
  return m;
}
void join(Mesh &a, const Mesh &b) {
  auto base = uint32_t(a.positions.size());
  a.positions.insert(a.positions.end(), b.positions.begin(), b.positions.end());
  a.normals.insert(a.normals.end(), b.normals.begin(), b.normals.end());
  a.uv.insert(a.uv.end(), b.uv.begin(), b.uv.end());
  for (auto i : b.indices)
    a.indices.push_back(base + i);
}
uint64_t fingerprint(const Mesh &m) {
  uint64_t h = 1469598103934665603ULL;
  auto mix = [&](uint32_t x) {
    h ^= x;
    h *= 1099511628211ULL;
  };
  for (auto p : m.positions) {
    mix(std::bit_cast<uint32_t>(p.x));
    mix(std::bit_cast<uint32_t>(p.y));
    mix(std::bit_cast<uint32_t>(p.z));
  }
  for (auto n : m.normals) {
    mix(std::bit_cast<uint32_t>(n.x));
    mix(std::bit_cast<uint32_t>(n.y));
    mix(std::bit_cast<uint32_t>(n.z));
  }
  for (auto uv : m.uv)
    for (auto x : uv)
      mix(std::bit_cast<uint32_t>(x));
  for (auto i : m.indices)
    mix(i);
  for (auto x : m.uv_seams)
    mix(x);
  for (auto x : m.uv_pins)
    mix(x);
  return h;
}
int main() try {
  auto plane = grid(2, 1);
  auto original = fingerprint(plane);
  RemeshOptions o;
  o.target_length = .2f;
  o.iterations = 5;
  o.collapse = false;
  o.flip = false;
  o.relax = false;
  auto refined = remesh_triangles(plane, o);
  require(refined.splits > 0 &&
              refined.mesh.indices.size() > plane.indices.size(),
          "Long triangle edges split toward the isotropic target length");
  require(fingerprint(plane) == original,
          "Remeshing owns a draft and leaves the immutable source unchanged");
  bool boundary = true, provenance = true;
  for (auto p : refined.mesh.positions)
    boundary &= std::abs(p.z) < 1e-7f && std::abs(p.x) <= .500001f &&
                std::abs(p.y) <= .500001f;
  for (size_t c = 0; c < refined.mesh.indices.size(); c++) {
    auto source = refined.source_corners[c];
    Vec3 p{};
    std::array<float, 2> uv{};
    for (size_t k = 0; k < 3; k++) {
      auto i = plane.indices[source.triangle * 3 + k];
      p += plane.positions[i] * source.barycentric[k];
      for (size_t j = 0; j < 2; j++)
        uv[j] += plane.uv[i][j] * source.barycentric[k];
    }
    auto i = refined.mesh.indices[c];
    provenance &= length(p - refined.mesh.positions[i]) < 1e-6f &&
                  std::abs(uv[0] - refined.mesh.uv[i][0]) < 1e-6f &&
                  std::abs(uv[1] - refined.mesh.uv[i][1]) < 1e-6f;
  }
  require(boundary,
          "Refinement retains the original open boundary and source plane");
  require(provenance &&
              refined.source_corners.size() == refined.mesh.indices.size(),
          "Every remeshed corner carries reconstructable source triangle "
          "barycentrics");
  auto dense = grid(5, .5f);
  dense.positions[13].x = .03f;
  o = {};
  o.target_length = .5f;
  o.iterations = 1;
  o.split = false;
  o.flip = false;
  o.relax = false;
  auto collapsed = remesh_triangles(dense, o);
  require(collapsed.collapses > 0 &&
              collapsed.mesh.indices.size() < dense.indices.size(),
          "Short interior edges collapse with manifold link and fold checks");
  require(inspect_mesh(collapsed.mesh).components == 1 &&
              !inspect_mesh(collapsed.mesh).nonmanifold_edges,
          "Interior collapse preserves the connected manifold surface");
  auto regular = grid(5, .5f);
  // Deliberately swap one interior diagonal, creating valences5,5,7,7.
  std::array<uint32_t, 6> poor_diagonal = {7, 11, 6, 11, 7, 12};
  std::copy(poor_diagonal.begin(), poor_diagonal.end(),
            regular.indices.begin() + 30);
  o.collapse = false;
  o.flip = true;
  auto swapped = remesh_triangles(regular, o);
  require(swapped.flips > 0,
          "Legal diagonal swaps improve local target valence");
  regular.positions[12].x = .1f;
  regular.positions[12].y = .15f;
  o.flip = false;
  o.relax = true;
  o.relax_strength = .5f;
  auto relaxed = remesh_triangles(regular, o);
  require(relaxed.relaxed_vertices > 0, "Interior relaxation projects accepted "
                                        "positions onto the original source");
  bool corners = true;
  for (auto corner : {0, 4, 20, 24}) {
    auto p = regular.positions[corner];
    corners &= std::any_of(relaxed.mesh.positions.begin(),
                           relaxed.mesh.positions.end(),
                           [&](Vec3 q) { return length(p - q) < 1e-8f; });
  }
  require(corners,
          "Boundary corner vertices remain exactly fixed during relaxation");
  regular.uv_pins.assign(25, 0);
  regular.uv_pins[12] = 1;
  auto pinned = regular.positions[12];
  auto pinned_result = remesh_triangles(regular, o);
  bool pin = false;
  for (size_t i = 0; i < pinned_result.mesh.positions.size(); i++)
    if (pinned_result.mesh.uv_pins[i])
      pin |= length(pinned_result.mesh.positions[i] - pinned) < 1e-8f;
  require(pin, "Pinned source vertices retain exact positions and pin flags");
  auto solid = cube(.2f);
  o = {};
  o.target_length = .12f;
  o.iterations = 4;
  o.maximum_projection_distance = .05f;
  auto remeshed_solid = remesh_triangles(solid, o);
  require(inspect_mesh(remeshed_solid.mesh).closed_manifold(),
          "Sharp-feature cube remeshing preserves a closed solid");
  bool bounds = true;
  for (auto p : remeshed_solid.mesh.positions)
    bounds &= std::abs(std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) -
                       .2f) < 1e-5f;
  require(
      bounds,
      "Protected crease vertices stay on the original cube faces and edges");
  Mesh seams;
  for (size_t t = 0; t < solid.indices.size() / 3; t++)
    for (size_t k = 0; k < 3; k++) {
      auto i = solid.indices[t * 3 + k];
      seams.positions.push_back(solid.positions[i]);
      seams.normals.push_back(solid.normals[i]);
      seams.uv.push_back({float(t) + float(k) * .1f, float(k) * .2f});
      seams.indices.push_back(uint32_t(seams.indices.size()));
      seams.uv_seams.push_back(1);
    }
  std::vector<uint32_t> materials(12);
  for (uint32_t t = 0; t < 12; t++)
    materials[t] = t;
  auto seamed = remesh_triangles(seams, o, materials);
  bool charts = true;
  for (size_t c = 0; c < seamed.source_corners.size(); c++) {
    auto src = seamed.source_corners[c];
    charts &= materials[src.triangle] == seamed.materials[c / 3];
    std::array<float, 2> uv{};
    for (size_t k = 0; k < 3; k++)
      for (size_t j = 0; j < 2; j++)
        uv[j] += seams.uv[seams.indices[src.triangle * 3 + k]][j] *
                 src.barycentric[k];
    auto i = seamed.mesh.indices[c];
    charts &= std::abs(uv[0] - seamed.mesh.uv[i][0]) < 1e-5f &&
              std::abs(uv[1] - seamed.mesh.uv[i][1]) < 1e-5f;
  }
  require(
      inspect_mesh(seamed.mesh).closed_manifold(),
      "Duplicated seam boundaries split in lockstep without geometric cracks");
  require(charts, "Source provenance remains inside each disconnected UV and "
                  "material chart");
  auto thin = grid(5, .2f);
  thin.positions[12].z = .002f;
  auto upper = thin;
  for (auto &p : upper.positions)
    p.z += .0005f;
  size_t first_faces = thin.indices.size() / 3;
  join(thin, upper);
  std::vector<uint32_t> layers(thin.indices.size() / 3, 0);
  std::fill(layers.begin() + first_faces, layers.end(), 1);
  o = {};
  o.split = false;
  o.collapse = false;
  o.flip = false;
  o.iterations = 2;
  o.feature_angle_degrees = 120;
  auto sheets = remesh_triangles(thin, o, layers);
  bool layer_provenance = true;
  for (size_t c = 0; c < sheets.source_corners.size(); c++)
    layer_provenance &=
        layers[sheets.source_corners[c].triangle] == sheets.materials[c / 3];
  require(inspect_mesh(sheets.mesh).components == 2 && layer_provenance,
          "Thin parallel disconnected sheets keep their own source projection "
          "provenance");
  auto adaptive_source = grid(3, .5f);
  adaptive_source.positions[4].z = .2f;
  o = {};
  o.iterations = 2;
  o.collapse = false;
  o.flip = false;
  o.relax = false;
  o.target_length = .7f;
  o.feature_angle_degrees = 150;
  o.maximum_projection_distance = .3f;
  auto uniform = remesh_triangles(adaptive_source, o);
  o.adaptivity = 8;
  auto adaptive = remesh_triangles(adaptive_source, o);
  require(
      adaptive.splits > uniform.splits,
      "Curvature adaptation refines bent regions beyond the uniform target");
  o.adaptivity = 0;
  std::vector<float> targets(adaptive_source.positions.size(), .1f);
  auto field = remesh_triangles(adaptive_source, o, {}, targets);
  require(
      field.splits > uniform.splits,
      "Explicit per-source vertex target lengths drive adaptive refinement");
  auto strip = grid(3, .01f);
  for (auto &p : strip.positions)
    p.y *= .001f;
  o = {};
  o.target_length = .003f;
  o.iterations = 3;
  o.collapse = false;
  o.flip = false;
  o.relax = false;
  auto narrow = remesh_triangles(strip, o);
  require(narrow.splits > 0 && inspect_mesh(narrow.mesh).components == 1,
          "Extreme-aspect thin triangle strips refine without degenerating "
          "topology");
  Mesh tetra;
  tetra.positions = {{{.1f, .1f, .1f},
                      {-.1f, -.1f, .1f},
                      {-.1f, .1f, -.1f},
                      {.1f, -.1f, -.1f}}};
  for (auto p : tetra.positions)
    tetra.normals.push_back(normalized(p));
  tetra.indices = {0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3};
  o = {};
  o.target_length = 1;
  o.iterations = 3;
  o.feature_angle_degrees = 180;
  o.split = false;
  o.flip = false;
  o.relax = false;
  auto minimal = remesh_triangles(tetra, o);
  require(minimal.collapses == 0 &&
              inspect_mesh(minimal.mesh).closed_manifold() &&
              minimal.mesh.indices.size() == 12,
          "Collapse rejects tetrahedron extinction and duplicate-face "
          "singularities");
  auto invalid = solid;
  invalid.indices.insert(invalid.indices.end(), {0, 2, 1});
  rejects([&] { remesh_triangles(invalid); },
          "Nonmanifold duplicate faces reject before remeshing");
  invalid = solid;
  std::swap(invalid.indices[0], invalid.indices[1]);
  rejects(
      [&] { remesh_triangles(invalid); },
      "Inconsistent winding rejects instead of propagating inverted topology");
  auto fan = grid(2, 1);
  fan.positions.push_back({-1, -1, 0});
  fan.positions.push_back({-2, -1, 0});
  fan.normals.insert(fan.normals.end(), 2, {0, 0, 1});
  fan.uv.insert(fan.uv.end(), 2, {0, 0});
  fan.indices.insert(fan.indices.end(), {0, 4, 5});
  rejects([&] { remesh_triangles(fan); },
          "Vertex-only disconnected face fans reject as nonmanifold");
  o = {};
  o.target_length = .1f;
  o.max_vertices = 4;
  rejects([&] { remesh_triangles(plane, o); },
          "Refinement resource limits fail before partial publication");
  o.max_vertices = 200'000;
  o.max_operations = 1;
  rejects([&] { remesh_triangles(plane, o); },
          "Remesh operation count bounds iterative work");
  o.max_operations = 100'000;
  o.max_edge_visits = 1;
  rejects([&] { remesh_triangles(plane, o); },
          "Adjacency and candidate traversal enforce edge work budgets");
  o.max_edge_visits = 20'000'000;
  o.max_projection_triangle_tests = 1;
  rejects([&] { remesh_triangles(plane, o); },
          "Source filtering enforces a cumulative projection work budget");
  o.max_projection_triangle_tests = 20'000'000;
  int calls = 0;
  o.cancelled = [&] { return ++calls > 20; };
  rejects([&] { remesh_triangles(plane, o); },
          "Remeshing can cancel after native draft work has begun");
  require(
      fingerprint(plane) == original,
      "Failed or cancelled remeshing leaves every source attribute unchanged");
  RemeshOptions late;
  late.iterations = 0;
  int total_calls = 0;
  late.cancelled = [&] {
    ++total_calls;
    return false;
  };
  remesh_triangles(plane, late);
  calls = 0;
  late.cancelled = [&] { return ++calls >= total_calls; };
  rejects(
      [&] { remesh_triangles(plane, late); },
      "Remeshing checks cancellation immediately before publishing its result");
  std::cout << "Native triangle remeshing checks complete\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
