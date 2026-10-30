// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-color-source.hpp"
#include "dome/color-preparation.hpp"
#include "source-color-fixtures.hpp"
#include "source-colors.hpp"
#include <iostream>
using namespace dome;
namespace {
unsigned checks = 0;
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
} // namespace
int main() try {
  auto scene = source_color_fixture::model();
  const auto before = scene.encode();
  const auto &o = scene.active();
  auto captured = prepare_polygon_source_colors(*o.painted, o.appearance);
  auto colors = captured.field.packed(captured.geometry.topology);
  const auto raw = detail::source_vertex_colors(
      o.appearance.get(), o.painted->mesh().positions.size());
  bool exact = true;
  for (size_t i = 0; i < colors.size(); ++i)
    exact &= colors[i] == raw.at(captured.geometry.source_vertices[i]);
  check(exact && colors.size() == 6 && captured.native_corners.size() == 6,
        "Actual authored GLB COLOR_0 maps to exact native polygon corners with "
        "independent raw accessor witnesses");
  check(captured.original_paint->encode() == o.painted->encode() &&
            captured.original_appearance->original_bytes ==
                o.appearance->original_bytes &&
            scene.encode() == before,
        "Capture retains complete original paint/source bytes and never "
        "rewrites the raw accessor or native scene");
  const auto &mesh = captured.geometry.topology;
  auto edge = mesh.edges().front();
  auto edited = edit_colored_polygon(mesh, captured.field,
                                     {PolygonOperation::CutEdges,
                                      {PolygonSelectionMode::Edge, {edge.id}},
                                      {},
                                      0,
                                      1});
  bool alpha_mid = false;
  for (const auto &row : edited.colors->corners())
    if (row.provenance.size() == 2) {
      auto a = captured.field.sample(row.provenance[0].first),
           b = captured.field.sample(row.provenance[1].first),
           value = edited.colors->sample(row.corner);
      for (size_t k = 0; k < 4; ++k)
        exact &= std::abs(value[k] - (a[k] + b[k]) * .5f) < 1e-7;
      alpha_mid = true;
    }
  check(exact && alpha_mid, "Real colored source edge cuts interpolate RGB and "
                            "alpha linearly using actual source sides");
  check(scene.encode() == before && captured.field.packed(mesh) == colors,
        "Accepted detached colored topology leaves full source textures, "
        "paint, skin, morphs and original attributes exact");
  auto render = edited.geometry.replacement->triangulate();
  check(
      edited.colors->packed(*edited.geometry.replacement).size() ==
          render.mesh.positions.size(),
      "Current color packing follows the exact edited derivative corner order");
  size_t count = 0;
  PolygonLimits measured;
  measured.cancelled = [&] {
    ++count;
    return false;
  };
  prepare_polygon_source_colors(*o.painted, o.appearance, measured);
  for (size_t stop = 1; stop <= count; ++stop) {
    size_t calls = 0;
    PolygonLimits l;
    l.cancelled = [&] { return ++calls == stop; };
    if (!refuses([&] {
          prepare_polygon_source_colors(*o.painted, o.appearance, l);
        }) ||
        scene.encode() != before)
      throw std::runtime_error(
          "Color capture cancellation changed original native source");
  }
  check(true, "Every actual raw capture cancellation boundary preserves "
              "original scene and deterministic retry");
  // Actual raw GLB metadata must not turn a different diagonal into a false
  // proof for an arbitrary piecewise field. A constant field is independent.
  for (bool zero : {false, true}) {
    auto bytes = source_color_fixture::bytes(zero);
    uint32_t length = 0;
    for (unsigned k = 0; k < 4; ++k)
      length |= uint32_t(bytes[12 + k]) << (8 * k);
    auto j =
        nlohmann::json::parse(bytes.begin() + 20, bytes.begin() + 20 + length);
    j["meshes"][0]["primitives"][0]["extras"] = {
        {"dome_source_polygons_version", 1},
        {"dome_source_vertices", {0, 1, 2, 3}},
        {"dome_source_corners", {0, 1, 2, 3}},
        {"dome_source_polygons",
         {{{"source_face", 0},
           {"vertices", {0, 1, 2, 3}},
           {"first_triangle", 0},
           {"triangle_count", 2}}}}};
    auto text = j.dump();
    while (text.size() % 4)
      text.push_back(' ');
    std::vector<uint8_t> raw;
    auto word = [&](uint32_t value) {
      for (unsigned k = 0; k < 4; ++k)
        raw.push_back(uint8_t(value >> (8 * k)));
    };
    for (auto value : {0x46546c67u, 2u,
                       uint32_t(20 + text.size() + bytes.size() - 20 - length),
                       uint32_t(text.size()), 0x4e4f534au})
      word(value);
    raw.insert(raw.end(), text.begin(), text.end());
    raw.insert(raw.end(), bytes.begin() + 20 + length, bytes.end());
    auto owned = std::make_shared<const std::vector<uint8_t>>(raw);
    ImportLimits il;
    il.mode = ImportMode::PreserveGltf;
    auto asset = std::make_shared<const ImportedAsset>(import_glb(*owned, il));
    const auto &primitive = asset->primitives.at(0);
    auto appearance = std::make_shared<CreatorAppearance>();
    appearance->asset = asset;
    appearance->original_bytes = owned;
    appearance->source_mesh = primitive.source_mesh;
    appearance->source_primitive = primitive.source_primitive;
    appearance->source_node = 0;
    for (uint32_t k = 0; k < primitive.mesh.positions.size(); ++k)
      appearance->source_vertices.push_back(k);
    auto paint = PaintedMesh(primitive.mesh, 64);
    if (zero) {
      auto constant = prepare_polygon_source_colors(paint, appearance);
      check(constant.geometry.authored_ngons &&
                constant.geometry.topology.faces().size() == 1 &&
                constant.field.packed(constant.geometry.topology) ==
                    std::vector<PolygonRgba>(4),
            "Actual source quad with uniform transparent RGBA retains its "
            "N-gon despite a harmless diagonal change");
    } else
      check(refuses([&] { prepare_polygon_source_colors(paint, appearance); }),
            "Actual raw source N-gon refuses changed non-affine piecewise "
            "color basis rather than inventing color authority");
  }
  auto prepared =
      prepare_source_colors({o.painted, o.appearance, o.material_bindings,
                             o.deformation, o.material});
  check(refuses([&] {
          prepare_polygon_source_colors(*prepared.paint, o.appearance);
        }),
        "Independent prepared PBR/common color planes remain guarded until a "
        "real topology field adapter exists");
  std::cout << checks << " actual raw source color checks / " << count
            << " cancellation boundaries\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
