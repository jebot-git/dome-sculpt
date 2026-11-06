// SPDX-License-Identifier: GPL-3.0-or-later
// Shared-owner codec qualification. This fixture deliberately exercises actual
// producer captures and actual typed holders. Full Scene8 and GUI Restore are
// separate integration gates; this test does not manufacture polygon authority.
#include "dome/source-maps.hpp"
#include "dome/source-paint.hpp"
#include "polygon-conversion.hpp"
#include "polygon-scene-archive.hpp"
#include "texture-set-archive.hpp"
#include "texture-set-paint-access.hpp"
#include <iostream>
using namespace dome;
using namespace dome::detail;
namespace {
using Json = nlohmann::json;
size_t checks = 0;
void check(bool yes, const char *message) {
  if (!yes)
    throw std::runtime_error(message);
  ++checks;
}
template <class F> void refuses(F &&f, const char *message) {
  bool caught = false;
  try {
    f();
  } catch (const std::exception &e) {
    caught = true;
    std::cout << "EXPECTED " << e.what() << '\n';
  }
  check(caught, message);
}
template <class T, class U>
bool owner(const std::shared_ptr<T> &a, const std::shared_ptr<U> &b) {
  return a && a == b && !a.owner_before(b) && !b.owner_before(a);
}
Scene fixture() {
  Mesh mesh;
  mesh.positions = {{-1, -1, 0},   {1, -1, 0},   {0, 1, 0},
                    {-1, -1, .5f}, {1, -1, .5f}, {0, 1, .5f}};
  mesh.normals.assign(6, {0, 0, 1});
  mesh.indices = {0, 1, 2, 3, 4, 5};
  mesh.uv = {{{.1f, .1f}}, {{.9f, .1f}}, {{.5f, .9f}},
             {{.1f, .1f}}, {{.9f, .1f}}, {{.5f, .9f}}};
  Scene scene;
  std::array<TextureSetInitialField, 2> fields;
  fields[0].resolution = 64;
  fields[1].resolution = 128;
  fields[0].material.name = "Back";
  fields[1].material.name = "Front";
  fields[0].material.color = {.8f, .1f, .2f, 1};
  fields[1].material.color = {.1f, .6f, .2f, 1};
  initialize_texture_sets(scene, scene.active().id, mesh, fields,
                          std::array<TextureSetId, 2>{0, 1});
  edit_texture_set(scene, scene.active().id, 1, [](PaintedMesh &p, Material &) {
    p.add_layer("Retained independent layer");
    return true;
  });
  return scene;
}
std::shared_ptr<const PolygonOrigin> capture(const Scene &s) {
  return capture_polygon_origin(s.active(), s.world_transform(s.active().id));
}
Json root(const Json &holder, uint32_t origin) {
  return {{"polygon", nullptr},    {"shape_source", origin},
          {"rig_source", nullptr}, {"material_bindings", nullptr},
          {"appearance", nullptr}, {"painted", nullptr},
          {"mesh_active", true},   {"texture_sets", holder}};
}
void shared_history() {
  auto scene = fixture();
  const auto before = capture(scene);
  const auto &original = scene.active();
  check(before->paint != original.painted &&
            owner(TextureSetPaintAccess::raster_owner(*before->paint),
                  TextureSetPaintAccess::raster_owner(*original.painted)),
        "Actual capture snapshots pixels while retaining the actual raster "
        "control block");
  check(owner(before->texture_sets, original.texture_sets),
        "Actual capture retains the complete collection owner");
  PaintImageArchive images;
  CreatorAssetArchive models;
  SourceMaterialArchive materials;
  TextureSetArchive writer;
  writer.shared_records();
  const auto current = writer.encode(original, images, nullptr, nullptr);
  PolygonSceneArchive history;
  const auto id = history.add_origin(before);
  const auto encoded = history.encode(images, models, materials, nullptr,
                                      nullptr, nullptr, nullptr, &writer);
  const auto geometry = writer.geometry_records();
  check(encoded.at("version") == 8 && encoded.at("bake_recipes") == false &&
            encoded.at("paints").empty(),
        "Retained collection emits genuine archive8 without duplicating its "
        "paint into the old pool");
  check(geometry.at("meshes").size() == 1 &&
            geometry.at("topologies").size() == 1 &&
            geometry.at("assignments").size() == 1 &&
            geometry.at("domains").size() == 2 &&
            geometry.at("rasters").size() == 2 &&
            geometry.at("paints").size() == 3 &&
            geometry.at("collections").size() == 1,
        "Explicit owner tables preserve actual "
        "geometry/domain/raster/paint/collection cardinalities");
  const auto objects = Json::array({root(current, id)});
  PolygonSceneArchive::preflight(encoded, objects, nullptr, nullptr, nullptr,
                                 &geometry);
  auto reader = TextureSetArchive::decode_geometry(geometry);
  auto decoded = original;
  decoded.painted.reset();
  decoded.texture_sets.reset();
  reader.decode(decoded, current, images, nullptr, nullptr, {});
  auto restored = PolygonSceneArchive::decode(
      encoded, images, models, materials, nullptr, nullptr, nullptr, nullptr,
      SourcePaintDecodeContext{}, &reader);
  reader.require_reachable();
  const auto &retained = *restored.origins().at(id);
  check(owner(decoded.texture_sets, retained.texture_sets),
        "Current and retained holder reuse their exact single collection "
        "control block");
  check(decoded.painted != retained.paint &&
            owner(TextureSetPaintAccess::raster_owner(*decoded.painted),
                  TextureSetPaintAccess::raster_owner(*retained.paint)),
        "Distinct captured pixel owners retain the same exact raster owner "
        "after decode");
  check(
      owner(decoded.painted->mesh_owner(), retained.paint->mesh_owner()) &&
          owner(decoded.painted->mesh_owner(),
                decoded.texture_sets->at(1).painted->mesh_owner()),
      "All current and retained fields keep one actual shared geometry owner");
  check(
      owner(decoded.texture_sets->primary_domain(),
            retained.texture_sets->primary_domain()) &&
          owner(TextureSetPaintAccess::domain(*retained.paint),
                decoded.texture_sets->primary_domain()),
      "Primary member domain and captured Paint7 control blocks remain paired");
  check(decoded.painted->resolution() == 64 &&
            decoded.texture_sets->at(1).painted->resolution() == 128 &&
            retained.texture_sets->at(1).painted->layers().size() == 2,
        "History preserves independent field resolutions and all layers");
  TextureSetArchive again;
  again.shared_records();
  const auto current2 = again.encode(decoded, images, nullptr, nullptr);
  const auto encoded2 = restored.encode(images, models, materials, nullptr,
                                        nullptr, nullptr, nullptr, &again);
  check(current2 == current && encoded2 == encoded &&
            again.geometry_records() == geometry,
        "Actual decoded shared-owner graph re-encodes exactly without "
        "value-based owner merging");
  auto missing = encoded;
  missing.at("origins")[id].erase("texture_sets");
  refuses(
      [&] {
        PolygonSceneArchive::preflight(missing, objects, nullptr, nullptr,
                                       nullptr, &geometry);
      },
      "Incomplete retained holder refuses before read");
  auto old = encoded;
  old["version"] = 7;
  old.erase("bake_recipes");
  refuses(
      [&] {
        PolygonSceneArchive::preflight(old, objects, nullptr, nullptr, nullptr,
                                       &geometry);
      },
      "Numeric old capability cannot admit retained shared ownership");
  auto forged = current;
  forged["primary"] = 1;
  refuses([&] { reader.preflight_holder(forged); },
          "An additional paint owner cannot substitute for the actual primary "
          "domain");
  auto source = geometry;
  source["paints"][0]["painted"]["field_source"] = 0;
  refuses(
      [&] {
        PolygonSceneArchive::preflight(encoded, objects, nullptr, nullptr,
                                       nullptr, &source);
      },
      "Plain archive8 cannot borrow source authority from an unrepresented "
      "field seed");
  auto unreachable = geometry;
  unreachable["paints"].push_back(unreachable["paints"][0]);
  auto spare = TextureSetArchive::decode_geometry(unreachable);
  auto recipient = original;
  recipient.painted.reset();
  recipient.texture_sets.reset();
  spare.decode(recipient, current, images, nullptr, nullptr, {});
  (void)PolygonSceneArchive::decode(encoded, images, models, materials, nullptr,
                                    nullptr, nullptr, nullptr,
                                    SourcePaintDecodeContext{}, &spare);
  refuses([&] { spare.require_reachable(); },
          "An equal-valued unused paint record remains a distinct unreachable "
          "owner");
  TextureSetLimits quota;
  quota.maximum_bytes = 1024;
  refuses([&] { (void)TextureSetArchive::decode_geometry(geometry, quota); },
          "Owning byte quota refuses before raster construction");
  TextureSetLimits cancelled;
  cancelled.cancelled = [] { return true; };
  refuses(
      [&] { (void)TextureSetArchive::decode_geometry(geometry, cancelled); },
      "Owning cancellation refuses before native publication");
  const auto untouched = decoded.painted;
  auto broken = current;
  broken["collection"] = 999;
  refuses([&] { reader.decode(decoded, broken, images, nullptr, nullptr, {}); },
          "Foreign holder refuses transactionally");
  check(decoded.painted == untouched &&
            decoded.texture_sets == retained.texture_sets,
        "Failure leaves the actual recipient's complete owners unchanged");
}
void distinct_equal_collections() {
  auto scene = fixture();
  const auto before = capture(scene);
  const auto original = scene.active().texture_sets;
  check(rename_texture_set(scene, scene.active().id, 1, "Temporary") &&
            rename_texture_set(scene, scene.active().id, 1, "Front"),
        "Real authoring creates a distinct collection with equal final "
        "names/values");
  check(!owner(original, scene.active().texture_sets),
        "Equal final values do not imply the same actual collection owner");
  PaintImageArchive images;
  TextureSetArchive writer;
  writer.shared_records();
  const auto current = writer.encode(scene.active(), images, nullptr, nullptr);
  const auto old = writer.encode(*before, images, nullptr, nullptr);
  const auto geometry = writer.geometry_records();
  check(current.at("collection") != old.at("collection") &&
            geometry.at("collections").size() == 2 &&
            geometry.at("assignments").size() == 1,
        "Distinct equal-valued collection owners serialize separately while "
        "their actual membership owner remains shared");
  auto reader = TextureSetArchive::decode_geometry(geometry);
  auto now = scene.active();
  now.painted.reset();
  now.texture_sets.reset();
  reader.decode(now, current, images, nullptr, nullptr, {});
  auto prior = *before;
  prior.paint.reset();
  prior.texture_sets.reset();
  reader.decode(prior, old, images, nullptr, nullptr, {});
  reader.require_reachable();
  check(!owner(now.texture_sets, prior.texture_sets) &&
            owner(now.texture_sets->primary_domain(),
                  prior.texture_sets->primary_domain()),
        "Decode preserves distinct collection control blocks and their "
        "authentic shared domain");
  check(
      owner(now.texture_sets->at(1).painted, prior.texture_sets->at(1).painted),
      "Unedited additional paint remains one exact shared field owner across "
      "collection history");
}
} // namespace
int main() try {
  shared_history();
  distinct_equal_collections();
  std::cout << "PASS " << checks
            << " actual shared texture history codec checks\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
