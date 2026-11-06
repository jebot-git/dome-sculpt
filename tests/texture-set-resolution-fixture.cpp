// SPDX-License-Identifier: GPL-3.0-or-later
// Real native producers and expected endpoints for external FLTK/OpenXR tests.
// No product observer, alternate editor or shortcut renderer is linked.
#define main unused_texture_resolution_native_suite
#include "texture-set-resolution.cpp"
#undef main
#include <fstream>
namespace {
nlohmann::json measured(const TextureSetResolutionReport &r) {
  auto layers = nlohmann::json::array();
  for (const auto &q : r.layers)
    layers.push_back({{"id", q.layer},
                      {"raw", q.value_error},
                      {"contribution", q.contribution_error},
                      {"pbr", q.pbr_error},
                      {"pbr_raw", q.pbr_value_error},
                      {"float", q.emission_relative_error},
                      {"float_raw", q.emission_value_error},
                      {"physical_absolute", q.physical_absolute_error},
                      {"rms", q.rms_error}});
  return {{"changed", r.changed},
          {"from", r.original_resolution},
          {"to", r.resolution},
          {"peak_bytes", r.peak_bytes},
          {"work", r.work},
          {"material", r.material_error},
          {"material_rms", r.material_rms_error},
          {"layers", layers}};
}
void primary_planes(Scene &s) {
  edit_texture_set(s, s.active().id, 0, [](PaintedMesh &p, Material &) {
    PaintSettings ink;
    ink.color = {.64f, .12f, .33f, .7f};
    stamp(p, ink, .6f);
    ink.channel = PaintChannel::Metallic;
    ink.metallic = .31f;
    stamp(p, ink, .5f);
    ink.channel = PaintChannel::Roughness;
    ink.roughness = .67f;
    stamp(p, ink, .8f);
    ink.channel = PaintChannel::Emission;
    ink.emission = {.15f, .4f, .09f};
    stamp(p, ink, .4f);
    ink.emission_units = PaintEmissionUnits::Physical;
    ink.emission = {23, .001f, 1200};
    stamp(p, ink, .2f);
    ink.channel = PaintChannel::Mask;
    ink.mask = .42f;
    stamp(p, ink);
    p.add_layer("Primary hidden authoring");
    ink = {};
    ink.color = {.29f, .57f, .13f, .8f};
    stamp(p, ink, .35f);
    p.layer_properties("Primary hidden authoring", false, 0);
    return true;
  });
}
} // namespace
int main(int argc, char **argv) try {
  if (argc != 2)
    return 2;
  const auto out = std::filesystem::absolute(argv[1]);
  std::filesystem::create_directories(out);
  auto rich = fixture();
  primary_planes(rich);
  rich.validate();
  const auto rich_wire = rich.encode();
  const auto sibling = rich.active().painted;
  const auto original_domain = rich.active().texture_sets->at(1).domain;
  nlohmann::json manifest = {
      {"version", 1},
      {"scope",
       "Actual native producer inputs; exact API endpoints plus measured "
       "quality-refusal witness; no runtime/UI qualification"},
      {"files", nlohmann::json::object()},
      {"reports", nlohmann::json::object()}};
  auto save = [&](const char *key, const Scene &s) {
    const auto name = std::string(key) + ".dsculpt";
    s.save(out / name);
    check(Scene::load(out / name).encode() == s.encode(),
          "Generated workflow fixture saves and reopens exact native fields");
    manifest["files"][key] = name;
  };
  save("rich-extra128", rich);
  auto endpoint = rich.snapshot();
  TextureSetResolutionOptions options;
  options.resolution = 64;
  const auto r64 =
      resample_texture_set(endpoint, endpoint.active().id, 1, options);
  check(
      endpoint.active().texture_sets->at(1).domain == original_domain,
      "Native expected endpoint preserves actual member-domain control block");
  manifest["reports"]["extra64"] = measured(r64);
  save("extra64", endpoint);
  endpoint = rich.snapshot();
  options.resolution = 256;
  manifest["reports"]["extra256"] = measured(
      resample_texture_set(endpoint, endpoint.active().id, 1, options));
  save("extra256", endpoint);
  auto primary = rich.snapshot();
  select_texture_set(primary, primary.active().id, 0);
  save("rich-primary64", primary);
  endpoint = primary.snapshot();
  options.resolution = 128;
  manifest["reports"]["primary128"] = measured(
      resample_texture_set(endpoint, endpoint.active().id, 0, options));
  save("primary128", endpoint);
  auto contrast = rich.snapshot();
  edit_texture_set(
      contrast, contrast.active().id, 1, [](PaintedMesh &p, Material &) {
        p.add_layer("Visible quality refusal control");
        PaintSettings ink;
        ink.front_only = false;
        ink.color = {.99f, .01f, .8f, .3f};
        Brush b;
        b.from = b.to = {-.5859375f, -.7109375f, .5f};
        b.normal = {0, 0, 1};
        b.radius = .008f;
        b.strength = 1;
        p.begin_stroke();
        p.stamp(b, ink);
        if (!p.end_stroke())
          throw std::runtime_error(
              "Single-texel quality witness was not actually authored");
        return true;
      });
  save("quality-source", contrast);
  const auto exact = contrast.encode();
  options.resolution = 64;
  bool rejected = false;
  std::string reason;
  try {
    resample_texture_set(contrast, contrast.active().id, 1, options);
  } catch (const std::runtime_error &e) {
    reason = e.what();
    rejected = reason.find("reconstruction bounds") != std::string::npos;
  }
  check(rejected && contrast.encode() == exact,
        "Default policy refuses the measured localized authored data without "
        "publication");
  auto loss = contrast.snapshot();
  options.maximum_component_error = 1;
  options.maximum_rms_error = 1;
  const auto loss_report =
      resample_texture_set(loss, loss.active().id, 1, options);
  double worst = 0, rms = 0;
  for (const auto &q : loss_report.layers) {
    for (double v : q.value_error)
      worst = std::max(worst, v);
    for (double v : q.contribution_error)
      worst = std::max(worst, v);
    for (double v : q.pbr_error)
      worst = std::max(worst, v);
    for (double v : q.emission_relative_error)
      worst = std::max(worst, v);
    rms = std::max(rms, q.rms_error);
  }
  check(worst > .30 || rms > .08,
        "Loose explicit control independently exposes a measured error beyond "
        "the default acceptance bound");
  save("quality-loose64", loss);
  manifest["quality"] = {{"reason", reason},
                         {"measured_maximum", worst},
                         {"measured_rms", rms},
                         {"report", measured(loss_report)}};
  check(rich.encode() == rich_wire && rich.active().painted == sibling,
        "Producing expected endpoints never changes the complete original or "
        "sibling owner");
  const auto field = texture_set_view(rich.active(), 1);
  const auto tiny =
      field.painted->emission_pixel(field.painted->active_layer(), 20, 20);
  manifest["authoring"] = {
      {"primary_resolution", rich.active().painted->resolution()},
      {"extra_resolution", field.painted->resolution()},
      {"primary_layers", rich.active().painted->layers().size()},
      {"extra_layers", field.painted->layers().size()},
      {"tiny_float_coverage", tiny.coverage},
      {"tiny_physical_radiance", tiny.physical},
      {"common_fallback_coverage",
       field.painted->pixel(field.painted->active_layer(), 20, 20).data[12]}};
  {
    std::ofstream file(out / "manifest.json");
    file << manifest.dump(2) << '\n';
  }
  std::cout << "PASS " << checks
            << " actual workflow fixture production checks\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
