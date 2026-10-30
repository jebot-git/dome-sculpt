// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/local-surface.hpp"
#include "dome/material-bindings.hpp"
#include "dome/sculpt-resolution.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
namespace {
unsigned checks = 0;
void check(bool yes, const char *label) {
  if (!yes)
    throw std::runtime_error(label);
  ++checks;
  std::cout << "PASS " << label << '\n';
}
template <class F> bool rejects(F call) {
  try {
    call();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
auto ordered(const Volume &v) {
  auto j = v.encode();
  std::sort(j["samples"].begin(), j["samples"].end());
  for (auto &l : j["sculpt_layers"])
    std::sort(l["delta"].begin(), l["delta"].end());
  if (j.contains("sculpt_mask"))
    std::sort(j["sculpt_mask"]["values"].begin(),
              j["sculpt_mask"]["values"].end());
  return j;
}
void write(const std::filesystem::path &path, const nlohmann::json &j) {
  auto bytes = nlohmann::json::to_cbor(j);
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  if (!out)
    throw std::runtime_error("Fixture write failed");
}
Volume sphere() {
  Volume v(.04f);
  v.begin_stroke();
  Brush b;
  b.radius = .137f;
  b.blend = 0;
  b.from = b.to = {.013f, .017f, -.011f};
  v.stamp(b);
  v.end_stroke();
  return v;
}
void stamp(Volume &v) {
  v.begin_stroke();
  Brush b;
  b.tool = Tool::Carve;
  b.radius = .04f;
  b.blend = 0;
  b.from = b.to = {.10f, .02f, .07f};
  v.stamp(b);
  v.end_stroke();
}
bool same(const Mesh &a, const Mesh &b) {
  if (a.indices != b.indices || a.positions.size() != b.positions.size() ||
      a.normals.size() != b.normals.size())
    return false;
  for (size_t i = 0; i < a.positions.size(); ++i) {
    auto p = a.positions[i], q = b.positions[i], n = a.normals[i],
         m = b.normals[i];
    if (p.x != q.x || p.y != q.y || p.z != q.z || n.x != m.x || n.y != m.y ||
        n.z != m.z)
      return false;
  }
  return true;
}
nlohmann::json extended(nlohmann::json j, int version, bool materials = true,
                        bool retopo = true) {
  j["version"] = version;
  if (materials) {
    j["source_materials"] = {{"version", 1},
                             {"paints", nlohmann::json::array()},
                             {"origins", nlohmann::json::array()},
                             {"bindings", nlohmann::json::array()}};
    for (auto &o : j["objects"])
      o["material_bindings"] = nullptr;
  }
  if (retopo)
    for (auto &o : j["objects"])
      o["retopology"] = nullptr;
  return j;
}
} // namespace
int main() try {
  auto original = sphere();
  auto original_owner = original.geometry_owner();
  auto cache = LocalSurfaceCache::build(original);
  SculptResolutionOptions options;
  options.voxel_size = .02f;
  auto output = resample_sculpt(original, options);
  check(bool(output.replacement) && output.report.changed,
        "Resolution prepares a detached physical-band replacement");
  auto &fine = *output.replacement;
  check(fine.band() == original.band() &&
            fine.voxel_size() != original.voxel_size() &&
            fine.geometry_change(original_owner).kind ==
                Volume::GeometryChange::Kind::Rebuild,
        "Resampled map/context begins unknown authenticated lineage");
  auto copy = fine.geometry_snapshot();
  check(copy.band() == fine.band() &&
            copy.geometry_owner() == fine.geometry_owner(),
        "Full authoring snapshot retains physical band and exact SDF owner");
  LocalSurfaceUpdateReport report;
  cache = cache->update(fine, {}, &report);
  check(report.kind == LocalSurfaceUpdateReport::Kind::Rebuild &&
            cache->source_owner()->band() == fine.band() &&
            same(cache->materialize(), surface_mesh(fine)),
        "Minimal SDF cache rebuilds on resolution and samples the physical "
        "band correctly");
  auto before = fine.geometry_snapshot();
  stamp(fine);
  auto delta = fine.geometry_change(before.geometry_owner());
  check(delta.kind == Volume::GeometryChange::Kind::Local && delta.events == 1,
        "First post-resolution stamp creates authenticated local ancestry");
  auto snapshot = fine.geometry_snapshot();
  check(snapshot.band() == fine.band() &&
            snapshot.geometry_change(before.geometry_owner()).kind ==
                Volume::GeometryChange::Kind::Local,
        "Snapshot carries both physical context and complete geometry journal");
  cache = cache->update(snapshot, {}, &report);
  check(report.kind == LocalSurfaceUpdateReport::Kind::Local &&
            same(cache->materialize(), surface_mesh(snapshot)),
        "Post-resolution local extraction remains equivalent to whole-surface "
        "oracle");
  auto owner = fine.geometry_owner();
  auto history = fine.undo_count();
  options.voxel_size = fine.voxel_size();
  auto noop = resample_sculpt(fine, options);
  check(!noop.replacement && !noop.report.changed &&
            fine.geometry_owner() == owner && fine.undo_count() == history &&
            fine.geometry_change(before.geometry_owner()).events ==
                delta.events,
        "Same-spacing no-op preserves owner, editing history and authenticated "
        "ancestry");
  check(
      fine.undo() && fine.geometry_change(owner).kind ==
                         Volume::GeometryChange::Kind::Local,
      "Undo after resampling retains physical-band authenticated transitions");
  check(fine.redo() && fine.band() == original.band(),
        "Redo after resampling preserves physical sampling context");

  auto legacy1 = original.encode();
  legacy1["version"] = 1;
  for (auto key : {"sculpt_layers", "active_sculpt_layer", "next_sculpt_layer"})
    legacy1.erase(key);
  auto legacy2 = original.encode();
  auto protected_source = original.geometry_snapshot();
  protected_source.fill_mask(.37f);
  auto legacy3 = protected_source.encode();
  auto v4 = fine.encode();
  auto path = std::filesystem::temp_directory_path() /
              ("dome-resolution-auth-" + std::to_string(getpid()) + ".dsculpt");
  for (const auto &volume : {legacy1, legacy2, legacy3, v4}) {
    auto expected = Volume::decode(volume);
    auto decoded = Scene::decode(volume);
    write(path, volume);
    auto loaded = Scene::load(path);
    auto direct = Volume::load(path);
    check(ordered(*decoded.active().volume) == ordered(expected) &&
              ordered(*loaded.active().volume) == ordered(expected) &&
              ordered(direct) == ordered(expected),
          "Flat Volume1/2/3/4 decode and actual native loaders discriminate "
          "from Scene schemas");
  }
  Scene plain;
  plain.active().volume = std::make_shared<Volume>(fine.geometry_snapshot());
  auto encoded = plain.encode();
  for (int version : {4, 5, 6, 7, 8}) {
    auto scene = encoded;
    if (version == 4) {
      scene["version"] = 4;
      for (auto key :
           {"groups", "next_group_id", "source_images", "source_models"})
        scene.erase(key);
    } else if (version >= 7)
      scene = extended(scene, version);
    else
      scene["version"] = version;
    write(path, scene);
    auto loaded = Scene::load(path);
    check(loaded.objects().size() == 1 &&
              ordered(*loaded.active().volume) == ordered(fine),
          "Scene4/5/6/7/8 load nested Volume4 without flat-file "
          "misclassification");
  }
  for (auto [materials, retopo] :
       {std::pair{true, false}, std::pair{false, true}}) {
    auto scene = extended(encoded, 7, materials, retopo);
    check(ordered(*Scene::decode(scene).active().volume) == ordered(fine),
          "Both historical Scene7 feature variants preserve independent nested "
          "Volume4");
  }
  for (auto key : {"samples", "voxel_size", "distance_band"}) {
    auto mixed = encoded;
    mixed[key] = v4.at(key);
    check(rejects([&] { Scene::decode(mixed); }),
          "Mixed flat Volume and Scene declarations are refused explicitly");
  }
  for (auto key : {"distance_band", "sculpt_mask", "sculpt_controls"}) {
    auto bad = v4;
    bad.erase(key);
    check(rejects([&] { Scene::decode(bad); }),
          "Flat Volume4 cannot fall through to Scene4 after losing required "
          "fields");
  }
  for (auto version : {1, 2, 3}) {
    auto bad = v4;
    bad["version"] = version;
    check(rejects([&] { Scene::decode(bad); }),
          "Legacy flat files cannot silently discard a Volume4 physical band");
  }
  for (auto value :
       {nlohmann::json(false), nlohmann::json(".16"), nlohmann::json(0),
        nlohmann::json(-1), nlohmann::json(5)}) {
    auto bad = encoded;
    bad["objects"][0]["volume"]["distance_band"] = value;
    check(rejects([&] { Scene::decode(bad); }),
          "Nested Volume4 physical context is strictly checked before decoding "
          "maps");
  }
  {
    auto bad = encoded;
    bad["objects"][0]["volume"]["extra_resolution_state"] = true;
    check(rejects([&] { Scene::decode(bad); }),
          "Nested Volume4 unknown semantics are refused by the shared header "
          "preflight");
  }
  {
    auto bad = encoded;
    bad["objects"].push_back(bad["objects"][0]);
    bad["objects"][1]["id"] = 2;
    bad["next_id"] = 3;
    for (auto &o : bad["objects"]) {
      auto &values = o["volume"]["sculpt_mask"]["values"];
      values = nlohmann::json::array();
      values.get_ref<nlohmann::json::array_t &>().resize(2'000'001);
    }
    bool quota = false;
    try {
      Scene::decode(bad);
    } catch (const std::length_error &e) {
      quota =
          std::string(e.what()) == "Scene sculpt storage limit exceeded" ||
          std::string(e.what()) == "Scene sculpt mask storage limit exceeded";
    }
    check(quota, "Aggregate nested Volume4 storage quota runs before any "
                 "malformed row is decoded");
  }
  plain.save(path);
  auto reopened = Scene::load(path);
  std::filesystem::remove(path);
  check(ordered(*reopened.active().volume) == ordered(fine) &&
            reopened.encode()["version"] == 5,
        "Native Scene save/restart retains Volume4 with no unnecessary outer "
        "schema bump");
  check(reopened.active().volume->geometry_change(fine.geometry_owner()).kind ==
            Volume::GeometryChange::Kind::Rebuild,
        "Reloaded resolution state has unknown ephemeral geometry lineage");
  std::cout << checks
            << " resolution/authentication/archive integration checks passed\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
