// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Included after the advanced native fixture and its independent decoder.
// This suite exercises public USE of a genuinely authored advanced source;
// authoring SourceUV through the UI has its own separate scenario/qualification.
namespace advanced_public {
using Json = nlohmann::json;
inline unsigned checks = 0;
void require(bool value, const char *why) {
  if (!value) throw std::runtime_error(why);
  ++checks;
}
// Inspect geometry directly: retained normal payloads require Scene-owned archives.
bool same_positions(const Mesh &a, const Mesh &b) {
  return std::equal(a.positions.begin(), a.positions.end(),
                    b.positions.begin(), b.positions.end(),
                    [](Vec3 p, Vec3 q) { return p.x == q.x && p.y == q.y && p.z == q.z; });
}
const char *family(SourceNormalUvMethod method) {
  return method == SourceNormalUvMethod::Rasterize ? "raster" : "retained";
}
const char *method_name(SourceNormalUvMethod method) {
  return method == SourceNormalUvMethod::Rasterize ? "Rasterize" : "RetainSource";
}
void advanced_authority(const SculptObject &source, SourceNormalUvMethod method) {
  require(source.painted && source.painted->normal_projection() &&
          source.material_bindings && source.material_bindings->normal_preparation,
          "Advanced public source owns genuine first-Bake normal authority");
  const auto &p = *source.material_bindings->normal_preparation;
  if (method == SourceNormalUvMethod::Rasterize)
    require(p.uv_reprojection && !p.sampling_domain &&
            source.painted->normal_projection()->version == 1,
            "Rasterize public source owns its actual ordered-corner UV replay recipe");
  else
    require(p.sampling_domain && !p.uv_reprojection &&
            source.painted->normal_projection()->version == 2 &&
            source.painted->normal_projection()->sampling_domain,
            "RetainSource public source owns actual v2 independent normal coordinates");
}
prepared_public::Case make_case(bool legacy, bool posed, SourceNormalUvMethod method) {
  // Keep the qualified first-Bake geometry, retained Original, source IDs and
  // default public source ordering; change only the genuinely authored source.
  auto c = prepared_public::make_case(legacy, posed);
  const auto original = c.before.object(c.original).painted;
  const auto original_binding = c.before.object(c.original).material_bindings;
  const auto original_data = c.before.encode().at("objects").front();
  auto &source = c.before.object(c.prepared);
  const auto first_recipe = source.bake_recipe;
  auto adopted = advanced_fixture::uv_adoption(source, method, false);
  source.painted = std::move(adopted.paint);
  source.material_bindings = std::move(adopted.material_bindings);
  source.deformation = std::move(adopted.deformation);
  source.appearance = std::move(adopted.appearance);
  source.name = std::string(method_name(method)) + (legacy ? " MT0 " : " MT1 ") +
                (posed ? "posed" : "rest");
  // A distinct target with corresponding geometry/current UVs makes the normal
  // oracle independent of world-to-triangle projection and numerical heuristics.
  c.before.object(c.destination).painted =
      std::make_shared<PaintedMesh>(source.painted->mesh(), 64);
  c.before.select(c.destination);
  c.before.validate();
  advanced_authority(source, method);
  require(source.bake_recipe == first_recipe && first_recipe &&
          first_recipe->source.object == c.original &&
          c.before.object(c.original).painted == original &&
          c.before.object(c.original).material_bindings == original_binding &&
          c.before.encode().at("objects").front() == original_data,
          "Actual SourceUV adoption retains Original and the exact genuine first-Bake recipe");
  require(Scene::decode(c.before.encode()).encode() == c.before.encode(),
          "Advanced public before capsule preserves its complete native owning UV graph");
  Document document;
  document.replace(c.before.snapshot());
  require(document.wait_mesh() && document.frame().state->can_bake &&
          document.frame().state->bake_source == c.original &&
          std::find(document.frame().state->bake_sources.begin(),
                    document.frame().state->bake_sources.end(), c.prepared) !=
              document.frame().state->bake_sources.end(),
          "Advanced public UI initially offers retained Original first and genuinely offers Prepared");
  return c;
}
Json normal_oracle(const Scene &scene, uint64_t source_id, uint64_t target_id,
                   SourceNormalUvMethod method) {
  const auto &source = scene.object(source_id);
  const auto &destination = scene.object(target_id);
  advanced_authority(source, method);
  require(destination.painted->normal_projection() && destination.painted->baked(),
          "Actual advanced second Bake produces independent target normal planes");
  const auto &a = source.painted->mesh();
  const auto &b = destination.painted->mesh();
  require(same_positions(a, b) &&
          a.indices == b.indices && a.uv == b.uv,
          "Advanced public oracle uses exact ordered corresponding geometry and current UVs");
  const auto &mesh = destination.painted->mesh();
  std::array<double, 2> maximum{}, wrong_domain{}, omitted{}, swapped{};
  size_t interior = 0;
  for (size_t y = 0; y < 64; ++y) for (size_t x = 0; x < 64; ++x) {
    const std::array<double, 2> q{(x + .5) / 64., (y + .5) / 64.};
    for (size_t tri = 0; tri < mesh.indices.size() / 3; ++tri) {
      const auto p = mesh.uv[mesh.indices[tri * 3]];
      const auto r = mesh.uv[mesh.indices[tri * 3 + 1]];
      const auto s = mesh.uv[mesh.indices[tri * 3 + 2]];
      const double d = (r[1] - s[1]) * (p[0] - s[0]) +
                       (s[0] - r[0]) * (p[1] - s[1]);
      require(std::isfinite(d) && std::abs(d) > 1e-20,
              "Independent advanced public UV determinant is finite and nonsingular");
      const double u = ((r[1] - s[1]) * (q[0] - s[0]) +
                        (s[0] - r[0]) * (q[1] - s[1])) / d;
      const double v = ((s[1] - p[1]) * (q[0] - s[0]) +
                        (p[0] - s[0]) * (q[1] - s[1])) / d;
      require(std::isfinite(u) && std::isfinite(v) && std::isfinite(1-u-v),
              "Independent advanced public barycentrics are finite");
      if (std::min({u, v, 1-u-v}) < .06) continue;
      const std::array<float, 3> bary{float(u), float(v), float(1-u-v)};
      for (size_t role = 0; role < 2; ++role) {
        const auto wanted = advanced_fixture::decode(source, tri, bary, role);
        const auto actual = direction(destination, tri, bary, role);
        const auto wrong = advanced_fixture::decode(source, tri, bary, role, true);
        const auto absent = advanced_fixture::decode(source, tri, bary, role, false, true);
        const auto other = advanced_fixture::decode(source, tri, bary, 1-role);
        for (auto value : {wanted, actual, wrong, absent, other}) finite_sample(value);
        for (size_t axis = 0; axis < 3; ++axis) {
          maximum[role] = std::max(maximum[role], std::abs(wanted[axis]-actual[axis]));
          wrong_domain[role] = std::max(wrong_domain[role], std::abs(wanted[axis]-wrong[axis]));
          omitted[role] = std::max(omitted[role], std::abs(wanted[axis]-absent[axis]));
          swapped[role] = std::max(swapped[role], std::abs(wanted[axis]-other[axis]));
        }
      }
      ++interior;
      break;
    }
  }
  require(interior > 500, "Advanced public oracle covers more than500 genuine interior texels");
  for (size_t role = 0; role < 2; ++role)
    require(std::isfinite(maximum[role]) && maximum[role] < .035 &&
            wrong_domain[role] > .05 && omitted[role] > .05 && swapped[role] > .05,
            "Each actual advanced normal role satisfies .035 and rejects wrong-domain, omitted and swapped controls beyond .05 on the same texels");
  return {{"interior_texels", interior}, {"maximum_component_error", maximum},
          {"maximum_allowed_error", .035}, {"wrong_domain", wrong_domain},
          {"omitted", omitted}, {"swapped", swapped}, {"minimum_negative_error", .05},
          {"finite_samples", true},
          {"oracle", "Independent double declared-frame/domain/live-plane decoder; no source resolver or renderer output"}};
}
void emit(const std::filesystem::path &folder) {
  std::filesystem::create_directories(folder);
  Json groups = Json::object();
  for (auto method : {SourceNormalUvMethod::Rasterize, SourceNormalUvMethod::RetainSource}) {
    const auto group = folder / family(method);
    std::filesystem::create_directories(group);
    Json cases = Json::object(), labels = Json::array();
    for (bool legacy : {false, true}) for (bool posed : {false, true}) {
      const auto c = make_case(legacy, posed, method);
      const auto name = std::string(family(method)) + (legacy ? "-mt0-" : "-mt1-") + (posed ? "posed" : "rest");
      const auto out = group / name;
      std::filesystem::create_directories(out);
      c.before.save(out / "before.dsculpt");
      const auto before = c.before.encode();
      const auto sources = prepared_public::retained_sources(c.before, c.destination);
      const auto before_paint = c.before.object(c.prepared).painted;
      const auto binding = c.before.object(c.prepared).material_bindings;
      const auto first_recipe = c.before.object(c.prepared).bake_recipe;
      Json routes = Json::object(), oracles = Json::object();
      for (bool desktop : {false, true}) {
        const std::string route = desktop ? "desktop" : "xr";
        const auto options = prepared_public::route_settings(desktop);
        auto expected = c.before.snapshot();
        const auto paint = expected.object(c.prepared).painted;
        require(paint != before_paint && expected.encode() == before &&
                expected.object(c.prepared).material_bindings == binding &&
                expected.object(c.prepared).bake_recipe == first_recipe,
                "Detached advanced public snapshot preserves exact data with its own paint wrapper");
        require(bake_object(expected, c.prepared, options).projected > 0,
                "Actual UI-default advanced-source second Bake projects genuine data");
        const auto &recipe = expected.object(c.destination).bake_recipe;
        require(recipe && recipe->source.object == c.prepared &&
                recipe->source.original_object == c.prepared &&
                recipe->source.name == c.before.object(c.prepared).name &&
                recipe->settings.prepare_normals,
                "Advanced public expected endpoint records the selected source ID/name and normals");
        require(prepared_public::retained_sources(expected, c.destination) == sources &&
                expected.object(c.prepared).painted == paint &&
                expected.object(c.prepared).material_bindings == binding &&
                expected.object(c.prepared).bake_recipe == first_recipe &&
                c.before.object(c.prepared).painted == before_paint && c.before.encode() == before,
                "Advanced public second Bake retains exact Original, SourceUV graph and first-Bake owners");
        oracles[route] = normal_oracle(expected, c.prepared, c.destination, method);
        auto wrong = c.before.snapshot();
        require(bake_object(wrong, c.original, options).projected > 0 &&
                wrong.object(c.destination).bake_recipe &&
                wrong.object(c.destination).bake_recipe->source.object == c.original &&
                wrong.encode() != expected.encode() &&
                prepared_public::retained_sources(wrong, c.destination) == sources,
                "Actual wrong-Original counterfactual is distinct and retains the same genuine SourceUV context");
        const auto accepted = route + "-expected.dsculpt";
        const auto wrong_file = route + "-wrong-original.dsculpt";
        expected.save(out / accepted);
        wrong.save(out / wrong_file);
        Document document;
        document.replace(c.before.snapshot());
        require(document.wait_mesh(), "Advanced public worker publishes its real before geometry");
        document.bake_from_object(c.prepared, options);
        auto save = document.save_async(out / (route + "-native-ordered.dsculpt"));
        save.get();
        require(document.wait_asset(std::chrono::seconds(120)) && document.wait_mesh() &&
                document.frame().state->error.empty() &&
                document.copy_scene().encode() == expected.encode() &&
                Scene::load(out / (route + "-native-ordered.dsculpt")).encode() == expected.encode(),
                "Actual advanced worker and immediately ordered Save match the independent source/native endpoint");
        require(document.undo() && document.wait_mesh() && document.copy_scene().encode() == before,
                "Advanced public second Bake Undo restores the complete before Scene");
        require(document.redo() && document.wait_mesh() && document.copy_scene().encode() == expected.encode(),
                "Advanced public second Bake Redo restores the exact accepted Scene");
        routes[route] = {{"expected", accepted}, {"wrong_original", wrong_file},
                         {"settings", prepared_public::settings(options)}};
      }
      const auto &source = c.before.object(c.prepared);
      Json manifest{{"version", 1}, {"kind", "advanced-prepared-normal-public"},
        {"case", name}, {"legacy", legacy}, {"posed_original", posed}, {"resolution", 64},
        {"uv_method", method_name(method)}, {"source_uv_applied", true},
        {"source_normal_version", source.painted->normal_projection()->version},
        {"original_source", c.original}, {"prepared_source", c.prepared}, {"destination", c.destination},
        {"source_choice_initial", c.original}, {"source_choice_required", c.prepared},
        {"original_label", c.before.object(c.original).name}, {"prepared_label", source.name},
        {"destination_label", c.before.object(c.destination).name},
        {"label_keys", {{"source_original", name+"-original"}, {"source_prepared", name+"-source"},
          {"baking_prepared", name+"-baking"}, {"desktop_prepared", name+"-desktop"},
          {"desktop_original", name+"-desktop-original"}, {"normals_no", "portable-normals-no"},
          {"normals_yes", "portable-normals-yes"}}},
        {"genuine_first_bake_source", first_recipe->source.object},
        {"first_bake_recipe", first_recipe->encode()}, {"all_three_objects_retained", true},
        {"portable_normals_toggle_required", true},
        {"all_other_UI_defaults", prepared_public::settings(ObjectBakeSettings{})},
        {"before", "before.dsculpt"}, {"routes", routes}, {"independent_normal_oracles", oracles},
        {"wrong_original_must_fail_full_native_endpoint", true},
        {"source_uv_operation_scope", "Native-produced authentic SourceUV before public second Bake; not public UV-authoring action coverage"}};
      prepared_public::write_json(out / "manifest.json", manifest);
      cases[name] = name + "/manifest.json";
      for (auto entry : std::array<std::pair<std::string, std::string>,5>{{
          {"source", "Source: "+source.name}, {"original", "Source: "+c.before.object(c.original).name},
          {"baking", source.name}, {"desktop-original", c.before.object(c.original).name},
          {"desktop", source.name}}})
        labels.push_back({{"key", name+"-"+entry.first}, {"text", entry.second},
                          {"baseline", entry.first=="baking" ? 285 : 202}});
    }
    labels.push_back({{"key", "portable-normals-no"}, {"text", "Author portable normals: No"}, {"baseline", 312}});
    labels.push_back({{"key", "portable-normals-yes"}, {"text", "Author portable normals: Yes"}, {"baseline", 312}});
    require(labels.size() == 22, "Each advanced family stays within the existing bounded32-label font request");
    prepared_public::write_json(group / "labels.json", labels);
    prepared_public::write_json(group / "manifest.json", {{"version", 1},
      {"kind", "advanced-prepared-normal-public-family"}, {"uv_method", method_name(method)},
      {"cases", cases}, {"label_requests", "labels.json"}, {"existing_native17_unchanged", true},
      {"scope", "Four genuine native capsules; public actions require separately reviewed current-product application"}});
    groups[family(method)] = std::string(family(method)) + "/manifest.json";
  }
  prepared_public::write_json(folder / "manifest.json", {{"version", 1},
    {"kind", "advanced-prepared-normal-public-suite"}, {"families", groups},
    {"case_count", 8}, {"new_targets", 0}, {"UI_execution", false}});
}
Json verify(const std::filesystem::path &manifest_path, const std::string &route,
            const std::string &phase, const std::filesystem::path &actual_path) {
  std::ifstream file(manifest_path);
  Json manifest;
  file >> manifest;
  require(manifest.at("version") == 1 && manifest.at("kind") == "advanced-prepared-normal-public" &&
          (route == "desktop" || route == "xr"), "Advanced verifier requires an explicit genuine public capsule");
  const auto folder = manifest_path.parent_path();
  const uint64_t original = manifest.at("original_source"), source = manifest.at("prepared_source"),
                 target_id = manifest.at("destination");
  const auto before = Scene::load(folder / manifest.at("before").get<std::string>());
  const auto actual = Scene::load(actual_path);
  const auto expected = Scene::load(folder / manifest.at("routes").at(route).at("expected").get<std::string>());
  const auto wrong = Scene::load(folder / manifest.at("routes").at(route).at("wrong_original").get<std::string>());
  const auto method = manifest.at("uv_method") == "Rasterize" ? SourceNormalUvMethod::Rasterize : SourceNormalUvMethod::RetainSource;
  require(manifest.at("uv_method") == method_name(method), "Advanced verifier rejects an unknown UV method");
  advanced_authority(before.object(source), method);
  advanced_authority(actual.object(source), method);
  require(before.objects().size() == 3 && actual.objects().size() == 3 &&
          prepared_public::retained_sources(actual, target_id) == prepared_public::retained_sources(before, target_id) &&
          before.object(source).bake_recipe && before.object(source).bake_recipe->source.object == original,
          "Advanced actual Save retains exact Original/SourceUV graph and genuine first-Bake recipe");
  Json oracle;
  if (phase == "undone")
    require(actual.encode() == before.encode(), "Actual advanced UI Undo restores the complete before Scene");
  else {
    require(phase == "accepted" || phase == "redone" || phase == "reopened", "Unknown advanced public durable phase");
    require(actual.encode() == expected.encode() && actual.encode() != wrong.encode(),
            "Actual advanced UI must match the prepared endpoint and reject independently baked wrong Original");
    const auto &recipe = actual.object(target_id).bake_recipe;
    require(recipe && recipe->source.object == source && recipe->source.original_object == source &&
            recipe->source.name == manifest.at("prepared_label").get<std::string>() && recipe->settings.prepare_normals,
            "Actual advanced UI recipe records explicitly selected advanced source ID/name and normals");
    oracle = normal_oracle(actual, source, target_id, method);
  }
  return {{"verified", true}, {"case", manifest.at("case")}, {"route", route}, {"phase", phase},
          {"uv_method", method_name(method)}, {"original_source", original}, {"prepared_source", source},
          {"destination", target_id}, {"all_sources_and_first_recipe_exact", true},
          {"normal_oracle", oracle}, {"checks", checks}};
}
} // namespace advanced_public
