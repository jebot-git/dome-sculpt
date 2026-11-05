// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh-processing.hpp"
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
Mesh cube(float r) {
  Mesh m;
  m.positions = {{-r, -r, -r}, {r, -r, -r}, {r, r, -r}, {-r, r, -r},
                 {-r, -r, r},  {r, -r, r},  {r, r, r},  {-r, r, r}};
  for (auto p : m.positions)
    m.normals.push_back(normalized(p));
  m.indices = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
               1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
  return m;
}
int main() try {
  auto mesh = cube(.2f);
  auto report = inspect_mesh(mesh);
  require(report.closed_manifold() && report.components == 1 &&
              report.geometric_vertices == 8 && report.triangles == 12,
          "Closed cube topology is manifold with exact geometric adjacency");
  auto flat = mesh;
  flat.indices.resize(flat.indices.size() - 3);
  report = inspect_mesh(flat);
  require(report.boundary_edges == 3 && !report.closed_manifold(),
          "Missing triangle exposes three boundary edges");
  rejects([&] { mesh_to_volume(flat); },
          "Open surfaces reject signed voxelization before publication");
  auto reverse = mesh;
  std::swap(reverse.indices[0], reverse.indices[1]);
  report = inspect_mesh(reverse);
  require(report.inconsistent_edges == 3,
          "Inconsistent triangle orientation is reported");
  auto duplicate = mesh;
  duplicate.indices.insert(duplicate.indices.end(), {0, 2, 1});
  report = inspect_mesh(duplicate);
  require(report.duplicate_triangles == 1 && report.nonmanifold_edges == 3,
          "Duplicate/nonmanifold faces are counted");
  auto clean = cleanup_mesh(duplicate);
  require(
      clean.removed_duplicates == 1 &&
          inspect_mesh(clean.mesh).closed_manifold(),
      "Explicit cleanup removes duplicate faces without changing the solid");
  auto degenerate = mesh;
  degenerate.indices.insert(degenerate.indices.end(), {0, 0, 1});
  require(
      inspect_mesh(degenerate).degenerate_triangles == 1,
      "Degenerate triangle detection is independent of structural validation");
  require(cleanup_mesh(degenerate).removed_degenerate == 1,
          "Cleanup removes degenerate triangles explicitly");
  Mesh seams;
  seams.positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0},
                     {0, 0, 0}, {1, 1, 0}, {0, 1, 0}};
  seams.normals.assign(6, {0, 0, 1});
  seams.uv = {{{0, 0}}, {{.4, 0}}, {{.4, .4}}, {{.6, .6}}, {{1, 1}}, {{.6, 1}}};
  seams.indices = {0, 1, 2, 3, 4, 5};
  seams.uv_pins = {1, 0, 0, 0, 0, 0};
  seams.uv_seams = {0, 0, 1, 1, 0, 0};
  auto seamclean = cleanup_mesh(seams);
  require(seamclean.mesh.positions.size() == 6 &&
              seamclean.mesh.uv == seams.uv &&
              seamclean.mesh.uv_pins == seams.uv_pins &&
              seamclean.mesh.uv_seams == seams.uv_seams,
          "Cleanup preserves hard UV seams, explicit cuts and pinned vertices");
  require(seamclean.source_vertices ==
                  std::vector<uint32_t>({0, 1, 2, 3, 4, 5}) &&
              seamclean.source_triangles == std::vector<uint32_t>({0, 1}),
          "Cleanup exposes source mappings for all external attributes");
  auto overlap = seams;
  overlap.positions[5] = overlap.positions[1];
  auto overlapclean = cleanup_mesh(overlap);
  require(overlapclean.removed_duplicates == 0 &&
              overlapclean.mesh.indices.size() == 6,
          "Coincident faces with distinct corner attributes are retained");
  report = inspect_mesh(seams);
  require(report.geometric_vertices == 4 && report.boundary_edges == 4 &&
              report.components == 1,
          "Topology connects geometric edges across distinct UV vertices");
  SurfaceQuery query(mesh);
  auto closest = query.closest({0, 0, .4f});
  require(
      std::abs(closest.distance - .2f) < 1e-6f &&
          std::abs(closest.position.z - .2f) < 1e-6f &&
          std::abs(closest.barycentric[0] + closest.barycentric[1] +
                   closest.barycentric[2] - 1) < 1e-6f,
      "Reusable BVH closest queries expose triangle identity and barycentrics");
  bool thin_interior=true,thin_edges=true,thin_filters=true,thin_ranges=true;
  for(float scale:{std::ldexp(1.f,-20),1.f,32.f})
    for(float ratio:{std::ldexp(1.f,-10),std::ldexp(1.f,-30),std::ldexp(1.f,-60)})
      for(int axis=0;axis<3;++axis)for(bool flipped:{false,true}) {
        auto rotate=[axis](Vec3 p) {return axis==0?p:axis==1?Vec3{p.z,p.x,p.y}:Vec3{p.y,p.z,p.x};};
        const float h=scale*ratio,d=scale*.125f;
        Mesh thin;thin.positions={rotate({0,0,0}),rotate({scale,0,0}),rotate({scale,h,0})};
        thin.normals.assign(3,rotate({0,0,1}));thin.indices=flipped?std::vector<uint32_t>{0,2,1}:std::vector<uint32_t>{0,1,2};
        SurfaceQuery surface(thin);
        const auto interior=surface.closest(rotate({scale*.75f,h*.25f,d}));
        const std::array<float,3> expected=flipped?std::array<float,3>{.25f,.25f,.5f}:std::array<float,3>{.25f,.5f,.25f};
        const auto point=rotate({scale*.75f,h*.25f,0});
        thin_interior &= interior.barycentric==expected&&interior.position.x==point.x&&interior.position.y==point.y&&interior.position.z==point.z&&interior.distance==d;
        const auto edge=surface.closest(rotate({scale*.5f,-h,d}));
        const std::array<float,3> edge_bary=flipped?std::array<float,3>{.5f,0,.5f}:std::array<float,3>{.5f,.5f,0};
        const auto vertex=surface.closest(rotate({-scale*.5f,-h,d}));
        thin_edges &= edge.barycentric==edge_bary&&vertex.barycentric==std::array<float,3>{1,0,0};
        bool observed=false;SurfaceQueryOptions options;
        options.closest_filter=[&](uint32_t t,const auto &bary){observed=true;return t==0&&bary==expected;};
        thin_filters &= surface.nearest(rotate({scale*.75f,h*.25f,d}),options).has_value()&&observed;
        options.closest_filter={};options.closest_maximum=d;
        thin_ranges &= surface.nearest(rotate({scale*.75f,h*.25f,d}),options).has_value();
        options.closest_maximum=d*.5f;
        thin_ranges &= !surface.nearest(rotate({scale*.75f,h*.25f,d}),options);
      }
  require(thin_interior,"Thin valid triangles preserve interior barycentrics across scales, axes and winding");
  require(thin_edges,"Thin triangle exterior queries retain the correct edge and vertex regions");
  require(thin_filters,"Closest filters receive actual thin-triangle barycentrics without rounded-point reconstruction");
  require(thin_ranges,"Thin triangle nearest queries obey exact inclusive physical distance bounds");
  Mesh tiny_range;tiny_range.positions={{0,0,0},{1,0,0},{0,1,0}};
  tiny_range.normals.assign(3,{0,0,1});tiny_range.indices={0,1,2};
  SurfaceQuery tiny_surface(tiny_range);SurfaceQueryOptions tiny_options;
  const float tiny_distance=std::ldexp(1.f,-80);tiny_options.closest_maximum=tiny_distance;
  auto tiny_hit=tiny_surface.nearest({.25f,.25f,tiny_distance},tiny_options);
  require(tiny_hit&&tiny_hit->distance==tiny_distance,"Nearest radius squares retain sub-float-range physical distances");
  tiny_options.closest_maximum=tiny_distance*.5f;
  require(!tiny_surface.nearest({.25f,.25f,tiny_distance},tiny_options),"Sub-float-range radius cannot admit a more distant surface");
  auto filtered = query.closest(
      {0, 0, .4f},
      {.triangle_filter = [](uint32_t t) { return t < 2; }, .cancelled = {}});
  require(std::abs(filtered.distance - .6f) < 1e-6f && filtered.triangle < 2,
          "Filtered BVH closest queries stay on the selected source chart");
  rejects(
      [&] {
        query.closest({0, 0, .4f},
                      {.triangle_filter = [](uint32_t) { return false; },
                       .cancelled = {}});
      },
      "Empty source filters reject instead of returning an unrelated triangle");
  rejects(
      [&] {
        query.closest({0, 0, .4f},
                      {.triangle_filter = [](uint32_t) { return false; },
                       .cancelled = {},
                       .max_triangle_tests = 1});
      },
      "Filtered source traversal enforces triangle work limits");
  rejects(
      [&] {
        query.closest({0, 0, .4f}, {.triangle_filter = {},
                                    .cancelled = [] { return true; }});
      },
      "Filtered source queries honor cancellation");
  auto hit = query.raycast({0, 0, .4f}, {0, 0, -2}, .3f);
  require(hit && std::abs(hit->distance - .2f) < 1e-6f &&
              hit->position.z > .19f,
          "Surface rays normalize direction and report physical distance");
  require(!query.raycast({0, 0, .4f}, {0, 0, -1}, .1f),
          "Surface ray distance limits prevent remote hits");
  bool edges_hit=true, outside_rejected=true;
  for(float scale:{.001f,1.f,100.f}) {
    Mesh plane;plane.positions={{-scale,-scale,0},{scale,-scale,0},{scale,scale,0},{-scale,scale,0}};
    plane.normals.assign(4,{0,0,1});plane.indices={0,1,2,0,2,3};
    SurfaceQuery surface(plane);
    for(int k=0;k<101;++k) {
      float v=(-.95f+1.9f*k/100)*scale;
      auto direction=normalized(Vec3{.1f+.003f*k,.6f-.004f*k,-1});
      auto point=Vec3{v,v,0};auto origin=point-direction*(.1234567f*scale);
      auto hit=surface.raycast(origin,direction,.13f*scale);
      edges_hit &= hit && length(hit->position-point)<scale*2e-5f;
      auto outside=Vec3{scale*1.0001f,scale*.3f,0};
      outside_rejected &= !surface.raycast(outside-direction*(.1234567f*scale),direction,.13f*scale);
    }
  }
  require(edges_hit,"Oblique projections onto shared triangle edges remain watertight across metric scales");
  require(outside_rejected,"Watertight projection does not expand geometry across real outside gaps");
  auto preserved = query.closest({0, 0, .4f}).distance;
  mesh.positions[0].x = -.3f;
  require(query.closest({0, 0, .4f}).distance == preserved,
          "BVH queries own an immutable source snapshot");
  mesh = cube(.2f);
  VoxelizeOptions options;
  options.voxel_size = .05f;
  auto field = mesh_to_volume(mesh, options);
  require(std::abs(field.at({0, 0, 0}) + .2f) < 1e-5f &&
              std::abs(field.at({3, 0, 0}) + .05f) < 1e-5f &&
              std::abs(field.at({5, 0, 0}) - .05f) < 1e-5f,
          "BVH distance and ray parity match analytic cube signed distances");
  require(std::abs(field.at({5, 5, 5}) - std::sqrt(3.f) * .05f) < 1e-5f,
          "Closest-triangle distance handles corner regions");
  auto surface = surface_mesh(field);
  surface.validate();
  require(!surface.indices.empty(), "Imported triangle solid remeshes through "
                                    "the native scalar-field extractor");
  auto nested = mesh;
  auto inner = cube(.1f);
  uint32_t base = nested.positions.size();
  nested.positions.insert(nested.positions.end(), inner.positions.begin(),
                          inner.positions.end());
  nested.normals.insert(nested.normals.end(), inner.normals.begin(),
                        inner.normals.end());
  for (size_t i = 0; i < inner.indices.size(); i += 3)
    nested.indices.insert(nested.indices.end(),
                          {base + inner.indices[i], base + inner.indices[i + 2],
                           base + inner.indices[i + 1]});
  auto hollow = mesh_to_volume(nested, options);
  require(
      hollow.at({0, 0, 0}) > 0 && hollow.at({3, 0, 0}) < 0,
      "Odd-even fill retains nested shell cavities and disconnected topology");
  options.max_grid_points = 100;
  rejects([&] { mesh_to_volume(mesh, options); },
          "Voxelization budget rejects oversized grids before evaluation");
  options.max_grid_points = 4'000'000;
  int calls = 0;
  options.cancelled = [&] { return ++calls > 10; };
  rejects([&] { mesh_to_volume(mesh, options); },
          "Voxelization cancellation interrupts the worker without a partial "
          "result");
  rejects([&] { cleanup_mesh(mesh, {.cancelled = [] { return true; }}); },
          "Cleanup cancellation is honored");
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
