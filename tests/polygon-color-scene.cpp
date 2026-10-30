// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon-scene.hpp"
#include "dome/source-placement.hpp"
#include "polygon-color-current-fixture.hpp"
#include "polygon-conversion.hpp"
#include "polygon-paint.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool b, const char *message) {
  if (!b)
    throw std::runtime_error(message);
  ++checks;
  std::cout << "PASS " << message << std::endl;
}
template <class F> void rejects(F f, const char *message) {
  bool caught = false;
  try {
    f();
  } catch (const std::exception &) {
    caught = true;
  }
  check(caught, message);
}
Scene fixture() {
  auto original = source_color_fixture::model();
  auto captured = prepare_polygon_source_colors(*original.active().painted,
                                                original.active().appearance);
  auto edit = edit_colored_polygon(
      captured.geometry.topology, captured.field,
      {PolygonOperation::CutEdges,
       {PolygonSelectionMode::Edge, {captured.geometry.topology.edges()[0].id}},
       {},
       0,
       2});
  auto authority = std::make_shared<PolygonAuthority>();
  authority->topology = std::move(*edit.geometry.replacement);
  authority->colors =
      std::make_shared<const PolygonColorField>(std::move(*edit.colors));
  authority->origin = capture_polygon_origin(original, original.active().id);
  auto result = original;
  auto &target = result.active();
  PaintSurfaceOptions options;
  options.policy = PaintUvPolicy::PolygonAuthored;
  target.painted = polygon_paint_geometry(
      *target.painted, authority->topology.triangulate().mesh, options);
  target.appearance.reset();
  target.deformation.reset();
  target.material_bindings.reset();
  target.polygon = std::move(authority);
  result.validate();
  return result;
}
} // namespace
int main() try {
  auto scene = fixture();
  const auto encoded = scene.encode();
  check(encoded.at("version") == 14 &&
            encoded.at("polygon_sources").at("version") == 3,
        "Current editable corner RGBA requires Scene14 and polygon archive3");
  check(encoded.at("polygon_sources").at("color_bases").size() == 1 &&
            encoded.at("polygon_sources").at("color_fields").size() == 1,
        "Immutable source basis and current provenance have distinct bounded "
        "tables");
  auto expected =
      scene.active().polygon->colors->packed(scene.active().polygon->topology);
  auto restored = Scene::decode(
      nlohmann::json::from_cbor(nlohmann::json::to_cbor(encoded)));
  check(restored.encode() == encoded &&
            restored.active().polygon->colors->packed(
                restored.active().polygon->topology) == expected,
        "Scene14 CBOR restart retains exact float RGBA and double corner "
        "provenance");
  auto surfaces = restored.surfaces();
  check(surfaces.size() == 1 && surfaces[0].polygon_colors &&
            detail::current_vertex_colors(surfaces[0]) == expected,
        "Actual Scene surface producer supplies authenticated current REST "
        "color authority");
  const auto &origin = *restored.active().polygon->origin;
  check(origin.appearance &&
            origin.appearance->asset->primitives[0].colors ==
                scene.active()
                    .polygon->origin->appearance->asset->primitives[0]
                    .colors &&
            *origin.appearance->original_bytes ==
                *scene.active().polygon->origin->appearance->original_bytes &&
            origin.deformation && origin.material_bindings && origin.paint,
        "Original raw accessor bytes, full deformation, material and paint "
        "remain in the retained capsule");
  for (int version : {5, 11, 12, 13}) {
    auto old = encoded;
    old["version"] = version;
    rejects([&] { Scene::decode(old); },
            "Older Scene cannot silently ignore current corner colors");
  }
  auto bad = encoded;
  bad["polygon_sources"]["version"] = 1;
  rejects([&] { Scene::decode(bad); },
          "Legacy polygon archive rejects appended color tables");
  bad = encoded;
  bad["polygon_sources"]["version"] = 2;
  rejects([&] { Scene::decode(bad); },
          "Affine-only polygon archive rejects appended color tables");
  bad = encoded;
  bad["polygon_sources"].erase("color_fields");
  rejects([&] { Scene::decode(bad); },
          "Partial color archive fails before bulk decode");
  bad = encoded;
  bad["polygon_sources"]["authorities"][0]["colors"] = 99;
  rejects([&] { Scene::decode(bad); },
          "Missing current color field reference is refused");
  bad = encoded;
  bad["polygon_sources"]["color_fields"][0]["basis"] = 99;
  rejects([&] { Scene::decode(bad); },
          "Missing initial corner basis reference is refused");
  bad = encoded;
  bad["polygon_sources"]["color_bases"].push_back(
      bad["polygon_sources"]["color_bases"][0]);
  rejects([&] { Scene::decode(bad); },
          "Unreachable source color basis is refused");
  bad = encoded;
  bad["polygon_sources"]["color_fields"].push_back(
      bad["polygon_sources"]["color_fields"][0]);
  rejects([&] { Scene::decode(bad); },
          "Unreachable current corner field is refused");
  bad = encoded;
  bad["polygon_sources"]["color_bases"][0][0][1][3] = -1;
  rejects([&] { Scene::decode(bad); },
          "Out-of-domain straight alpha is refused");
  bad = encoded;
  bad["polygon_sources"]["color_fields"][0]["corners"][0][1][0][0] = UINT64_MAX;
  rejects([&] { Scene::decode(bad); },
          "A corner cannot borrow a missing immutable source sample");
  bad = encoded;
  bad["polygon_sources"]["color_fields"][0]["corners"][0][1][0][1] = .25;
  rejects([&] { Scene::decode(bad); },
          "Non-convex incomplete corner partition is refused");
  bad = encoded;
  bad["polygon_sources"]["color_fields"][0]["corners"].erase(0);
  rejects([&] { Scene::decode(bad); },
          "Current topology requires all exact corner identities");
  bad = encoded;
  bad["polygon_sources"]["color_bases"][0][0][0] = 0;
  rejects([&] { Scene::decode(bad); },
          "Source IDs remain nonzero stable component identities");
  // A prior authority with a different current map shares its exact immutable
  // initial basis, rather than serializing source COLOR_0 again per edit.
  auto nested = scene;
  auto prior = nested.active().polygon;
  auto moved = edit_colored_polygon(
      prior->topology, *prior->colors,
      {PolygonOperation::Move,
       {PolygonSelectionMode::Face, {prior->topology.faces()[0].id}},
       {0, 0, .02f}});
  auto next = std::make_shared<PolygonAuthority>();
  next->topology = std::move(*moved.geometry.replacement);
  next->colors =
      std::make_shared<const PolygonColorField>(std::move(*moved.colors));
  next->origin = capture_polygon_origin(nested, nested.active().id);
  PaintSurfaceOptions options;
  options.policy = PaintUvPolicy::PolygonAuthored;
  nested.active().painted = polygon_paint_geometry(
      *nested.active().painted, next->topology.triangulate().mesh, options);
  nested.active().polygon = next;
  auto nested_encoded = nested.encode();
  check(
      nested_encoded["polygon_sources"]["color_bases"].size() == 1 &&
          nested_encoded["polygon_sources"]["color_fields"].size() == 2,
      "Retained edit history deduplicates the immutable initial corner basis");
  auto nested_restored = Scene::decode(nested_encoded);
  check(nested_restored.active().polygon->colors->source_owner() ==
            nested_restored.active()
                .polygon->origin->polygon->colors->source_owner(),
        "Shared immutable source-basis aliases survive restart");
  auto before_restore = nested_restored.encode();
  restore_polygon_origin(nested_restored, nested_restored.active().id);
  check(nested_restored.encode() == encoded,
        "Explicit Restore recovers prior accepted corner authority and "
        "complete paint");
  auto inactive = std::make_shared<PolygonAuthority>(*next);
  inactive->active = false;
  Scene historical;
  historical.active().polygon = inactive;
  check(historical.encode()["version"] == 14,
        "Inactive and retained-only corner authority still propagates required "
        "Scene14");
  // Scene14 color tables coexist with an actual Scene13 captured native affine
  // placement. Historical transforms are provenance, not silently TRS-fitted.
  auto affine = scene;
  auto matrix = AffineTransform{}.matrix();
  matrix[4] = .2;
  affine.active().affine_edit =
      std::make_shared<const NativeAffineEdit>(AffineTransform(matrix));
  auto affine_origin = capture_polygon_origin(affine, affine.active().id);
  auto affine_authority =
      std::make_shared<PolygonAuthority>(*affine.active().polygon);
  affine_authority->origin = affine_origin;
  affine.active().polygon = affine_authority;
  auto combined = affine.encode();
  auto combined_restored = Scene::decode(combined);
  check(combined["version"] == 14 && combined.contains("affine_state") &&
            combined["polygon_sources"]["version"] == 3 &&
            combined_restored.encode() == combined &&
            combined_restored.active().polygon->origin->placement,
        "Actual Scene13 placement capsules and Scene14 color tables roundtrip "
        "together");
  check(scene.encode() == encoded && restored.encode() == encoded &&
            !before_restore.empty(),
        "Malformed records and restored derivatives never mutate their "
        "original source");
  std::cout << checks << " Scene14 current corner-color checks\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
