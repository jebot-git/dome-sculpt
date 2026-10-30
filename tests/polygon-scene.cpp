// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/polygon-scene.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/import-authoring.hpp"
#include "dome/import-source.hpp"
#include "dome/material-bindings.hpp"
#include "dome/retopo-authoring.hpp"
#include "import-skin-fixtures.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
  ++checks;
  std::cout << "PASS " << m << '\n';
}
template <class F>
void refuses(F f, const char *m, std::string_view diagnostic = {}) {
  try {
    f();
  } catch (const std::exception &e) {
    if (!diagnostic.empty() &&
        std::string_view(e.what()).find(diagnostic) == std::string_view::npos)
      throw std::runtime_error(std::string(m) + ": wrong diagnostic " +
                               e.what());
    check(true, m);
    return;
  }
  throw std::runtime_error(std::string(m) + ": accepted");
}
std::shared_ptr<PolygonAuthority> authority() {
  auto a = std::make_shared<PolygonAuthority>();
  a->topology = PolygonMesh::grid();
  return a;
}
void install(Scene &s, std::shared_ptr<const PolygonAuthority> a) {
  auto &o = s.active();
  o.polygon = a;
  o.mesh_active = true;
  o.painted = std::make_shared<PaintedMesh>(a->topology.triangulate().mesh, 64);
  o.deformation.reset();
  o.appearance.reset();
  o.material_bindings.reset();
  o.retopology.reset();
}
std::shared_ptr<PolygonOrigin> origin(const SculptObject &o) {
  auto p = std::make_shared<PolygonOrigin>();
  p->volume = std::make_shared<const Volume>(o.volume->geometry_snapshot());
  if (o.painted)
    p->paint = o.painted->snapshot();
  p->name = o.name;
  p->material = o.material;
  p->mesh_active = o.mesh_active;
  p->deformation = o.deformation;
  p->appearance = o.appearance;
  p->material_bindings = o.material_bindings;
  p->retopology = o.retopology;
  p->polygon = o.polygon;
  p->world_transform = o.transform;
  p->source_object = o.id;
  return p;
}
} // namespace
int main() try {
  auto path = std::filesystem::temp_directory_path() /
              ("dome-polygon-scene-" + std::to_string(getpid()));
  std::filesystem::create_directories(path);
  struct Cleanup {
    std::filesystem::path p;
    ~Cleanup() {
      std::error_code e;
      std::filesystem::remove_all(p, e);
    }
  } cleanup{path};
  Scene plain;
  auto legacy = plain.encode();
  check(legacy["version"] == 5 && !legacy.contains("polygon_sources"),
        "Ordinary legacy scene writer remains version5 without polygon fields");
  check(Scene::decode(legacy).encode() == legacy,
        "Ordinary legacy archive remains byte-semantic stable");
  auto a = authority();
  a->origin = origin(plain.active());
  Scene scene;
  install(scene, a);
  scene.validate();
  auto encoded = scene.encode();
  check(encoded["version"] == 12 &&
            encoded["polygon_sources"]["authorities"].size() == 1,
        "Editable polygon authority selects complete Scene12 shape");
  auto restarted = Scene::decode(encoded);
  check(
      restarted.encode() == encoded &&
          restarted.active().polygon->topology.encode() == a->topology.encode(),
      "Stable IDs, N-gon loops, hard edges, UV and origin round-trip exactly");
  scene.save(path / "polygon.dsculpt");
  check(Scene::load(path / "polygon.dsculpt").encode() == encoded,
        "Native CBOR save/restart retains editable polygon authority");
  auto duplicate = scene.duplicate(scene.active().id);
  auto shared = Scene::decode(scene.encode());
  check(shared.objects()[0].polygon == shared.object(duplicate).polygon &&
            shared.objects()[0].polygon->origin ==
                shared.object(duplicate).polygon->origin,
        "Two instances deduplicate authority and first-source holder by "
        "identity");
  check(scene.encode()["polygon_sources"]["origins"].size() == 1 &&
            scene.encode()["polygon_sources"]["volumes"].size() == 1,
        "Shared immutable source Volume is serialized once");
  auto prior = origin(scene.active());
  auto layered = authority();
  layered->origin = prior;
  Scene chain;
  install(chain, layered);
  auto chain_json = chain.encode();
  auto chain_copy = Scene::decode(chain_json);
  check(chain_copy.encode() == chain_json &&
            chain_copy.active().polygon->origin->polygon->origin,
        "Explicit reconversion retains a bounded prior-authority source DAG");
  Volume physical;
  physical.fill_mask(.75f);
  auto v4 = physical.encode();
  v4["version"] = 4;
  v4["distance_band"] = .2f;
  physical = Volume::decode(v4);
  physical.fill_mask(.75f);
  physical.add_layer("retained detail");
  auto volorigin = origin(plain.active());
  volorigin->volume =
      std::make_shared<const Volume>(physical.geometry_snapshot());
  auto voltop = authority();
  voltop->origin = volorigin;
  Scene physical_scene;
  install(physical_scene, voltop);
  auto physical_copy = Scene::decode(physical_scene.encode());
  check(physical_copy.active().polygon->origin->volume->encode() ==
            physical.encode(),
        "Retained Volume4 preserves physical band, sculpt layers and mask "
        "authoring state");
  auto changed = encoded;
  changed["version"] = 11;
  refuses([&] { Scene::decode(changed); },
          "Scene11 rejects rather than ignoring polygon data", "Scene12");
  changed = encoded;
  changed.erase("polygon_sources");
  refuses([&] { Scene::decode(changed); }, "Scene12 rejects missing archive");
  changed = encoded;
  changed["objects"][0].erase("polygon");
  refuses([&] { Scene::decode(changed); },
          "Scene12 rejects partial per-object feature shape");
  changed = encoded;
  changed["objects"][0]["polygon"] = 0.;
  refuses([&] { Scene::decode(changed); },
          "Floating archive references cannot masquerade as IDs");
  changed = encoded;
  changed["polygon_sources"]["origins"][0]["polygon"] = 0;
  refuses([&] { Scene::decode(changed); },
          "Archive source cycles rejected before shared objects are allocated",
          "cycle");
  changed = encoded;
  changed["polygon_sources"]["authorities"].push_back(
      changed["polygon_sources"]["authorities"][0]);
  refuses([&] { Scene::decode(changed); },
          "Unreachable archive payload is refused before decoding",
          "Unreachable");
  auto cyclic = authority();
  auto loop = origin(plain.active());
  cyclic->origin = loop;
  loop->polygon = cyclic;
  Scene invalid;
  install(invalid, cyclic);
  refuses([&] { invalid.encode(); },
          "Caller-created source cycle refuses atomically", "cycle");
  loop->polygon.reset();
  auto deep = authority();
  deep->active = false;
  for (int i = 0; i < 33; ++i) {
    auto o = origin(plain.active());
    o->polygon = deep;
    auto next = authority();
    next->active = false;
    next->origin = o;
    deep = next;
  }
  invalid = Scene{};
  install(invalid, deep);
  refuses([&] { invalid.encode(); }, "Bounded retained source depth enforced",
          "depth");
  auto badrest = a->topology.triangulate().mesh;
  badrest.positions[0].x += .01f;
  invalid = Scene{};
  install(invalid, a);
  invalid.active().painted = std::make_shared<PaintedMesh>(badrest, 64);
  refuses([&] { invalid.validate(); },
          "Canonical REST derivative position mismatch is rejected");
  badrest = a->topology.triangulate().mesh;
  badrest.uv[0][0] += .01f;
  invalid.active().painted = std::make_shared<PaintedMesh>(badrest, 64);
  refuses([&] { invalid.validate(); },
          "Canonical REST derivative UV mismatch is rejected");
  auto empty = std::make_shared<PolygonAuthority>();
  empty->origin = a->origin;
  Scene gone;
  Brush b;
  b.radius = .15f;
  gone.active().volume->begin_stroke();
  gone.active().volume->stamp(b);
  gone.active().volume->end_stroke();
  check(!surface_mesh(*gone.active().volume).indices.empty(),
        "Empty-authority fixture retains actual stale sculpt surface");
  gone.active().polygon = empty;
  gone.active().mesh_active = false;
  gone.active().painted.reset();
  auto invisible = gone.encode();
  check(gone.preview_surfaces().empty() && gone.surfaces().empty() &&
            Scene::decode(invisible).preview_surfaces().empty(),
        "Active zero-face authority suppresses retained Volume before and "
        "after restart");
  auto dormant = std::make_shared<PolygonAuthority>(*a);
  dormant->active = false;
  gone.active().polygon = dormant;
  check(!gone.preview_surfaces().empty(),
        "Explicit dormant polygon authority permits active sculpt preview");
  refuses([&] { gone.surfaces(); },
          "Dormant polygon export does not silently discard source authority");
  // Real source adapter supplies exact IBM/rest matrices and immutable GLB.
  auto f = import_fixture::skinned(true);
  f.save(path / "source.glb");
  ImportAuthoringOptions options;
  options.paint_resolution = 64;
  auto imported =
      prepare_authoring_import(Scene{}, path / "source.glb", options);
  auto source = imported.scene.object(imported.objects.front());
  auto retained = origin(source);
  retained->source_object = UINT64_MAX;
  retained->retopology = std::make_shared<const RetopoPatch>(
      RetopoPatch{UINT64_MAX, RetopoMesh{}});
  auto top = authority();
  top->origin = retained;
  Scene sourced;
  install(sourced, top);
  auto before = sourced.encode();
  auto copy = Scene::decode(before);
  const auto &r = *copy.active().polygon->origin;
  check(before["version"] == 12 && r.source_object == UINT64_MAX &&
            *r.appearance->original_bytes == *source.appearance->original_bytes,
        "Deleted/high provenance ID and original GLB bytes survive without "
        "live lookup");
  check(r.deformation->skeleton->inverse_bind_override ==
                source.deformation->skeleton->inverse_bind_override &&
            r.deformation->encode(r.paint->mesh()) ==
                source.deformation->encode(source.painted->mesh()),
        "Retained-only authored inverse binds, exact rest and current pose "
        "survive Scene12");
  check(r.appearance->source_vertices == source.appearance->source_vertices &&
            r.material_bindings->normal_frames->targets.size() ==
                source.material_bindings->normal_frames->targets.size(),
        "Raw source remap and original-frame target authority survive polygon "
        "conversion");
  check(r.retopology && r.retopology->source == UINT64_MAX,
        "Detached original retopology provenance does not require a live "
        "source object");
  changed = before;
  changed["polygon_sources"]["origins"][0]["material_bindings"] = 999;
  changed["source_images"] = nullptr;
  refuses([&] { Scene::decode(changed); },
          "Retained material references checked before image/model allocation",
          "polygon archive reference");
  changed = before;
  changed["polygon_sources"]["origins"][0]["appearance"]["asset"] = 999;
  changed["source_images"] = nullptr;
  refuses([&] { Scene::decode(changed); },
          "Retained raw-model references checked before central resource "
          "allocation",
          "polygon archive reference");
  check(copy.encode() == before,
        "Nested source/frame/deformation archive exact repeated restart");
  // Source receipts for non-GLB formats share the same central asset archive.
  std::ofstream(path / "source.obj")
      << "o quad\nv 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n";
  auto obj = prepare_authoring_import(Scene{}, path / "source.obj", options);
  auto objorigin = origin(obj.scene.object(obj.objects.front()));
  auto objtop = authority();
  objtop->origin = objorigin;
  Scene objscene;
  install(objscene, objtop);
  auto objcopy = Scene::decode(objscene.encode());
  check(objcopy.active().polygon->origin->appearance->source_receipt->format ==
                ImportSourceFormat::Obj &&
            *objcopy.active()
                    .polygon->origin->appearance->source_receipt->root.bytes ==
                *objorigin->appearance->source_receipt->root.bytes,
        "Nested original OBJ format/resource receipt is retained separately "
        "from canonical GLB");
  changed = encoded;
  auto &p = changed["polygon_sources"];
  p["paints"].clear();
  auto paint = encoded["objects"][0]["painted"];
  paint["resolution"] = 2048;
  paint["mesh"]["positions"] = nullptr;
  // Five reachable retained surfaces request 20M SurfaceMap pixels in a tiny
  // serialized fixture; invalid mesh bytes ensure allocation guard runs first.
  p["authorities"].clear();
  p["origins"].clear();
  for (int i = 0; i < 5; ++i) {
    auto aa = encoded["polygon_sources"]["authorities"][0];
    aa["active"] = false;
    aa["origin"] = i;
    p["authorities"].push_back(aa);
    auto oo = encoded["polygon_sources"]["origins"][0];
    oo["painted"] = i;
    oo["mesh_active"] = false;
    oo["polygon"] = i == 4 ? nlohmann::json(nullptr) : nlohmann::json(i + 1);
    p["origins"].push_back(oo);
    p["paints"].push_back(paint);
  }
  refuses([&] { Scene::decode(changed); },
          "Aggregate retained paint allocation quota checked before any "
          "malformed mesh decode",
          "paint-map budget");
  changed = encoded;
  for (int i = 0; i < 512; ++i)
    changed["polygon_sources"]["authorities"].push_back(
        encoded["polygon_sources"]["authorities"][0]);
  refuses([&] { Scene::decode(changed); },
          "Bounded graph record count prevents archive amplification", "count");
  std::cout << checks << " Scene12 polygon archive checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
