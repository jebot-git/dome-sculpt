// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/polygon-color.hpp"
#include "dome/polygon-scene.hpp"
#include "polygon-color-current-fixture.hpp"
#include "source-color-fixtures.hpp"
#include <filesystem>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
  ++checks;
  std::cout << "PASS " << m << std::endl;
}
void ready(Document &d) {
  check(d.wait_polygon(),
        "Actual color-aware polygon worker reaches ready state");
  if (!d.frame().state->error.empty())
    throw std::runtime_error(d.frame().state->error);
}
void extrude(Document &d) {
  d.polygon_pointer({.23f, .17f, 3}, {0, 0, -1}, PolygonAction::Pick);
  ready(d);
  check(d.frame().state->polygon->selection->ids.size() == 1,
        "Actual controller/native ray picks one source-colored face");
  d.edit_polygon(PolygonAction::Extrude);
  ready(d);
}
} // namespace
int main(int argc, char **argv) try {
  auto original = source_color_fixture::model();
  auto deform = *original.active().deformation;
  deform.morph_weights = {.6f};
  deform.pose[0].translation.x = .1f;
  original.set_deformation(original.active().id, deform);
  if (argc == 3 && (std::string_view(argv[1]) == "--ui-fixture" ||
                    std::string_view(argv[1]) == "--xr-fixture")) {
    if (std::string_view(argv[1]) == "--xr-fixture")
      original.active().transform.scale = .35f;
    original.save(argv[2]);
    return 0;
  }
  auto directory = std::filesystem::temp_directory_path() /
                   ("dome-current-colors-doc-" + std::to_string(getpid()));
  std::filesystem::create_directories(directory);
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code e;
      std::filesystem::remove_all(path, e);
    }
  } cleanup{directory};
  Document d;
  d.replace(original);
  check(d.wait_mesh(), "Actual raw COLOR_0, source maps and original "
                       "normal/skin/morph source publish");
  const auto source = d.copy_scene().encode();
  bool refused = false;
  try {
    d.begin_polygon(false);
  } catch (const std::logic_error &) {
    refused = true;
  }
  check(refused && d.copy_scene().encode() == source,
        "Raw source conversion remains explicit and retains original data on "
        "refusal");
  d.begin_polygon(true);
  ready(d);
  auto initial = d.frame().state->polygon;
  check(initial->colors && initial->colors->sources().size() > 0,
        "Explicit source conversion captures real immutable corner RGBA on "
        "background worker");
  auto initial_colors = initial->colors->packed(*initial->topology);
  float minimum_x = 1e9f, minimum_z = 1e9f;
  for (const auto &vertex : initial->topology->vertices()) {
    minimum_x = std::min(minimum_x, vertex.position.x);
    minimum_z = std::min(minimum_z, vertex.position.z);
  }
  check(std::abs(minimum_x + .65f) < 1e-6f &&
            std::abs(minimum_z - .03f) < 1e-6f,
        "Raw source corner expansion maps the actual active skin and morph "
        "pose without relabelling REST or dropping RGBA");
  d.end_polygon(true);
  ready(d);
  check(d.copy_scene().encode() == source && !d.frame().state->can_undo,
        "Unchanged color conversion Apply is exact no-op with no global Undo");
  d.begin_polygon(true);
  ready(d);
  auto basis = d.frame().state->polygon->colors->source_owner();
  extrude(d);
  check(d.frame().state->polygon->colors->source_owner() == basis &&
            d.copy_scene().encode() == source,
        "Draft topology interpolation shares immutable original RGBA while "
        "committed source remains exact");
  check(d.undo(), "Global Undo routes to color-aware local topology history");
  ready(d);
  check(
      d.frame().state->polygon->colors->packed(
          *d.frame().state->polygon->topology) == initial_colors &&
          !d.frame().state->polygon->changed,
      "Local Undo restores exact original corner colors and geometry together");
  check(d.redo(),
        "Local Redo restores complete topology and current corner field");
  ready(d);
  d.end_polygon(false);
  check(d.copy_scene().encode() == source && !d.frame().state->can_undo,
        "Cancel discards all color/topology drafts without consuming "
        "source/history");
  d.begin_polygon(true);
  ready(d);
  extrude(d);
  d.end_polygon(true);
  auto saved = d.save_async(directory / "accepted.dms");
  saved.get();
  ready(d);
  check(d.wait_mesh(),
        "Accepted Scene14 current colors publish immutable renderer snapshot");
  const auto accepted = d.copy_scene().encode();
  auto current = d.copy_scene();
  check(accepted["version"] == 14 && current.active().polygon->colors &&
            !current.active().appearance && !current.active().deformation,
        "One accepted native transaction publishes geometry-owned RGBA with "
        "explicit original-source capsule");
  auto expected = current.active().polygon->colors->packed(
      current.active().polygon->topology);
  const auto ordered_colors = [&](const MeshSnapshot &snapshot) {
    const auto &rest = current.active().painted->mesh();
    if (snapshot.mesh.indices.size() != rest.indices.size() ||
        snapshot.source_colors.size() != snapshot.mesh.positions.size())
      return false;
    for (size_t c = 0; c < rest.indices.size(); ++c)
      if (snapshot.source_colors.at(snapshot.mesh.indices[c]) !=
          expected.at(rest.indices[c]))
        return false;
    return true;
  };
  check(ordered_colors(*d.frame().surface),
        "Actual Document mesh worker packs exact ordered current corner RGBA "
        "through any independent tangent splits into existing GPU stream");
  check(Scene::load(directory / "accepted.dms").encode() == accepted,
        "Immediate accepted Apply then Save waits for exact completed Scene14 "
        "endpoint");
  const auto &origin = *current.active().polygon->origin;
  check(origin.paint && origin.appearance && origin.deformation &&
            origin.material_bindings &&
            *origin.appearance->original_bytes ==
                *original.active().appearance->original_bytes,
        "Actual topology conversion retains exact raw GLB, full paint, source "
        "maps and artist deformation/frame authority");
  check(d.undo() && d.copy_scene().encode() == source &&
            !d.frame().state->can_undo,
        "Exactly one Undo restores full original raw-colored source and "
        "authoring data");
  check(d.redo() && d.copy_scene().encode() == accepted,
        "Global Redo restores exact persisted corner interpolation authority");
  d.open_async(directory / "accepted.dms").get();
  check(d.wait_mesh() && d.copy_scene().encode() == accepted &&
            ordered_colors(*d.frame().surface),
        "Actual native load restarts current field and renderer source stream "
        "together");
  d.begin_polygon();
  ready(d);
  check(d.frame().state->polygon->colors &&
            d.frame().state->polygon->colors->packed(
                *d.frame().state->polygon->topology) == expected,
        "Restarted native polygon workspace resumes editable original field "
        "without another source conversion");
  d.edit_polygon(PolygonAction::SelectAll);
  ready(d);
  d.edit_polygon(PolygonAction::DeleteFaces);
  ready(d);
  d.end_polygon(true);
  ready(d);
  d.save(directory / "empty.dms");
  auto empty = Scene::load(directory / "empty.dms");
  check(empty.surfaces().empty() && empty.active().polygon->colors &&
            empty.active().polygon->origin->polygon->colors,
        "Delete-all persists no stale surface and retains complete latest "
        "painted corner field for Restore");
  d.restore_polygon_source();
  check(d.wait_asset() && d.frame().state->error.empty() &&
            d.copy_scene().encode() == accepted,
        "Source Restore recovers exact previous colored polygon authority and "
        "paint after Delete-all");
  d.restore_polygon_source();
  check(d.wait_asset() && d.frame().state->error.empty() &&
            d.copy_scene().encode() == source,
        "First source Restore returns exact original raw COLOR_0, normal "
        "frames, skin/morph and source textures");
  auto seam = polygon_current_fixture::surface(true);
  auto authority = std::make_shared<PolygonAuthority>();
  authority->topology = *seam.polygon_colors->topology;
  authority->colors = seam.polygon_colors->field;
  Scene alpha;
  alpha.active().mesh_active = true;
  alpha.active().painted = std::make_shared<PaintedMesh>(seam.mesh, 64);
  alpha.active().polygon = authority;
  alpha.active().material = seam.material;
  alpha.active().material.alpha = Material::AlphaMode::Mask;
  alpha.active().material.alpha_cutoff = .35f;
  d.replace(alpha);
  check(d.wait_mesh(), "Current corner-alpha scene publishes before actual "
                       "pointer/pressure checks");
  const Vec3 hidden{.16666667f, -.16666667f, 2},
      visible{-.16666667f, .16666667f, 2};
  check(!d.raycast(hidden, {0, 0, -1}) && d.raycast(visible, {0, 0, -1}),
        "Actual desktop/XR Document ray picking respects current zero-alpha "
        "seam rather than missing raw appearance fallback");
  d.configure_object_grab(true, true);
  check(d.wait_mesh(), "Held mode's new picking revision publishes before "
                       "world-frame ray assertions");
  d.hold_object(alpha.active().id);
  check(d.begin_object_grab(alpha.active().id),
        "Actual current-color object begins GPU held preview");
  auto held = d.copy_scene().active().transform;
  held.position.x = .15f;
  d.move_held_object(held);
  check(!d.raycast(hidden + Vec3{.15f, 0, 0}, {0, 0, -1}) &&
            d.raycast(visible + Vec3{.15f, 0, 0}, {0, 0, -1}),
        "Held world-frame delta preserves exact current alpha picking without "
        "relabelling REST colors");
  d.end_object_grab(false);
  d.release_held_object(false);
  d.configure_object_grab(false, false);
  check(d.wait_mesh(),
        "Original current-color surface is ready after cancelled held motion");
  PaintSettings paint;
  paint.color = {1, 0, 0, 1};
  paint.front_only = false;
  d.paint_settings(paint);
  auto before_alpha = d.copy_scene().encode();
  const auto stroke = [&](Vec3 origin) {
    check(d.begin_native_stroke(),
          "Native pressure gesture begins on exact current field source");
    NativeStrokeSample sample;
    sample.origin = origin;
    sample.direction = {0, 0, -1};
    sample.brush.radius = .08f;
    check(d.sample_native_stroke(sample),
          "Actual pressure sample enters ordered native mutation worker");
    d.end_native_stroke(true);
    check(d.wait_native_stroke(),
          "Current alpha-aware native worker drains accepted stroke");
    if (!d.frame().state->error.empty())
      throw std::runtime_error(d.frame().state->error);
  };
  stroke(hidden);
  check(d.copy_scene().encode() == before_alpha && !d.frame().state->can_undo,
        "Native paint worker skips fully transparent current corner alpha with "
        "exact no-op/history");
  stroke(visible);
  check(d.copy_scene().encode() != before_alpha && d.frame().state->can_undo &&
            d.copy_scene().active().polygon->colors == authority->colors,
        "Visible pressure paints current UV layers while immutable RGBA "
        "authority remains exact");
  check(d.undo() && d.copy_scene().encode() == before_alpha,
        "One Undo restores exact paint and current corner colors after visible "
        "pressure");
  auto guarded = alpha;
  auto plain = guarded.add("Plain source or target", Volume{});
  guarded.object(plain).painted = std::make_shared<PaintedMesh>(seam.mesh, 64);
  guarded.object(plain).mesh_active = true;
  guarded.select(alpha.active().id);
  d.replace(guarded);
  check(d.wait_mesh() && !d.frame().state->can_cut &&
            !d.frame().state->can_bake,
        "Native/menu readiness does not advertise unsupported "
        "appearance-transferring Cut or Bake for a current field");
  auto guarded_before = d.copy_scene().encode();
  bool cut_refused = false, bake_refused = false;
  try {
    d.begin_cut();
  } catch (const std::logic_error &) {
    cut_refused = true;
  }
  try {
    d.bake_from_object(plain);
  } catch (const std::logic_error &) {
    bake_refused = true;
  }
  check(cut_refused && bake_refused &&
            d.copy_scene().encode() == guarded_before,
        "Unsupported cut/bake entry leaves all current field, source and "
        "history exact");
  guarded.select(plain);
  const auto plain_before = guarded.encode();
  bool low_bake_refused = false;
  try {
    bake_object(guarded, alpha.active().id);
  } catch (const std::logic_error &) {
    low_bake_refused = true;
  }
  check(low_bake_refused && guarded.encode() == plain_before,
        "Low-level baking cannot silently ignore a current-color source "
        "accessor");
  d.replace(guarded);
  check(d.wait_mesh() && !d.frame().state->can_bake,
        "Current corner-field sources are excluded from native bake picker "
        "until a real adapter exists");
  std::cout << checks << " actual native current-color authoring checks\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
