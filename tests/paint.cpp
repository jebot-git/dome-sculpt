// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
#include <png.h>
#include <unistd.h>
using namespace dome;
Mesh fixture() {
  Mesh m;
  m.positions = {{-1, -1, 0}, {0, -1, 0}, {0, 1, 0}, {-1, 1, 0},
                 {0, -1, 0},  {1, -1, 0}, {1, 1, 0}, {0, 1, 0}};
  m.normals.assign(8, {0, 0, 1});
  m.uv = {{{.05f, .05f}}, {{.45f, .05f}}, {{.45f, .95f}}, {{.05f, .95f}},
          {{.55f, .05f}}, {{.95f, .05f}}, {{.95f, .95f}}, {{.55f, .95f}}};
  m.indices = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
  return m;
}
int main() try {
  auto require = [](bool b, const char *s) {
    if (!b)
      throw std::runtime_error(s);
    std::cout << "PASS " << s << '\n';
  };
  Material material;
  material.color = {.1f, .1f, .1f, 1};
  PaintedMesh paint(fixture(), 64);
  auto base = paint.composite(material)->color.rgba;
  auto hit = paint.raycast({.1f, .2f, 1}, {0, 0, -1});
  require(hit && length(*hit - Vec3{.1f, .2f, 0}) < 1e-6f,
          "Pinned Blender-style picking finds exact local-space mesh "
          "intersections");
  Brush b;
  b.normal = {0, 0, 1};
  b.radius = .5f;
  b.from = {-.2f, 0, 0};
  b.to = {.2f, 0, 0};
  PaintSettings settings;
  settings.color = {1, 0, 0, 1};
  paint.begin_stroke();
  paint.stamp(b, settings);
  require(paint.end_stroke(),
          "A swept pressure brush commits a native texture transaction");
  auto red = paint.composite(material);
  auto texel = [](const Image &im, int x, int y, int channel) {
    return im.rgba[(y * im.width + x) * 4 + channel];
  };
  require(texel(red->color, 28, 32, 0) > 230 &&
              texel(red->color, 35, 32, 0) > 230,
          "World-space painting crosses separate UV seam islands continuously");
  require(texel(red->color, 4, 4, 0) == base[(4 * 64 + 4) * 4],
          "Painting leaves unrelated texels unchanged");
  auto held = paint.snapshot();
  require(paint.undo() && paint.composite(material)->color.rgba == base,
          "Paint undo restores exact original texture bytes");
  require(held->composite(material)->color.rgba == red->color.rgba,
          "A worker's paint snapshot remains immutable across later undo");
  require(paint.redo() &&
              paint.composite(material)->color.rgba == red->color.rgba,
          "Paint redo restores exact committed texture bytes");
  paint.begin_stroke();
  settings.color = {0, 0, 1, 1};
  paint.stamp(b, settings);
  paint.cancel_stroke();
  require(paint.composite(material)->color.rgba == red->color.rgba,
          "Cancelling a paint stroke restores its tiles exactly");
  paint.add_layer("Blue glaze");
  paint.begin_stroke();
  paint.stamp(b, settings);
  paint.end_stroke();
  auto blue = paint.composite(material);
  require(texel(blue->color, 28, 32, 2) > 230,
          "A second layer composites independently above earlier paint");
  paint.layer_properties("Blue glaze", false, 1);
  require(paint.composite(material)->color.rgba == red->color.rgba,
          "Layer visibility reveals the unchanged lower layer");
  paint.layer_properties("Blue glaze", true, 1);
  settings.channel = PaintChannel::Mask;
  settings.mask = 0;
  paint.begin_stroke();
  paint.stamp(b, settings);
  paint.end_stroke();
  require(texel(paint.composite(material)->color, 28, 32, 0) > 220,
          "Painting a layer mask reveals lower paint without deleting either "
          "layer");
  require(paint.undo() &&
              paint.composite(material)->color.rgba == blue->color.rgba,
          "Layer mask painting has exact independent undo");
  settings.channel = PaintChannel::Metallic;
  settings.metallic = 1;
  paint.begin_stroke();
  paint.stamp(b, settings);
  paint.end_stroke();
  settings.channel = PaintChannel::Roughness;
  settings.roughness = 0;
  paint.begin_stroke();
  paint.stamp(b, settings);
  paint.end_stroke();
  auto pbr = paint.composite(material);
  require(texel(pbr->metal_rough, 28, 32, 2) > 240 &&
              texel(pbr->metal_rough, 28, 32, 1) < 15,
          "Metalness and roughness paint into independent linear glTF B/G "
          "channels");
  settings.channel = PaintChannel::Emission;
  settings.emission = {0, 1, 0};
  paint.begin_stroke();
  paint.stamp(b, settings);
  paint.end_stroke();
  require(texel(paint.composite(material)->emission, 28, 32, 1) > 240,
          "Emission is retained as an independent sRGB map");
  auto encoded = paint.encode();
  auto decoded = PaintedMesh::decode(encoded);
  require(decoded->encode() == encoded,
          "Persistent mesh buffers, UVs, layers, masks and channels round-trip "
          "exactly");
  auto png = pbr->color.png();
  png_image image{};
  image.version = PNG_IMAGE_VERSION;
  require(png_image_begin_read_from_memory(&image, png.data(), png.size()),
          "The native PNG encoder produces a decodable image");
  image.format = PNG_FORMAT_RGBA;
  std::vector<uint8_t> pixels(PNG_IMAGE_SIZE(image));
  require(png_image_finish_read(&image, nullptr, pixels.data(), 0, nullptr) &&
              pixels == pbr->color.rgba,
          "PNG decoding preserves exported texture rows and color bytes");
  png_image_free(&image);
  auto rejects = [&](nlohmann::json j, const char *s) {
    bool rejected = false;
    try {
      PaintedMesh::decode(j);
    } catch (const std::exception &) {
      rejected = true;
    }
    require(rejected, s);
  };
  auto bad = encoded;
  bad["layers"][0]["tiles"][0]["index"] = 999999;
  rejects(bad, "Out-of-range persisted tile references are rejected");
  bad = encoded;
  bad["mesh"]["uv"] = nlohmann::json::binary(std::vector<uint8_t>(8));
  rejects(bad, "Mismatched persisted mesh/UV buffers are rejected");
  PaintedMesh light(fixture(), 64), heavy(fixture(), 64);
  settings.channel = PaintChannel::Color;
  b.from = b.to = {0, 0, 0};
  b.radius = .6f;
  b.pressure = .25f;
  light.begin_stroke();
  light.stamp(b, settings);
  light.end_stroke();
  b.pressure = 1;
  heavy.begin_stroke();
  heavy.stamp(b, settings);
  heavy.end_stroke();
  auto count = [](const PaintedMesh &p) {
    size_t n = 0;
    for (uint32_t y = 0; y < 64; ++y)
      for (uint32_t x = 0; x < 64; ++x)
        n += p.pixel(1, x, y).data[9] > 0;
    return n;
  };
  require(count(light) * 2 < count(heavy),
          "Pressure changes the physical footprint of the paint brush");
  Document doc;
  doc.seed();
  auto original = doc.copy_volume().voxels();
  doc.prepare_mesh(128);
  auto reads = std::async(std::launch::async, [&] {
    for (int i = 0; i < 10000; ++i)
      doc.frame();
  });
  require(
      reads.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
      "XR render snapshots remain responsive while mesh/UV preparation runs");
  require(doc.wait_asset(std::chrono::seconds(180)) &&
              doc.frame().state->mesh_active,
          "The background worker publishes a durable painting mesh");
  auto mesh_before = doc.copy_scene().active().painted->mesh();
  settings.color = {0, 0, 1, 1};
  doc.paint_settings(settings);
  b.from = b.to = {0, 0, .38f};
  b.radius = .12f;
  require(doc.begin_desktop(),
          "A painted mesh accepts a desktop texture transaction");
  doc.stamp_desktop(b);
  doc.end_desktop();
  auto painted =
      doc.copy_scene().active().painted->composite(Material{})->color.rgba;
  require(doc.copy_volume().voxels() == original &&
              doc.copy_scene().active().painted->mesh().indices ==
                  mesh_before.indices,
          "Texture painting preserves sculpt source samples and stable mesh "
          "topology");
  require(doc.undo() && doc.redo() &&
              doc.copy_scene()
                      .active()
                      .painted->composite(Material{})
                      ->color.rgba == painted,
          "Texture strokes participate in shared document undo/redo");
  auto path = std::filesystem::temp_directory_path() /
              ("dome-paint-" + std::to_string(getpid()) + ".dsculpt");
  doc.save(path);
  auto scene = Scene::load(path);
  require(
      scene.active().mesh_active &&
          scene.active().painted->composite(Material{})->color.rgba == painted,
      "Version-three projects restore the painted mesh and editable layers");
  doc.paint_command(SpatialCommand::AddLayer);
  require(doc.undo() && !doc.frame().state->dirty,
          "Undoing a layer command returns to the clean saved painting state");
  auto saved =
      doc.copy_scene().active().painted->composite(Material{})->color.rgba;
  settings.color = {0, 1, 0, 1};
  doc.paint_settings(settings);
  doc.begin_spatial();
  SpatialFrame frame;
  frame.sequence = 100;
  frame.time = 1;
  frame.focused = frame.head_valid = true;
  frame.head.position = {0, 1.6f, 0};
  frame.hands[1].tracked.valid = true;
  frame.hands[1].tracked.grip.position = {0, 1.48f, -.48f};
  doc.submit(frame);
  require(doc.wait_input(), "A prepared mesh accepts a fresh spatial baseline");
  ++frame.sequence;
  frame.time += .02;
  frame.hands[1].tracked.trigger = 1;
  doc.submit(frame);
  require(doc.wait_input() && doc.frame().state->stroking,
          "A tracked controller begins a texture stroke");
  ++frame.sequence;
  frame.time += .02;
  frame.focused = false;
  doc.submit(frame);
  require(doc.wait_input() && !doc.frame().state->dirty &&
              doc.copy_scene()
                      .active()
                      .painted->composite(Material{})
                      ->color.rgba == saved,
          "XR focus loss cancels transient texture edits exactly and retains "
          "the saved state");
  bool last_layer = false;
  try {
    doc.paint_command(SpatialCommand::DeleteLayer);
  } catch (const std::exception &) {
    last_layer = true;
  }
  require(last_layer && doc.undo(),
          "A rejected last-layer deletion preserves earlier stroke history");
  std::filesystem::remove(path);
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
