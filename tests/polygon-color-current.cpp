// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/gltf-export.hpp"
#include "dome/material-visibility.hpp"
#include "dome/render.hpp"
#include "polygon-color-current-fixture.hpp"
#include "source-color-consumer.hpp"
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
unsigned checks = 0;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
  ++checks;
  std::cout << "PASS " << m << std::endl;
}
template <class F> bool rejects(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
} // namespace
int main() try {
  auto surface = polygon_current_fixture::surface();
  auto colors = detail::current_vertex_colors(surface);
  check(colors == surface.polygon_colors->field->packed(
                      *surface.polygon_colors->topology),
        "Transient geometry-owned field packs exact current corner RGBA "
        "without a fabricated appearance");
  auto bad = surface;
  bad.mesh.positions[0].x += .01f;
  check(rejects([&] { detail::current_vertex_colors(bad); }),
        "Equal-size changed rest positions cannot borrow a current color "
        "authority");
  bad = surface;
  std::swap(bad.mesh.indices[0], bad.mesh.indices[1]);
  check(
      rejects([&] { detail::current_vertex_colors(bad); }),
      "Reordered triangle identity cannot borrow the field by matching counts");
  bad = surface;
  bad.mesh.uv[0][0] += .01f;
  check(rejects([&] { detail::current_vertex_colors(bad); }),
        "Changed ordered corner UVs cannot silently reuse a field witness");
  bad = surface;
  bad.mesh.normals[0].x += .1f;
  check(
      rejects([&] { detail::current_vertex_colors(bad); }),
      "Changed ordered rest normals cannot borrow a current geometry witness");
  auto uv_controls = surface;
  uv_controls.mesh.uv_pins.assign(surface.mesh.positions.size(), 1);
  uv_controls.mesh.uv_seams = {{0, 1}};
  check(
      detail::current_vertex_colors(uv_controls) == colors,
      "UV pin and seam authoring controls do not relabel or discard the exact "
      "corner color identity");
  auto deformation = std::make_shared<DeformationState>(surface.mesh);
  deformation->morphs = MorphSet(surface.mesh);
  auto moved = surface.mesh.positions;
  for (auto &p : moved)
    p.x += .1f;
  deformation->morphs->set_target(surface.mesh, "move", moved);
  deformation->morph_weights = {1};
  surface.deformation = deformation;
  check(detail::current_vertex_colors(surface) == colors,
        "A live pose/morph retains REST identity and does not resample "
        "immutable colors");
  bad = surface;
  bad.mesh = bad.preview_mesh();
  bad.deformation.reset();
  check(rejects([&] { detail::current_vertex_colors(bad); }),
        "Evaluated positions cannot be relabelled as a different REST color "
        "identity");
  auto maps = std::make_shared<TextureMaps>();
  maps->color_projection = std::make_shared<PaintColorProjection>();
  bad = surface;
  bad.textures = maps;
  check(rejects([&] { detail::current_vertex_colors(bad); }),
        "Conflicting prepared projection is explicitly rejected rather than "
        "preferred or dropped");
  auto bindings = std::make_shared<SourceMaterialBindings>();
  bindings->color_preparation = std::make_shared<SourceColorPreparation>();
  bad = surface;
  bad.material_bindings = bindings;
  check(
      rejects([&] { detail::current_vertex_colors(bad); }),
      "Conflicting material receipt cannot masquerade as a raw geometry field");
  bad = surface;
  auto orphan = std::make_shared<TextureMaps>();
  orphan->pbr_color = {1, 1, {255, 255, 255, 255}};
  bad.textures = orphan;
  check(rejects([&] { detail::current_vertex_colors(bad); }),
        "Unbound resolved PBR plane cannot silently coexist with raw geometry "
        "colors");
  auto path = std::filesystem::temp_directory_path() /
              ("dome-current-colors-" + std::to_string(getpid()) + ".glb");
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code e;
      std::filesystem::remove(path, e);
    }
  } cleanup{path};
  export_glb({surface}, path);
  color_consumer::Model model(path);
  auto &primitive = model.data->meshes[0].primitives[0];
  auto accessor =
      color_consumer::attr(primitive.attributes, primitive.attributes_count,
                           cgltf_attribute_type_color);
  bool exact = accessor && accessor->count == colors.size();
  for (size_t i = 0; exact && i < colors.size(); ++i) {
    float v[4];
    exact = cgltf_accessor_read_float(accessor, i, v, 4);
    for (size_t k = 0; k < 4; ++k)
      exact &= v[k] == colors[i][k];
  }
  check(exact, "Production GLB export and independent cgltf parsing retain "
               "every float RGBA including straight alpha");
  auto framed = surface;
  framed.deformation.reset();
  auto frame_origin = std::make_shared<SourceUvOrigin>();
  PaintSurfaceOptions source_options;
  frame_origin->paint = PaintedMesh::from_mesh(framed.mesh, 64, source_options);
  auto normals = std::make_shared<NormalFrames>();
  normals->origin_vertex_count = uint32_t(framed.mesh.positions.size());
  normals->origin_vertices = framed.mesh.indices;
  for (auto v : framed.mesh.indices)
    normals->normals.push_back(framed.mesh.normals[v]);
  bool split = false;
  for (size_t c = 1; c < framed.mesh.indices.size() && !split; ++c)
    if (std::find(framed.mesh.indices.begin(), framed.mesh.indices.begin() + c,
                  framed.mesh.indices[c]) != framed.mesh.indices.begin() + c) {
      normals->normals[c] = normalized(Vec3{.1f, 0, .99f});
      split = true;
    }
  check(split, "Frame export fixture has a repeated REST corner vertex");
  auto set = std::make_shared<NormalFrameSet>();
  set->pbr = normals;
  auto frame_binding = std::make_shared<SourceMaterialBindings>();
  frame_binding->has_mtoon = false;
  frame_binding->normal_frame = SourceMaterialNormalFrame::OriginalAttributes;
  frame_binding->uv_origin = frame_origin;
  frame_binding->normal_frames = set;
  framed.material_bindings = frame_binding;
  export_glb({framed}, path);
  color_consumer::Model frame_model(path);
  const auto &frame_primitive = frame_model.data->meshes[0].primitives[0];
  auto frame_colors = color_consumer::attr(frame_primitive.attributes,
                                           frame_primitive.attributes_count,
                                           cgltf_attribute_type_color);
  auto frame_positions = color_consumer::attr(frame_primitive.attributes,
                                              frame_primitive.attributes_count,
                                              cgltf_attribute_type_position);
  auto frame_uv = color_consumer::attr(frame_primitive.attributes,
                                       frame_primitive.attributes_count,
                                       cgltf_attribute_type_texcoord);
  check(
      frame_positions && frame_positions->count > framed.mesh.positions.size(),
      "Independent normal corners actually expand the exported vertex domain");
  bool corner_exact =
      frame_colors && frame_uv && frame_primitive.indices &&
      frame_colors->count == frame_positions->count &&
      frame_primitive.indices->count == framed.mesh.indices.size();
  for (size_t c = 0; corner_exact && c < framed.mesh.indices.size(); ++c) {
    auto exported = cgltf_accessor_read_index(frame_primitive.indices, c);
    auto rest = framed.mesh.indices[c];
    float rgba[4], xyz[3], uv[2];
    corner_exact =
        cgltf_accessor_read_float(frame_colors, exported, rgba, 4) &&
        cgltf_accessor_read_float(frame_positions, exported, xyz, 3) &&
        cgltf_accessor_read_float(frame_uv, exported, uv, 2);
    for (size_t k = 0; k < 4; ++k)
      corner_exact &= rgba[k] == colors[rest][k];
    const auto p = framed.mesh.positions[rest];
    corner_exact &= xyz[0] == p.x && xyz[1] == p.y && xyz[2] == p.z;
    corner_exact &=
        uv[0] == framed.mesh.uv[rest][0] && uv[1] == framed.mesh.uv[rest][1];
  }
  check(corner_exact,
        "Normal-frame splitting preserves exact ordered REST positions, UVs "
        "and current corner RGBA without borrowing a new authority");
  const std::array<uint32_t, 1> invalid_remap{uint32_t(colors.size())};
  check(rejects([&] {
          detail::remap_current_vertex_colors(colors, invalid_remap);
        }),
        "Frame attribute expansion refuses an out-of-domain REST mapping");
  const std::array<uint32_t, 1> valid_remap{0};
  check(rejects([&] {
          detail::remap_current_vertex_colors(colors, valid_remap,
                                              [] { return true; });
        }) &&
            detail::current_vertex_colors(surface) == colors,
        "Cancelled frame color expansion leaves the immutable source exact");
  auto seam = polygon_current_fixture::surface(true);
  auto packed = detail::current_vertex_colors(seam);
  std::vector<SurfaceRange> ranges{
      {1, 0, seam.mesh.indices.size(), seam.material}};
  ranges[0].material.alpha = Material::AlphaMode::Mask;
  ranges[0].material.alpha_cutoff = .35f;
  MaterialSurfaceQuery actual(seam.mesh, ranges, {}, packed);
  MaterialSurfaceQuery old(seam.mesh, ranges);
  auto center = [&](size_t triangle) {
    Vec3 p{};
    for (size_t k = 0; k < 3; ++k)
      p += seam.mesh.positions[seam.mesh.indices[triangle * 3 + k]];
    p = p * (1.f / 3);
    p.z = 1;
    return p;
  };
  check(!actual.raycast(center(0), {0, 0, -1}) &&
            actual.raycast(center(1), {0, 0, -1}) &&
            old.raycast(center(0), {0, 0, -1}),
        "Packed seam alpha rejects the transparent side that legacy "
        "missing-stream picking would incorrectly accept");
  auto missing = packed;
  missing.pop_back();
  check(
      rejects([&] { MaterialSurfaceQuery q(seam.mesh, ranges, {}, missing); }),
      "Packed alpha stream requires the exact complete vertex domain");
  missing = packed;
  missing[0][3] = std::numeric_limits<float>::quiet_NaN();
  check(
      rejects([&] { MaterialSurfaceQuery q(seam.mesh, ranges, {}, missing); }),
      "Malformed alpha is refused before it can be used for picking");
  auto original = source_color_fixture::model(true);
  const auto &o = original.active();
  auto prepared =
      prepare_source_colors({o.painted, o.appearance, o.material_bindings,
                             o.deformation, o.material});
  prepared.paint->begin_stroke();
  Brush brush;
  brush.from = brush.to = {0, 0, 0};
  brush.normal = {0, 0, 1};
  brush.radius = 5;
  PaintSettings white;
  white.color = {1, 1, 1, 1};
  white.front_only = false;
  prepared.paint->stamp(brush, white);
  prepared.paint->end_stroke();
  SurfaceRange prep{1,
                    0,
                    o.painted->mesh().indices.size(),
                    o.material,
                    prepared.paint->composite(o.material),
                    o.appearance,
                    0,
                    prepared.material_bindings};
  prep.material.alpha = Material::AlphaMode::Mask;
  prep.material.alpha_cutoff = .35f;
  std::vector<SurfaceRange> prepared_ranges{prep};
  std::span<const PolygonRgba> absent;
  MaterialSurfaceQuery empty_prepared(o.painted->mesh(), prepared_ranges, {},
                                      absent);
  std::vector<PolygonRgba> identity(o.painted->mesh().positions.size(),
                                    {1, 1, 1, 1});
  MaterialSurfaceQuery mixed_prepared(o.painted->mesh(), prepared_ranges, {},
                                      identity);
  check(empty_prepared.raycast({0, 0, 2}, {0, 0, -1}) &&
            mixed_prepared.raycast({0, 0, 2}, {0, 0, -1}),
        "Prepared empty or identity streams suppress original zero-alpha "
        "appearance instead of multiplying it again");
  auto double_tint = identity;
  double_tint[0][3] = 0;
  check(rejects([&] {
          MaterialSurfaceQuery q(o.painted->mesh(), prepared_ranges, {},
                                 double_tint);
        }),
        "Prepared geometry rejects a second nonidentity packed tint");
  auto raw_range = prep;
  raw_range.textures = o.painted->composite(o.material);
  raw_range.material_bindings = o.material_bindings;
  std::vector<SurfaceRange> raw_ranges{raw_range};
  MaterialSurfaceQuery legacy_raw(o.painted->mesh(), raw_ranges);
  check(!legacy_raw.raycast({0, 0, 2}, {0, 0, -1}) && rejects([&] {
    MaterialSurfaceQuery q(o.painted->mesh(), raw_ranges, {}, absent);
  }),
        "Old constructor keeps legacy accessor alpha, while an explicit "
        "unprepared stream cannot omit authored data");
  auto combined = o.painted->mesh();
  const size_t first = combined.positions.size(),
               first_index = combined.indices.size();
  for (auto p : o.painted->mesh().positions) {
    p.x += 2;
    combined.positions.push_back(p);
  }
  combined.normals.insert(combined.normals.end(),
                          o.painted->mesh().normals.begin(),
                          o.painted->mesh().normals.end());
  combined.uv.insert(combined.uv.end(), o.painted->mesh().uv.begin(),
                     o.painted->mesh().uv.end());
  for (auto v : o.painted->mesh().indices)
    combined.indices.push_back(uint32_t(first + v));
  raw_range.first = first_index;
  raw_range.first_vertex = first;
  std::vector<SurfaceRange> mixed_ranges{prep, raw_range};
  auto mixed_colors = identity;
  mixed_colors.insert(mixed_colors.end(), first, {0, 0, 0, 0});
  MaterialSurfaceQuery mixed_query(combined, mixed_ranges, {}, mixed_colors);
  check(mixed_query.raycast({0, 0, 2}, {0, 0, -1}) &&
            !mixed_query.raycast({2, 0, 2}, {0, 0, -1}),
        "Mixed prepared and raw ranges use bounded offsets without double tint "
        "or lost raw alpha");
  std::cout << checks << " current color transport checks\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
