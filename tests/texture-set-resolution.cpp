// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/texture-set-resolution.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/document.hpp"
#include "dome/material-bindings.hpp"
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
  // Use the actual native brush bound; radius5 covers this full two-unit quad
  // while retaining genuine radial coverage instead of an invalid flat stamp.
  b.radius = 5;
  b.strength = strength;
  p.begin_stroke();
  p.stamp(b, settings);
  check(p.end_stroke(), "Actual native stamp authors the requested plane");
}
Scene fixture(bool separate_materials = false) {
  Mesh m;
  m.positions = {{-1, -1, 0},   {1, -1, 0},   {1, 1, 0},   {-1, 1, 0},
                 {-1, -1, .5f}, {1, -1, .5f}, {1, 1, .5f}, {-1, 1, .5f}};
  if (separate_materials) {
    for (size_t i = 0; i < m.positions.size(); ++i) {
      m.positions[i].x = .44f * m.positions[i].x + (i < 4 ? -.49f : .49f);
      m.positions[i].y *= .8f;
    }
  }
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
// Independent long-double composition from actual raw layers. This does not
// call the production overlay evaluator, preview or quality implementation.
void emission_oracle(const PaintedMesh &before, const PaintedMesh &after,
                     const Material &material,
                     const TextureSetResolutionReport &report) {
  const auto at = [&](const PaintedMesh &paint, int x, int y) {
    const auto n = int(paint.resolution());
    x = std::clamp(x, 0, n - 1);
    y = std::clamp(y, 0, n - 1);
    std::array<long double, 4> result{};
    for (size_t k = 0; k < 3; ++k)
      result[k] = material.emissive[k];
    for (const auto &layer : paint.layers()) {
      if (!layer.visible || layer.opacity == 0)
        continue;
      const auto common = paint.pixel(layer.id, x, y);
      const auto value = paint.emission_pixel(layer.id, x, y);
      const long double coverage = value.coverage *
          static_cast<long double>(common.data[13]) / 255 * layer.opacity;
      for (size_t k = 0; k < 3; ++k)
        result[k] = result[k] * (1 - coverage) +
            (static_cast<long double>(value.legacy[k]) + value.physical[k]) * coverage;
      result[3] = result[3] * (1 - coverage) + coverage;
    }
    return result;
  };
  long double peak = 1;
  const auto n = before.resolution();
  for (uint32_t y = 0; y < n; ++y)
    for (uint32_t x = 0; x < n; ++x) {
      const auto value = at(before, x, y);
      for (size_t k = 0; k < 3; ++k)
        peak = std::max(peak, value[k]);
    }
  std::array<long double, 4> maximum{};
  for (uint32_t y = 0; y < n; ++y)
    for (uint32_t x = 0; x < n; ++x) {
      const long double u = (static_cast<long double>(x) + .5L) * after.resolution() / n - .5L,
                        v = (static_cast<long double>(y) + .5L) * after.resolution() / n - .5L;
      const int bx = int(floorl(u)), by = int(floorl(v));
      std::array<long double, 4> reconstructed{};
      for (int corner = 0; corner < 4; ++corner) {
        const auto value = at(after, bx + (corner & 1), by + ((corner >> 1) & 1));
        const long double weight = ((corner & 1) ? u - bx : 1 - (u - bx)) *
            ((corner & 2) ? v - by : 1 - (v - by));
        for (size_t k = 0; k < 4; ++k)
          reconstructed[k] += weight * value[k];
      }
      const auto actual = at(before, x, y);
      for (size_t k = 0; k < 4; ++k)
        maximum[k] = std::max(maximum[k], fabsl(actual[k] - reconstructed[k]) /
                                                    (k < 3 ? peak : 1));
    }
  for (size_t k = 0; k < 4; ++k)
    check(fabsl(report.material_error[8 + k] - maximum[k]) < 2e-6L,
          "Independent visible layer/mask/FLOAT radiance integral matches actual material quality");
  check(maximum[0] > 0 && maximum[0] < .30L &&
            fabsl(report.material_error[13] - maximum[3]) < 2e-6L,
        "Actual nonzero physical reconstruction error and independent FLOAT coverage are measured");
  std::cout << "ORACLE linear_emission peak=" << double(peak)
            << " relative_rgb=" << double(maximum[0]) << ',' << double(maximum[1])
            << ',' << double(maximum[2]) << " float_coverage=" << double(maximum[3])
            << '\n';
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
  emission_oracle(*before.painted, *field.painted, before.material, report);
  auto luminous = fixture();
  edit_texture_set(luminous, luminous.active().id, 1,
                   [](PaintedMesh &, Material &material) {
    material.emissive = {500, .125f, 3};
    return true;
  });
  const auto luminous_before = texture_set_view(luminous.active(), 1);
  const auto luminous_report = resample_texture_set(
      luminous, luminous.active().id, 1, options);
  check(luminous_report.changed,
        "Actual mixed FLOAT/legacy paint resizes over an authored HDR background");
  emission_oracle(*luminous_before.painted,
                 *texture_set_view(luminous.active(), 1).painted,
                 luminous_before.material, luminous_report);
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
  auto pbr_scene = fixture();
  TextureSetResolutionOptions pbr_grid;
  pbr_grid.resolution = 128;
  check(resample_texture_set(pbr_scene, pbr_scene.active().id, 0, pbr_grid).changed,
        "Independent PBR control receives a real128 native primary grid");
  edit_texture_set(pbr_scene, pbr_scene.active().id, 0,
                   [](PaintedMesh &paint, Material &) {
    PaintSettings ink;
    ink.color = {.4f, .7f, .2f, .8f};
    stamp(paint, ink, .7f);
    ink.channel = PaintChannel::Metallic;
    ink.metallic = .31f;
    stamp(paint, ink, .6f);
    ink.channel = PaintChannel::Roughness;
    ink.roughness = .67f;
    stamp(paint, ink, .8f);
    ink.channel = PaintChannel::Emission;
    ink.emission = {.15f, .4f, .09f};
    stamp(paint, ink, .4f);
    ink.channel = PaintChannel::Mask;
    ink.mask = .42f;
    stamp(paint, ink);
    return true;
  });
  const auto pbr_before = texture_set_view(pbr_scene.active(), 0);
  std::vector<std::array<float, 4>> colors(
      pbr_before.painted->mesh().positions.size(), {.4f, .7f, .2f, .8f});
  ColorPreparationOptions color_options;
  auto prepared = SourceColorPreparationAccess::prepare(
      *pbr_before.painted, colors, pbr_before.material, color_options);
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
  size_t distinct_pbr = 0;
  for (size_t li = 0; li < prepared.paint->layers().size(); ++li) {
    const auto &input = prepared.paint->layers()[li];
    const auto &output = layers[li];
    for (uint32_t y : {0u, 9u, 31u, 63u})
      for (uint32_t x : {0u, 13u, 42u, 63u}) {
        const auto target_tile = size_t(y / 32) * 2 + x / 32;
        const auto target_pixel = size_t(y % 32) * 32 + x % 32;
        check(bool(output.pbr_tiles.at(target_tile)),
              "Actual independent PBR target tile exists at each analytic sample");
        const auto &actual = output.pbr_tiles[target_tile]->pixels[target_pixel];
        for (size_t k = 0; k < 4; ++k) {
          long double integral = 0, coverage = 0;
          for (uint32_t dy = 0; dy < 2; ++dy)
            for (uint32_t dx = 0; dx < 2; ++dx) {
              const auto sx = x * 2 + dx, sy = y * 2 + dy;
              const auto tile = size_t(sy / 32) * 4 + sx / 32;
              const auto pixel = size_t(sy % 32) * 32 + sx % 32;
              check(bool(input.tiles.at(tile)) && bool(input.pbr_tiles.at(tile)),
                    "Independent PBR oracle reads actual authored source planes");
              const auto &common = input.tiles[tile]->pixels[pixel];
              const auto &value = input.pbr_tiles[tile]->pixels[pixel];
              distinct_pbr += value[k] != common.data[k];
              coverage += common.data[9];
              integral += (k < 3 ? decode(value[k]) :
                           static_cast<long double>(value[k]) / 255) * common.data[9];
            }
          check(coverage > 0,
                "Independent PBR area control has actual authored color coverage");
          const auto expected = k < 3 ? encode(integral / coverage) :
              int(llroundl(integral / coverage * 255));
          check(std::abs(int(actual[k]) - expected) <= 1,
                "Independent linear-light PBR RGBA area integral matches kernel output");
        }
      }
  }
  check(distinct_pbr > 0,
        "Actual prepared PBR planes differ from common color at analytic samples");
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

void baked_member(bool typed_mtoon) {
  auto scene=fixture(true);
  const auto target=scene.active().id;
  const auto solid=[](uint32_t resolution,std::array<uint8_t,4> pixel){
    Image image{resolution,resolution,std::vector<uint8_t>(size_t(resolution)*resolution*4)};
    for(size_t at=0;at<image.rgba.size();at+=4)
      std::copy(pixel.begin(),pixel.end(),image.rgba.begin()+at);
    return image;
  };
  edit_texture_set(scene,target,0,[&](PaintedMesh &paint,Material &){
    BakedTextures bake;
    bake.maps.color=solid(64,{82,221,129,255});
    bake.maps.metal_rough=solid(64,{213,115,84,255});
    bake.maps.normal=solid(64,{126,151,246,255});
    bake.maps.emission=solid(64,{81,115,47,255});
    bake.coverage.assign(64*64,190);
    paint.install_bake(std::move(bake));
    return true;
  });
  if (typed_mtoon) {
    const auto current = active_texture_set(scene.active()).material;
    SourceMaterialBindings binding;
    binding.material = MToonMaterial::from_vrm1({
        {"pbrMetallicRoughness", {{"baseColorFactor", current.color}, {"baseColorTexture", {{"index", 0}}}}},
        {"normalTexture", {{"index", 1}}}, {"alphaMode", "OPAQUE"},
        {"emissiveFactor", current.emissive},
        {"extensions", {{"VRMC_materials_mtoon", {{"specVersion", "1.0"},
                           {"shadeMultiplyTexture", {{"index", 2}}}}}}}
    }, 3);
    binding.native_reference={current.color,current.emissive,int(current.alpha),current.alpha_cutoff,current.double_sided};
    auto &base = binding.roles[size_t(MToonSlot::Base)];
    base.image = std::make_shared<const Image>(Image{2,1,{153,133,94,255,153,133,94,255}});
    binding.roles[size_t(MToonSlot::Shade)].image =
        std::make_shared<const Image>(Image{2,1,{114,143,178,255,140,155,164,255}});
    binding.roles[size_t(MToonSlot::Normal)].image =
        std::make_shared<const Image>(Image{1,1,{147,156,248,255}});
    check(bind_texture_set_mtoon(scene,target,1,binding),"Bind independent typed base, shade and normal images");
  }
  Mesh mesh;
  mesh.positions={{.05f,-.8f,.53f},{.93f,-.8f,.53f},{.93f,.8f,.53f},{.05f,.8f,.53f}};
  mesh.normals.assign(4,{0,0,1});
  mesh.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};
  mesh.indices={0,1,2,0,2,3};
  const auto source=scene.add("Hidden bake source",Volume{});
  auto &donor=scene.object(source);
  donor.mesh_active=true;donor.visible=false;
  donor.material.emissive={3.8f,.46f,.25f};
  donor.painted=std::make_shared<PaintedMesh>(std::move(mesh),64);
  BakedTextures source_maps;
  source_maps.maps.color=solid(64,{176,58,202,225});
  source_maps.maps.metal_rough=solid(64,{200,130,52,255});
  source_maps.maps.normal=solid(64,{157,137,248,255});
  source_maps.coverage.assign(64*64,182);
  source_maps.emission_field=BakedEmissionField::generate_compact(64,[](size_t at){
    const float a=.0012f+float(at%64)*.00001f;
    return BakedEmissionSample{{a*.25f,a*.1f,0},{a*6800,a*50,a*3},a};
  });
  donor.painted->install_bake(std::move(source_maps));
  scene.select(target);
  Document doc;
  doc.replace(scene);
  settle(doc);
  ObjectBakeSettings settings;
  settings.front_mm=settings.back_mm=50;
  settings.ao_samples=0;
  settings.padding=0;
  doc.bake_from_object(source,settings);
  doc.save_async("baked-member-128.dsculpt").get();
  settle(doc);
  const auto baked=doc.copy_scene();
  const auto field=active_texture_set(baked.active());
  const auto sibling=texture_set_view(baked.active(),0);
  check(bool(field.painted->baked()),"The selected material has a Bake after projection");
  const auto &old=*field.painted->baked();
  check(field.id==1&&field.painted->resolution()==128&&old.emission_field&&
        !old.maps.normal.rgba.empty()&&field.bake_recipe&&field.bake_recipe->source.object==source,
        "The real Bake worker publishes a canonical FLOAT Bake and recipe on the additional member");
  const auto before=baked.encode();
  auto denied=baked;
  TextureSetLimits limits;
  limits.maximum_bytes=1;
  refuses([&]{resample_texture_set(denied,target,1,{256},limits);},"Insufficient bytes refuse before publishing a baked member");
  check(denied.encode()==before,"Refusal retains the original collection and its complete baked field");
  ResizeController hand(doc);
  hand.open();hand.sector(6);hand.ray(.4f);hand.ray(.4f,true,false);
  check(doc.copy_scene().encode()==before,"Controller Cancel leaves all baked members unchanged");
  hand.sector(6);hand.ray(.4f);hand.ray(.4f,true,true);
  doc.save_async("baked-member-256.dsculpt").get();
  settle(doc);
  const auto resized=doc.copy_scene();
  const auto current=active_texture_set(resized.active());
  check(bool(current.painted->baked()),"The resized native material retains a Bake");
  const auto &grown=*current.painted->baked();
  check(current.painted->resolution()==256&&current.domain==field.domain&&current.bake_recipe==field.bake_recipe&&
        current.painted->mesh_owner()==field.painted->mesh_owner(),
        "Resize retains the selected face domain, Bake recipe and mesh owner");
  if (typed_mtoon)
    check(current.material_bindings==field.material_bindings&&current.material_bindings->has_mtoon&&
          current.material_bindings->roles[size_t(MToonSlot::Base)].image->width==2,
          "MToon source roles keep their exact binding, image and sampler owners");
  const auto retained_sibling=texture_set_view(resized.active(),0);
  check(retained_sibling.domain==sibling.domain&&
        detail::TextureSetPaintAccess::same_current(*retained_sibling.painted,*sibling.painted)&&
        detail::TextureSetPaintAccess::same_current(*resized.object(source).painted,*baked.object(source).painted),
        "The sibling and hidden donor keep their complete current paint and Bake state");
  bool equal=true;
  for(uint32_t y=0;y<256&&equal;++y)for(uint32_t x=0;x<256&&equal;++x){
    const size_t src=size_t(y/2)*128+x/2,dst=size_t(y)*256+x;
    equal=grown.coverage[dst]==old.coverage[src];
    for(auto [a,b]:{std::pair{&old.maps.color,&grown.maps.color},
        {&old.maps.normal,&grown.maps.normal},{&old.maps.metal_rough,&grown.maps.metal_rough}})
      equal&=std::equal(a->rgba.data()+4*src,a->rgba.data()+4*src+4,b->rgba.data()+4*dst);
    const auto a=old.emission_field->texel(src),b=grown.emission_field->texel(dst);
    equal&=a.coverage==b.coverage&&a.absolute==b.absolute&&a.legacy==b.legacy;
    for(const auto &layer:field.painted->layers())
      equal&=current.painted->pixel(layer.id,x,y)==field.painted->pixel(layer.id,x/2,y/2)&&
        current.painted->emission_pixel(layer.id,x,y)==field.painted->emission_pixel(layer.id,x/2,y/2);
  }
  check(equal,"Every common, normal, independent FLOAT and authored layer cell survives member enlargement");
  const auto exact=resized.encode();
  check(Scene::load("baked-member-256.dsculpt").encode()==exact&&doc.undo()&&doc.copy_scene().encode()==before&&
        doc.redo()&&doc.copy_scene().encode()==exact,"Ordered Save and one Undo/Redo preserve complete baked member history");
  settle(doc);
  doc.export_asset("baked-member-standard.glb",AssetExportFormat::Glb).get();
  doc.export_asset("baked-member-editable.glb",AssetExportFormat::EditableGlb256).get();
  check(Scene::load("baked-member-editable.glb").encode()==exact,"Editable GLB restores the selected grid and both material sets exactly");

  auto reduced=resized;
  const auto report=resample_texture_set(reduced,target,1,{128});
  const auto reduced_field=active_texture_set(reduced.active());
  check(bool(reduced_field.painted->baked()),"The reduced native material retains a Bake");
  const auto &restored=*reduced_field.painted->baked();
  equal=report.changed&&restored.coverage==old.coverage&&restored.maps.color.rgba==old.maps.color.rgba&&
        restored.maps.metal_rough.rgba==old.maps.metal_rough.rgba&&restored.maps.normal.rgba==old.maps.normal.rgba;
  for(size_t i=0;i<old.emission_field->sample_count()&&equal;++i){
    const auto a=old.emission_field->texel(i),b=restored.emission_field->texel(i);
    equal=a.coverage==b.coverage&&a.legacy==b.legacy&&a.absolute==b.absolute;
  }
  check(equal&&reduced_field.bake_recipe==field.bake_recipe,"Reducing the exact enlargement recovers every baked texel and emission coefficient");
  auto primary=resized;
  check(resample_texture_set(primary,target,0,{128}).changed&&primary.active().painted->resolution()==128&&
        primary.active().painted->baked()->maps.emission.width==128&&texture_set_view(primary.active(),1).painted==current.painted,
        "The primary RGB8 Bake resizes without replacing the additional FLOAT member");
}

void typed_source_grid() {
  auto mesh = fixture(true).active().painted->mesh();
  for (size_t i = 0; i < mesh.uv.size(); ++i)
    mesh.uv[i][0] = .46f * mesh.uv[i][0] + (i < 4 ? .01f : .53f);
  auto maps = std::make_shared<PaintSourceMaps>();
  maps->color.image = std::make_shared<const Image>(Image{2,1,{104,172,164,255,146,181,182,255}});
  Scene scene;
  auto &object = scene.active();
  const auto id = object.id;
  object.mesh_active = true;
  object.painted = PaintedMesh::from_maps(std::move(mesh), maps, 64);
  SourceMaterialBindings binding;
  binding.material = MToonMaterial::from_vrm1({
      {"pbrMetallicRoughness", {{"baseColorTexture", {{"index", 0}}}}},
      {"extensions", {{"VRMC_materials_mtoon", {{"specVersion", "1.0"}}}}}
  }, 1);
  binding.roles[size_t(MToonSlot::Base)].image = maps->color.image;
  object.material_bindings = std::make_shared<const SourceMaterialBindings>(std::move(binding));
  const auto set = create_texture_set(scene, id, std::array<uint64_t,2>{3,4}, "Independent UV0 images", 64);
  edit_texture_set(scene,id,set,[](PaintedMesh &paint,Material &) {
    PaintSettings ink;
    ink.color={.45f,.31f,.17f,1};
    stamp(paint,ink,.42f);
    return true;
  });
  const auto before=active_texture_set(scene.active());
  check(before.id==set && before.painted->source_maps() && !before.source,
        "The native MToon partition has independent generic UV0 sources and real paint");
  check(resample_texture_set(scene,id,set,{128}).changed,"Native MToon with independent core images resizes");
  const auto resized=active_texture_set(scene.active());
  check(resized.domain==before.domain && resized.material_bindings==before.material_bindings &&
        resized.painted->source_maps()==before.painted->source_maps() &&
        resized.painted->resolution()==128 && texture_set_view(scene.active(),0).painted->resolution()==64,
        "Resize keeps the actual independent UV0 image collection, roles and sibling grid");
  scene.save("mtoon-core-images.dsculpt");
  check(Scene::load("mtoon-core-images.dsculpt").encode()==scene.encode(),
        "Native reopen preserves independent core images and the resized writable grid");
}
} // namespace
int main(int argc, char **argv) try {
  if (argc > 1) {
    std::filesystem::create_directories(argv[1]);
    std::filesystem::current_path(argv[1]);
  }
  if(argc==3&&std::string_view(argv[2])=="baked-mtoon"){baked_member(true);typed_source_grid();}
  else if(argc==3&&std::string_view(argv[2])=="baked")baked_member(false);
  else {numeric();native();}
  std::cout << "PASS " << checks << " measured texture resolution checks\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
  return 1;
}
