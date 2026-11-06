// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/texture-set-resolution.hpp"
#include "dome/document.hpp"
#include "paint-color-access.hpp"
#include "texture-set-controls.hpp"
#include "texture-set-paint-access.hpp"
#include "texture-set-resample.hpp"
#include <iostream>
using namespace dome;
namespace {
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
void settle(Document &d) {
  check(d.wait_asset(std::chrono::seconds(120)) &&
            d.wait_mesh(std::chrono::seconds(120)),
        "Actual native worker queues terminate");
  if (!d.frame().state->error.empty())
    throw std::runtime_error(d.frame().state->error);
}
void stamp(PaintedMesh &p, PaintSettings settings, float strength = 1) {
  settings.front_only = false;
  Brush b;
  b.from = b.to = {0, 0, .5f};
  b.normal = {0, 0, 1};
  b.radius = 10000;
  b.strength = strength;
  p.begin_stroke();
  p.stamp(b, settings);
  check(p.end_stroke(), "Actual native stamp authors the requested plane");
}
Scene fixture() {
  Mesh m;
  m.positions = {{-1, -1, 0},   {1, -1, 0},   {1, 1, 0},   {-1, 1, 0},
                 {-1, -1, .5f}, {1, -1, .5f}, {1, 1, .5f}, {-1, 1, .5f}};
  m.normals.assign(8, {0, 0, 1});
  m.uv = {{{0, 0}}, {{1, 0}}, {{1, 1}}, {{0, 1}},
          {{0, 0}}, {{1, 0}}, {{1, 1}}, {{0, 1}}};
  m.indices = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
  Scene s;
  std::array<TextureSetInitialField, 2> fields;
  fields[0].resolution = 64;
  fields[1].resolution = 128;
  fields[0].material.name = "Sibling";
  fields[1].material.name = "Resize";
  initialize_texture_sets(s, s.active().id, m, fields,
                          std::array<TextureSetId, 4>{0, 0, 1, 1});
  select_texture_set(s, s.active().id, 1);
  edit_texture_set(s, s.active().id, 1, [](PaintedMesh &p, Material &) {
    PaintSettings ink;
    ink.color = {.18f, .62f, .91f, .4f};
    stamp(p, ink, .7f);
    ink.channel = PaintChannel::Metallic;
    ink.metallic = .72f;
    stamp(p, ink, .6f);
    ink.channel = PaintChannel::Roughness;
    ink.roughness = .23f;
    stamp(p, ink, .8f);
    ink.channel = PaintChannel::Emission;
    ink.emission = {.27f, .04f, .63f};
    stamp(p, ink, .5f);
    ink.emission_units = PaintEmissionUnits::Physical;
    ink.emission = {6000, .002f, 8};
    stamp(p, ink, .3f);
    ink.channel = PaintChannel::Mask;
    ink.mask = .35f;
    stamp(p, ink);
    p.add_layer("Hidden masked authoring");
    ink = {};
    ink.color = {.7f, .21f, .06f, .6f};
    stamp(p, ink, .4f);
    ink.channel = PaintChannel::Mask;
    ink.mask = .6f;
    stamp(p, ink);
    p.layer_properties("Hidden masked authoring", false, 0);
    p.add_layer("Sub-UNORM8 physical coverage");
    ink = {};
    ink.channel = PaintChannel::Emission;
    ink.emission_units = PaintEmissionUnits::Physical;
    ink.emission = {10000, .003f, 17};
    stamp(p, ink, .0001f);
    return true;
  });
  return s;
}
long double decode(uint8_t value) {
  const long double x = static_cast<long double>(value) / 255;
  return x <= .04045L ? x / 12.92L : powl((x + .055L) / 1.055L, 2.4L);
}
int encode(long double value) {
  const auto v = value <= .0031308L ? 12.92L * value
                                    : 1.055L * powl(value, 1.L / 2.4L) - .055L;
  return int(llroundl(std::clamp(v, 0.L, 1.L) * 255));
}
void oracle(const PaintedMesh &old, const PaintedMesh &now,
            bool require_variation = false) {
  size_t varied = 0, nonnearest = 0, wrong_gamma = 0;
  check(old.resolution() == 128 && now.resolution() == 64,
        "Independent area oracle receives the actual128-to64 field");
  for (const auto &layer : old.layers())
    for (uint32_t y : {0u, 9u, 31u, 63u})
      for (uint32_t x : {0u, 13u, 42u, 63u}) {
        const std::array<PaintPixel, 4> p{
            old.pixel(layer.id, 2 * x, 2 * y),
            old.pixel(layer.id, 2 * x + 1, 2 * y),
            old.pixel(layer.id, 2 * x, 2 * y + 1),
            old.pixel(layer.id, 2 * x + 1, 2 * y + 1)};
        const auto actual = now.pixel(layer.id, x, y);
        varied += std::any_of(p.begin() + 1, p.end(),
                              [&](const auto &v) { return v != p[0]; });
        nonnearest += actual != p[0];
        for (size_t g = 0; g < 4; ++g) {
          if (g == 3 && !layer.emission_tiles.empty())
            continue; // FLOAT coverage owns this fallback.
          const size_t first = g == 0   ? 0
                               : g == 1 ? 4
                               : g == 2 ? 5
                                        : 6,
                       n = g == 0   ? 4
                           : g == 3 ? 3
                                    : 1;
          long double coverage = 0;
          for (const auto &v : p)
            coverage += v.data[9 + g];
          check(std::abs(int(actual.data[9 + g]) -
                         int(llroundl(coverage / 4))) <= 1,
                "Independent uniform footprint preserves each distinct channel "
                "coverage");
          for (size_t k = 0; k < n; ++k) {
            long double value = 0;
            for (const auto &v : p) {
              const auto sample =
                  first + k < 3 || first + k >= 6
                      ? decode(v.data[first + k])
                      : static_cast<long double>(v.data[first + k]) / 255;
              value += sample * (coverage ? v.data[9 + g] : 1);
            }
            value /= coverage ? coverage : 4;
            const auto expected = first + k < 3 || first + k >= 6
                                      ? encode(value)
                                      : int(llroundl(value * 255));
            if (g == 0 && k < 3 && coverage) {
              long double uncorrected = 0;
              for (const auto &v : p)
                uncorrected += static_cast<long double>(v.data[k]) * v.data[9];
              uncorrected /= coverage;
              wrong_gamma +=
                  std::abs(double(expected) - double(uncorrected)) > 2;
            }
            check(std::abs(int(actual.data[first + k]) - expected) <= 1,
                  "Independent linear-light associated area integral matches "
                  "every authored common value");
          }
        }
        const auto mask = (int(p[0].data[13]) + p[1].data[13] + p[2].data[13] +
                           p[3].data[13]) /
                          4.;
        check(std::abs(int(actual.data[13]) - int(std::lround(mask))) <= 1,
              "Mask remains independent from all four channel coverages");
        const std::array<PaintEmissionValue, 4> e{
            old.emission_pixel(layer.id, 2 * x, 2 * y),
            old.emission_pixel(layer.id, 2 * x + 1, 2 * y),
            old.emission_pixel(layer.id, 2 * x, 2 * y + 1),
            old.emission_pixel(layer.id, 2 * x + 1, 2 * y + 1)};
        const auto f = now.emission_pixel(layer.id, x, y);
        long double coverage = 0;
        for (const auto &v : e)
          coverage += v.coverage;
        if (!layer.emission_tiles.empty())
          check(std::abs(int(actual.data[12]) -
                         int(std::lround(float(coverage / 4) * 255))) <= 1,
                "UNORM8 emission fallback follows the actual authoritative "
                "FLOAT coverage");
        check(std::abs(f.coverage - double(coverage / 4)) < 1e-6,
              "Actual FLOAT coverage stays independent of UNORM8 fallback");
        for (size_t k = 0; k < 3; ++k) {
          long double legacy = 0, physical = 0;
          for (const auto &v : e) {
            legacy += v.legacy[k] * (coverage ? v.coverage : 1);
            physical += v.physical[k] * (coverage ? v.coverage : 1);
          }
          legacy /= coverage ? coverage : 4;
          physical /= coverage ? coverage : 4;
          check(std::abs(f.legacy[k] - double(legacy)) < 2e-6 &&
                    std::abs(f.physical[k] - double(physical)) < .002,
                "Independent physical and legacy unit integrals preserve "
                "radiance without clamp or strength baking");
        }
      }
  if (require_variation)
    check(varied && nonnearest && wrong_gamma,
          "Asymmetric actual authoring distinguishes footprint integration, "
          "linear-light filtering and nearest-neighbor copying");
}
void numeric() {
  auto scene = fixture();
  const auto original = scene.snapshot();
  const auto before = texture_set_view(original.active(), 1);
  const auto sibling = scene.active().painted;
  const auto collection = original.active().texture_sets;
  TextureSetResolutionOptions options;
  options.resolution = 64;
  auto report = resample_texture_set(scene, scene.active().id, 1, options);
  const auto field = texture_set_view(scene.active(), 1);
  check(report.changed && report.original_resolution == 128 &&
            report.resolution == 64 &&
            report.layers.size() == before.painted->layers().size() &&
            report.peak_bytes && report.work,
        "Actual resolution API reports its bounded complete transfer and "
        "measured layers");
  check(scene.active().painted == sibling && field.domain == before.domain &&
            field.painted->mesh_owner() == before.painted->mesh_owner(),
        "Resizing preserves sibling paint, actual member-domain and shared "
        "geometry control blocks");
  check(detail::TextureSetPaintAccess::raster_owner(*field.painted) !=
            detail::TextureSetPaintAccess::raster_owner(*before.painted),
        "Requested grid receives a genuinely new actual raster owner");
  check(field.painted->active_layer() == before.painted->active_layer() &&
            field.painted->layers()[1].name == "Hidden masked authoring" &&
            !field.painted->layers()[1].visible &&
            field.painted->layers()[1].opacity == 0,
        "All hidden/masked layer metadata and active ID survive");
  const auto tiny =
      field.painted->emission_pixel(field.painted->active_layer(), 20, 20);
  check(tiny.coverage > 0 && tiny.coverage < 1. / 255 &&
            tiny.physical[0] > 9000 &&
            field.painted->pixel(field.painted->active_layer(), 20, 20)
                    .data[12] == 0,
        "Sub-UNORM8 FLOAT coverage and native high radiance remain authored "
        "after resize");
  oracle(*before.painted, *field.painted);
  auto asymmetric = fixture();
  edit_texture_set(asymmetric, asymmetric.active().id, 1,
                   [](PaintedMesh &p, Material &) {
                     p.add_layer("Asymmetric mixed-unit filtering control");
                     PaintSettings ink;
                     ink.front_only = false;
                     ink.color = {.95f, .1f, .4f, .3f};
                     Brush brush;
                     brush.from = brush.to = {-.55f, -.7f, .5f};
                     brush.normal = {0, 0, 1};
                     brush.radius = .28f;
                     brush.strength = .7f;
                     const auto draw = [&] {
                       p.begin_stroke();
                       p.stamp(brush, ink);
                       if (!p.end_stroke())
                         throw std::runtime_error(
                             "Asymmetric control failed to author native data");
                     };
                     draw();
                     ink.color = {.03f, .8f, .1f, .9f};
                     brush.from = brush.to = {-.5f, -.65f, .5f};
                     brush.radius = .2f;
                     brush.strength = .5f;
                     draw();
                     ink.channel = PaintChannel::Emission;
                     ink.emission = {.13f, .62f, .4f};
                     draw();
                     ink.emission_units = PaintEmissionUnits::Physical;
                     ink.emission = {123, .003f, 7000};
                     brush.from = brush.to = {-.58f, -.68f, .5f};
                     brush.radius = .15f;
                     brush.strength = .3f;
                     draw();
                     ink.emission = {8000, .002f, 3};
                     brush.from = brush.to = {-.52f, -.7f, .5f};
                     brush.radius = .11f;
                     draw();
                     ink.channel = PaintChannel::Mask;
                     ink.mask = .15f;
                     brush.from = brush.to = {-.6f, -.7f, .5f};
                     brush.radius = .17f;
                     draw();
                     ink.channel = PaintChannel::Color;
                     ink.color = {.99f, .01f, .8f, .3f};
                     brush.from = brush.to = {-.5859375f, -.7109375f, .5f};
                     brush.radius = .008f;
                     brush.strength = 1;
                     draw();
                     return true;
                   });
  const auto asymmetric_before =
      texture_set_view(asymmetric.active(), 1).painted;
  TextureSetResolutionOptions integral;
  integral.resolution = 64;
  integral.maximum_component_error = 1;
  integral.maximum_rms_error = 1;
  check(resample_texture_set(asymmetric, asymmetric.active().id, 1, integral)
            .changed,
        "Explicit measured integral control executes on asymmetric actual "
        "channels");
  oracle(*asymmetric_before, *texture_set_view(asymmetric.active(), 1).painted,
         true);
  const auto exact = scene.encode();
  const auto owner = scene.active().texture_sets;
  check(!resample_texture_set(scene, scene.active().id, 1, options).changed &&
            scene.encode() == exact && scene.active().texture_sets == owner,
        "Same-size operation is an exact owner/value no-op");
  options.resolution = 128;
  check(resample_texture_set(scene, scene.active().id, 1, options).changed,
        "The actual bilinear magnification path returns to128");
  auto unchanged = original.snapshot();
  const auto initial = unchanged.encode();
  options.resolution = 64;
  TextureSetLimits bytes;
  bytes.maximum_bytes = 1024;
  refuses(
      [&] {
        resample_texture_set(unchanged, unchanged.active().id, 1, options,
                             bytes);
      },
      "Whole-operation peak budget refuses without publication");
  check(unchanged.encode() == initial &&
            unchanged.active().texture_sets == collection,
        "Peak refusal preserves original complete aliases and authored values");
  options.resolution = 64;
  TextureSetLimits work;
  work.maximum_work = 100;
  refuses(
      [&] {
        resample_texture_set(unchanged, unchanged.active().id, 1, options,
                             work);
      },
      "Prospective work budget refuses before layer output");
  TextureSetLimits cancel;
  size_t polls = 0;
  cancel.cancelled = [&] { return ++polls > 50; };
  refuses(
      [&] {
        resample_texture_set(unchanged, unchanged.active().id, 1, options,
                             cancel);
      },
      "Cancellation aborts a real detached transfer");
  check(unchanged.encode() == initial,
        "All cancelled/private drafts leave complete Scene data unchanged");
  auto contrast = original.snapshot();
  edit_texture_set(
      contrast, contrast.active().id, 1, [](PaintedMesh &p, Material &) {
        p.add_layer("Real high-frequency quality control");
        PaintSettings ink;
        ink.front_only = false;
        ink.color = {1, 0, .2f, .3f};
        Brush brush;
        brush.from = brush.to = {.123f, -.234f, .5f};
        brush.normal = {0, 0, 1};
        brush.radius = .065f;
        p.begin_stroke();
        p.stamp(brush, ink);
        if (!p.end_stroke())
          throw std::runtime_error(
              "Quality control did not author genuine localized data");
        return true;
      });
  const auto contrast_exact = contrast.encode();
  options.maximum_component_error = 0;
  options.maximum_rms_error = 0;
  refuses(
      [&] { resample_texture_set(contrast, contrast.active().id, 1, options); },
      "Zero reconstruction tolerance refuses genuine localized channel/mask "
      "resampling loss");
  check(contrast.encode() == contrast_exact,
        "Quality refusal keeps every authored original available");
  // A real color preparation supplies independent second-color authoring
  // planes. This checks the kernel only; its authenticated prepared-grid
  // transition deliberately remains refused by the actual scene/public resize
  // operation.
  std::vector<std::array<float, 4>> colors(
      before.painted->mesh().positions.size(), {.4f, .7f, .2f, .8f});
  ColorPreparationOptions color_options;
  auto prepared = SourceColorPreparationAccess::prepare(
      *before.painted, colors, before.material, color_options);
  check(prepared.paint->color_projection() &&
            !prepared.paint->layers()[0].pbr_tiles.empty(),
        "Actual preparation authors independent PBR color planes");
  TextureSetResolutionOptions raw;
  raw.resolution = 64;
  TextureSetResolutionReport result;
  auto layers =
      detail::resample_texture_set_layers(*prepared.paint, raw, {}, result);
  check(layers.size() == prepared.paint->layers().size() &&
            !layers[0].pbr_tiles.empty() &&
            std::any_of(layers[0].pbr_tiles.begin(), layers[0].pbr_tiles.end(),
                        [](const auto &p) { return bool(p); }),
        "Measured kernel carries genuine independent PBR planes without "
        "flattening");
  refuses(
      [&] {
        detail::TextureSetPaintAccess::resample_plain(*prepared.paint, raw, {},
                                                      result);
      },
      "Prepared PBR projection cannot borrow a plain writable-grid transition");
}
// Actual public SpatialFrame input, using the established controller fixture's
// radial selection and tracked panel rays; no direct private control callback.
struct ResizeController {
  Document &doc;
  SpatialFrame frame;
  explicit ResizeController(Document &d) : doc(d) {
    frame.sequence = 1;
    frame.time = 1;
    frame.focused = frame.head_valid = true;
    frame.head.position = {0, 1.6f, 0};
    frame.hands[1].tracked.valid = true;
    frame.hands[1].tracked.grip.position = {0, 1.48f, -.5f};
    send();
  }
  void send(float x = 0, float y = 0, bool click = false, float pressure = 0) {
    ++frame.sequence;
    frame.time += .05;
    auto &h = frame.hands[1];
    h.x = x;
    h.y = y;
    h.joystick = click;
    h.tracked.trigger = pressure;
    doc.submit(frame);
    check(doc.wait_input(),
          "Actual controller input reaches its publication fence");
  }
  void sector(int i) {
    const float a = i * 3.14159265358979323846f / 6;
    send(std::cos(a), std::sin(a));
    send(std::cos(a), std::sin(a), true);
    send();
  }
  void open() {
    doc.cancel_spatial_menu();
    send();
    send(0, 0, true);
    send();
    sector(9);
    sector(1);
    check(doc.frame().state->menu_page == 31,
          "Actual controller discovers the material-set page");
  }
  void ray(float t, bool button = false, bool accept = false) {
    const auto s = doc.frame().state;
    const float u = (button ? (accept ? 470.f : 170.f) : (100 + 420 * t)) / 640,
                v = (button ? 609.f : 300.f) / 640;
    auto &h = frame.hands[1].tracked;
    h.aimValid = true;
    h.aim.position =
        ob::transform(s->panel, {(u - .5f) * .6f, (.5f - v) * .6f, .25f});
    h.aim.orientation = s->panel.orientation;
    send(0, 0, false, .8f);
    send();
    h.aimValid = false;
  }
};
void native() {
  auto scene = fixture();
  const auto initial = scene.encode();
  Document d;
  d.replace(scene);
  settle(d);
  TextureSetEdit edit;
  edit.action = TextureSetAction::Resolution;
  edit.set = 1;
  edit.resolution = 256;
  const auto before = d.frame().state;
  const AssetSource captured{before->generation, before->revision,
                             before->active_object, before->edit_authority};
  d.edit_texture_set(edit, captured);
  d.cancel_asset_work();
  check(d.wait_asset(std::chrono::seconds(120)),
        "Actual cancelled resize worker terminates");
  check(d.copy_scene().encode() == initial && !d.frame().state->can_undo,
        "Worker Cancel leaves original grid and global history unchanged");
  settle(d);
  d.edit_texture_set(edit);
  auto saving = d.save_async("resized-field.dsculpt");
  settle(d);
  saving.get();
  const auto accepted = d.copy_scene();
  check(accepted.active().texture_sets->at(1).painted->resolution() == 256 &&
            accepted.active().painted->resolution() == 64,
        "Actual worker resizes only the requested active material field");
  check(Scene::load("resized-field.dsculpt").encode() == accepted.encode(),
        "Immediate ordered native Save/fresh reopen retains every resulting "
        "plane exactly");
  check(d.undo() && d.copy_scene().encode() == initial,
        "One global Undo restores exact original grids and authored channels");
  check(d.redo() && d.copy_scene().encode() == accepted.encode(),
        "One global Redo restores the entire accepted field endpoint");
  const auto exact = d.copy_scene().encode();
  refuses([&] { d.edit_texture_set(edit, captured); },
          "Stale captured resolution request refuses before later owner "
          "publication");
  check(d.copy_scene().encode() == exact,
        "Stale native request retains the accepted endpoint");
  d.configure_selection(false);
  ResizeController hand(d);
  hand.open();
  hand.sector(6);
  check(d.frame().state->controls.parameter == VrParameter::TextureResolution,
        "Actual XR panel opens the selected field's staged resolution");
  hand.ray(0);
  check(d.copy_scene().encode() == exact,
        "Moving an actual XR ray changes no authored field");
  hand.ray(0, true, false);
  check(d.copy_scene().encode() == exact &&
            d.frame().state->controls.parameter == VrParameter::None,
        "Actual XR Cancel retains every plane and closes staged resize");
  hand.sector(6);
  hand.ray(0);
  hand.ray(0, true, true);
  auto saved = d.save_async("xr-resized-field.dsculpt");
  settle(d);
  saved.get();
  const auto endpoint = d.copy_scene().encode();
  check(d.copy_scene().active().texture_sets->at(1).painted->resolution() ==
                64 &&
            Scene::load("xr-resized-field.dsculpt").encode() == endpoint,
        "Actual XR Apply and immediately ordered Save publish the complete "
        "resized field");
  check(d.undo() && d.copy_scene().encode() == exact && d.redo() &&
            d.copy_scene().encode() == endpoint,
        "Actual XR resize has one exact complete Undo/Redo endpoint");
  hand.open();
  hand.sector(6);
  hand.ray(.4f);
  hand.frame.focused = false;
  hand.send();
  check(d.copy_scene().encode() == endpoint &&
            d.frame().state->controls.parameter == VrParameter::None,
        "Actual XR focus loss cancels staged resizing without a worker "
        "publication");
}
} // namespace
int main(int argc, char **argv) try {
  if (argc > 1) {
    std::filesystem::create_directories(argv[1]);
    std::filesystem::current_path(argv[1]);
  }
  numeric();
  native();
  std::cout << "PASS " << checks << " measured texture resolution checks\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
