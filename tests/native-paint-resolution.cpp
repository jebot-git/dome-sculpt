// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/texture-set-resolution.hpp"
#include "texture-set-resample.hpp"
#include "import-controller-driver.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
using namespace dome;

void check(bool passed,const char *message) {
 if(!passed)throw std::runtime_error(message);
 std::cout << "PASS " << message << std::endl;
}

bool refuses(auto action) {
 try { action(); } catch(const std::exception &) { return true; }
 return false;
}

void ready(Document &doc) {
 if(!doc.wait_asset(std::chrono::seconds(180)) || !doc.wait_mesh(std::chrono::seconds(180)))
  throw std::runtime_error("Paint-resolution worker or surface timed out");
 if(!doc.frame().state->error.empty())
  throw std::runtime_error(doc.frame().state->error);
}

void resize_source_grid(const std::filesystem::path &root){
 for(const char *baseline:{"vroid-beta-female","vroid-beta-male"}){
  Scene scene;
  const auto group=append_humanoid(scene,humanoid_baseline_defaults(baseline),"Resize original face",{},256);
  const auto face=std::find_if(scene.objects().begin(),scene.objects().end(),[&](const auto &part){
    return part.group==group && part.name.find("Face_00_SKIN")!=std::string::npos;
  });
  check(face!=scene.objects().end()&&face->appearance&&face->painted->source_maps(),
        "Resize fixture has a genuine source-authored VRoid face and its original images");
  const auto id=face->id;
  scene.select(id);
  const auto before=scene.encode();
  auto denied=scene;
  auto small=detail::native_texture_grid_limits();
  small.maximum_bytes=1;
  check(refuses([&]{resample_native_texture_grid(denied,id,{1024},small);})&&denied.encode()==before,
        "Insufficient prospective raster memory refuses without changing the complete source scene");

  Document doc;
  doc.replace(scene);
  ready(doc);
  check(doc.can_resize_native_paint(),"An empty source-authored paint grid enables the native resolution control");
  ImportDriver hand(doc);
  hand.open();
  hand.sector(9);
  hand.sector(1);
  hand.sector(6);
  check(doc.frame().state->controls.parameter==VrParameter::TextureResolution,
        "The XR Materials and Texture Sets pages open the original mesh resolution control");
  hand.ray(1.f);
  hand.panel_button(true);
  const auto file=root/(std::string(baseline)+"-face2048.dsculpt");
  doc.save_async(file).get();
  ready(doc);
  const auto after=doc.copy_scene();
  const auto &current=after.object(id);
  check(current.painted->resolution()==2048&&current.painted->mesh_owner()==face->painted->mesh_owner()&&
        current.painted->source_maps()==face->painted->source_maps()&&current.appearance==face->appearance&&
        current.deformation==face->deformation&&current.material_bindings==face->material_bindings,
        "The controller changes only the selected native raster while retaining the mesh, images, rig and material");
  auto original_grid=after;
  original_grid.object(id).painted=face->painted;
  check(original_grid.encode()==before&&Scene::load(file).encode()==after.encode(),
        "The queued Save persists the changed grid and every unrelated source value exactly");
  check(doc.undo()&&doc.copy_scene().encode()==before&&doc.redo()&&doc.copy_scene().encode()==after.encode(),
        "Source-authored resolution change admits one exact Undo and Redo");


 }
}
void resize_painted_source_grid(const std::filesystem::path &root) {
  for (const char *baseline : {"vroid-beta-female", "vroid-beta-male"}) {
    Scene scene;
    const auto group = append_humanoid(scene, humanoid_baseline_defaults(baseline),
                                       "Painted source face", {}, 128);
    const auto face = std::ranges::find_if(scene.objects(), [&](const auto &part) {
      return part.group == group && part.name.find("Face_00_SKIN") != std::string::npos;
    });
    check(face != scene.objects().end() && face->appearance && face->painted->source_maps(),
          "Fixture contains an actual VRoid face with its source images");
    const auto id = face->id;
    scene.select(id);
    auto paint = face->painted->snapshot();
    scene.object(id).painted = paint;
    Brush brush;
    brush.from = brush.to = paint->mesh().positions.front();
    brush.radius = 2;
    PaintSettings ink;
    ink.front_only = false;
    ink.color = {.83f, .12f, .36f, .72f};
    const auto base = paint->active_layer();
    paint->begin_stroke();
    paint->stamp(brush, ink);
    ink.channel = PaintChannel::Mask;
    ink.mask = .59f;
    paint->stamp(brush, ink);
    check(paint->end_stroke(), "Source grid contains real painted colour, alpha and mask data");
    paint->add_layer("Hidden finish");
    const auto hidden = paint->active_layer();
    ink.channel = PaintChannel::Emission;
    ink.emission = {.25f, .68f, .42f};
    paint->begin_stroke();
    paint->stamp(brush, ink);
    check(paint->end_stroke(), "A second source layer contains authored emission");
    ink.emission_units = PaintEmissionUnits::Physical;
    ink.emission = {9.f, 23.f, 56.f};
    ink.flow = .61f;
    paint->begin_stroke();
    paint->stamp(brush, ink);
    check(paint->end_stroke() && paint->has_physical_emission(),
          "The hidden layer also owns a genuine FLOAT emission field");
    paint->layer_properties("Hidden finish", false, .48f);
    scene.validate();
    const auto before = scene.encode();
    TextureSetResolutionOptions options;
    options.resolution = 256;
    auto limits = detail::native_texture_grid_limits();
    if (compute_backend() != "CPU") {
      check(refuses([&] { resample_native_texture_grid(scene, id, options, limits); }) &&
                scene.encode() == before,
            "Missing accelerated grid resampling refuses without CPU substitution or paint loss");
      continue;
    }

    auto constrained = limits;
    unsigned polls = 0;
    constrained.cancelled = [&] { return ++polls >= 500; };
    check(refuses([&] { resample_native_texture_grid(scene, id, options, constrained); }) &&
              polls >= 500 && scene.encode() == before && scene.object(id).painted == paint,
          "Cancellation during private grid work preserves all original paint owners");
    constrained = limits;
    constrained.maximum_bytes = 1;
    check(refuses([&] { resample_native_texture_grid(scene, id, options, constrained); }) &&
              scene.encode() == before,
          "A prospective byte-budget refusal preserves the complete painted source");
    auto exact = options;
    exact.resolution = 64;
    exact.maximum_component_error = exact.maximum_rms_error = 0;
    check(refuses([&] { resample_native_texture_grid(scene, id, exact, limits); }) &&
              scene.encode() == before,
          "Inexact paint reconstruction refuses at a zero tolerance without changing the document");

    Document doc;
    doc.replace(scene);
    ready(doc);
    check(doc.can_resize_native_paint(),
          "A painted original grid enables the native resolution control");
    ImportDriver hand(doc);
    hand.open();
    hand.sector(9);
    hand.sector(1);
    hand.sector(6);
    check(doc.frame().state->controls.parameter == VrParameter::TextureResolution,
          "The shared XR texture-set control opens for painted source data");
    hand.ray(.4f);
    hand.panel_button(false);
    check(!doc.frame().state->asset_busy && doc.copy_scene().encode() == before,
          "Cancelling the controller resize keeps the scene and worker unchanged");
    if (!doc.frame().state->menu)
      hand.open();
    if (doc.frame().state->menu_page != 31) {
      hand.sector(9);
      hand.sector(1);
    }
    hand.sector(6);
    check(doc.frame().state->controls.parameter == VrParameter::TextureResolution,
          "The controller can reopen the size picker after cancellation");
    hand.ray(.4f);
    hand.panel_button(true);
    scene.save(root / (std::string(baseline) + "-painted128.dsculpt"));
    const auto file = root / (std::string(baseline) + "-painted256.dsculpt");
    doc.save_async(file).get();
    ready(doc);
    const auto after = doc.copy_scene();
    const auto &resized = after.object(id);
    check(resized.painted->resolution() == 256 && resized.painted->mesh_owner() == paint->mesh_owner() &&
              resized.painted->source_maps() == paint->source_maps() &&
              resized.material_bindings == face->material_bindings && resized.appearance == face->appearance &&
              resized.deformation == face->deformation,
          "Accepted resize retains mesh, original sources, materials and rig");
    const auto &layers = resized.painted->layers();
    check(layers.size() == 2 && layers[0].id == base && layers[1].id == hidden &&
              resized.painted->active_layer() == hidden && !layers[1].visible &&
              layers[1].name == "Hidden finish" && layers[1].opacity == .48f,
          "Resampling preserves layer order, identity, selection, visibility and opacity");
    auto float_at = [](const PaintedMesh &field, uint32_t x, uint32_t y) {
      const auto &tiles = field.layers().at(1).emission_tiles;
      const auto &tile = tiles.at(size_t(y / 32) * (field.resolution() / 32) + x / 32);
      return tile ? tile->pixels[(y % 32) * 32 + x % 32] : PaintEmissionValue{};
    };
    bool mask = false, emission = false, hdr = false, exact_layers = true;
    for (uint32_t y = 0; y < 256; ++y)
      for (uint32_t x = 0; x < 256; ++x) {
        const auto value = resized.painted->pixel(base, x, y);
        mask |= value.data[0] > 100 && value.data[9] > 64 &&
                value.data[13] > 80 && value.data[13] < 220;
        const auto finish = resized.painted->pixel(hidden, x, y);
        emission |= finish.data[7] > 100 && finish.data[12] > 64;
        exact_layers &= value == paint->pixel(base, x / 2, y / 2) &&
                        finish == paint->pixel(hidden, x / 2, y / 2) &&
                        float_at(*resized.painted, x, y) == float_at(*paint, x / 2, y / 2);
        hdr |= float_at(*resized.painted, x, y).physical[2] > 1.f;
      }
    check(mask && emission && hdr, "Painted colour, fractional masks and hidden emission all survive");
    check(exact_layers, "Enlargement retains every authored byte in both original layer grids");
    auto old_raster = after;
    old_raster.object(id).painted = paint;
    check(old_raster.encode() == before && Scene::load(file).encode() == after.encode(),
          "Ordered Save and reopen preserve the replacement grid and every unrelated Scene value");
    check(doc.undo() && doc.copy_scene().encode() == before &&
              doc.redo() && doc.copy_scene().encode() == after.encode(),
          "Resizing painted source data creates one exact Undo and Redo");
    auto reduced = after;
    options.resolution = 128;
    const auto report = resample_native_texture_grid(reduced, id, options, limits);
    auto restored = reduced.object(id).painted;
    bool restored_cells = true;
    for (uint32_t y = 0; y < 128; ++y)
      for (uint32_t x = 0; x < 128; ++x)
        restored_cells &= restored->pixel(base, x, y) == paint->pixel(base, x, y) &&
                          restored->pixel(hidden, x, y) == paint->pixel(hidden, x, y) &&
                          float_at(*restored, x, y) == float_at(*paint, x, y);
    check(report.changed && restored_cells && restored->source_maps() == paint->source_maps(),
          "Reducing the enlarged grid recovers every original authored cell and source owner");
    check(!resample_native_texture_grid(reduced, id, options, limits).changed &&
              reduced.object(id).painted == restored,
          "Same-size native resampling keeps the exact paint object");

    if (std::string_view(baseline) == "vroid-beta-female") {
      TextureSetEdit resize;
      resize.action = TextureSetAction::Resolution;
      resize.resolution = 1024;
      const auto source = doc.frame().state;
      doc.edit_texture_set(resize, {source->generation, source->revision,
                                   source->active_object, source->edit_authority});
      ready(doc);
      const auto large = doc.copy_scene();
      const auto &larger = *large.object(id).painted;
      bool exact_hdr = larger.resolution() == 1024;
      for (uint32_t y = 0; y < larger.resolution(); ++y)
        for (uint32_t x = 0; x < larger.resolution(); ++x)
          exact_hdr &= float_at(larger, x, y) == float_at(*paint, x / 8, y / 8);
      check(exact_hdr && larger.source_maps() == paint->source_maps(),
            "The desktop command enlarges sparse painted HDR layers to 1024");
      check(doc.undo() && doc.copy_scene().encode() == after.encode(),
            "One undo restores the previous native paint size");
    }
  }
}

} // namespace

int main(int argc,char **argv) try {
 if(argc!=3)throw std::invalid_argument("Expected a fixture kind and output directory");
 const std::filesystem::path root=argv[2];
 std::filesystem::create_directories(root);
 if(std::string_view(argv[1])=="empty")
  resize_source_grid(root);
 else
  resize_painted_source_grid(root);
 return 0;
} catch(const std::exception &error) {
 std::cerr << "FAIL " << error.what() << '\n';
 return 1;
}
