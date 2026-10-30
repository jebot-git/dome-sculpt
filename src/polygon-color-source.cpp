// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-color-source.hpp"
#include "polygon-source-metadata.hpp"
#include "source-colors.hpp"
#include <map>
#include <set>
namespace dome {
PolygonSourceColors prepare_polygon_source_colors(
    const PaintedMesh &paint,
    std::shared_ptr<const CreatorAppearance> appearance,
    const PolygonLimits &limits) {
  size_t work = 0;
  auto check = [&] {
    if (work == limits.maximum_work)
      throw std::length_error("Polygon native color work budget exceeded");
    ++work;
    if (limits.cancelled && limits.cancelled())
      throw std::runtime_error("Polygon native color capture cancelled");
  };
  check();
  if (paint.in_stroke() || paint.color_projection())
    throw std::logic_error("Finish paint and retain an unprepared source "
                           "before polygon color capture");
  if (!appearance || !appearance->asset)
    throw std::invalid_argument(
        "Polygon color capture requires its actual original source accessor");
  const auto &rest = paint.mesh();
  auto colors = detail::source_vertex_colors(appearance.get(),
                                             rest.positions.size(), [&] {
                                               check();
                                               return false;
                                             });
  if (colors.empty())
    throw std::invalid_argument(
        "Polygon source has no authored COLOR_0 accessor");
  auto prepared = prepare_polygon_topology(rest, appearance.get(), limits);
  auto rendered = prepared.topology.triangulate(limits);
  if (prepared.source_vertices.size() != rendered.corners.size() ||
      rendered.mesh.positions.size() != rendered.corners.size())
    throw std::invalid_argument(
        "Polygon color source corner mapping is incomplete");
  const ImportedPrimitive *primitive = nullptr;
  for (const auto &p : appearance->asset->primitives) {
    check();
    if (p.source_mesh == appearance->source_mesh &&
        p.source_primitive == appearance->source_primitive) {
      if (primitive)
        throw std::invalid_argument("Ambiguous polygon color raw accessor");
      primitive = &p;
    }
  }
  if (!primitive)
    throw std::invalid_argument("Missing polygon raw color primitive");
  std::vector<uint64_t> basis(appearance->source_vertices.begin(),
                              appearance->source_vertices.end());
  if (prepared.authored_ngons) {
    auto metadata = polygon_source_detail::parse(
        primitive->extras_json, primitive->mesh.positions.size(), limits,
        check);
    if (metadata.corners.size() != primitive->mesh.positions.size())
      throw std::invalid_argument("Incomplete polygon original corner witness");
    for (size_t i = 0; i < basis.size(); ++i) {
      check();
      basis[i] = metadata.corners.at(appearance->source_vertices[i]);
    }
  }
  std::map<uint64_t, PolygonRgba> logical_colors;
  for (size_t i = 0; i < basis.size(); ++i) {
    check();
    auto [entry, inserted] = logical_colors.emplace(basis[i], colors[i]);
    if (!inserted && entry->second != colors[i])
      throw std::invalid_argument("Original repeated polygon corner carries "
                                  "incompatible COLOR_0 values");
  }
  using Triangle = std::array<uint64_t, 3>;
  std::multiset<Triangle> original_triangles, derived_triangles;
  for (size_t i = 0; i < rest.indices.size(); i += 3) {
    check();
    Triangle t;
    for (size_t k = 0; k < 3; ++k)
      t[k] = basis.at(rest.indices[i + k]);
    std::sort(t.begin(), t.end());
    original_triangles.insert(t);
  }
  for (size_t i = 0; i < rendered.mesh.indices.size(); i += 3) {
    check();
    Triangle t;
    for (size_t k = 0; k < 3; ++k)
      t[k] =
          basis.at(prepared.source_vertices.at(rendered.mesh.indices[i + k]));
    std::sort(t.begin(), t.end());
    derived_triangles.insert(t);
  }
  const bool constant =
      std::all_of(colors.begin(), colors.end(),
                  [&](auto value) { return value == colors.front(); });
  if (original_triangles != derived_triangles && !constant)
    throw std::logic_error(
        "Polygon triangulation changes this source's piecewise COLOR_0 field; "
        "preserve its original triangle basis before conversion");
  std::vector<PolygonCornerRgba> corner_colors;
  std::vector<PolygonNativeColorCorner> witnesses;
  for (size_t i = 0; i < rendered.corners.size(); ++i) {
    check();
    auto native = prepared.source_vertices[i];
    if (native >= rest.positions.size())
      throw std::invalid_argument(
          "Polygon color native witness is outside its source");
    auto a = rendered.mesh.positions[i], b = rest.positions[native];
    if (a.x != b.x || a.y != b.y || a.z != b.z)
      throw std::invalid_argument(
          "Polygon color geometry differs from its original native corner");
    corner_colors.push_back({rendered.corners[i], colors[native]});
    witnesses.push_back(
        {rendered.corners[i], native, appearance->source_vertices[native]});
  }
  auto field =
      PolygonColorField::from_corners(prepared.topology, corner_colors, limits);
  auto source_paint = paint.snapshot();
  auto source_appearance =
      std::make_shared<const CreatorAppearance>(*appearance);
  check();
  return {std::move(prepared), std::move(field), std::move(source_paint),
          std::move(source_appearance), std::move(witnesses)};
}
} // namespace dome
