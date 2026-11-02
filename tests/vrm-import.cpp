// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/import-authoring.hpp"
#include "dome/material-bindings.hpp"
#include "dome/source-placement.hpp"
#include "import-controller-driver.hpp"
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks = 0;
std::atomic_bool gate = false, entered = false, released = false;
std::mutex gate_mutex;
std::condition_variable gate_changed;
template <class F> void wait(F f) {
  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!f()) {
    if (std::chrono::steady_clock::now() > end)
      throw std::runtime_error("VRM worker barrier timed out");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}
void check(bool, const char *);
void ready(Document &d) {
  check(d.wait_asset() && d.wait_native_stroke() && d.wait_mesh(),
        "VRM native workers settle");
  if (!d.frame().state->error.empty())
    throw std::runtime_error(d.frame().state->error);
}

void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
  ++checks;
}
std::vector<uint8_t> read(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {(std::istreambuf_iterator<char>(f)), {}};
}
uint32_t word(std::span<const uint8_t> b, size_t at) {
  return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 |
         uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}
} // namespace
extern "C" ssize_t __real_read(int, void *, size_t);
extern "C" ssize_t __wrap_read(int fd, void *buffer, size_t size) {
  if (gate.load()) {
    char link[64], path[4096];
    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    auto n = readlink(link, path, sizeof(path) - 1);
    if (n > 0 && std::string_view(path, size_t(n)).ends_with("/external.vrm")) {
      entered = true;
      gate_changed.notify_all();
      std::unique_lock lock(gate_mutex);
      gate_changed.wait_for(lock, std::chrono::seconds(10),
                            [] { return released.load(); });
    }
  }
  return __real_read(fd, buffer, size);
}
int main(int argc, char **argv) try {
  if (argc == 3) {
    const std::string_view mode = argv[1];
    if (mode == "--empty-project") {
      Scene{}.save(argv[2]);
      return 0;
    }
    if (mode == "--inspect") {
      std::cout << Scene::load(argv[2]).encode().dump() << '\n';
      return 0;
    }
    if (mode == "--ui-fixture") {
      auto bytes = read(DOME_VRM_TEST_SOURCE);
      std::ofstream f(argv[2], std::ios::binary);
      f.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
      return 0;
    }
  }
  if (argc < 2)
    return 2;
  const auto home = std::filesystem::temp_directory_path() /
                    ("dome-vrm-import-" + std::to_string(getpid()));
  std::filesystem::create_directories(home);
  setenv("HOME", home.c_str(), 1);
  setenv("XDG_STATE_HOME", home.c_str(), 1);
  setenv("XDG_CONFIG_HOME", home.c_str(), 1);
  struct Cleanup {
    std::filesystem::path p;
    ~Cleanup() {
      gate = false;
      released = true;
      gate_changed.notify_all();
      std::error_code ec;
      std::filesystem::remove_all(p, ec);
    }
  } cleanup{home};
  for (int arg = 1; arg < argc; ++arg) {
    auto bytes = read(argv[arg]);
    auto json = nlohmann::json::parse(bytes.begin() + 20,
                                      bytes.begin() + 20 + word(bytes, 12));
    Scene original;
    auto before = original.encode();
    ImportAuthoringOptions options;
    options.paint_resolution = 64;
    auto imported = prepare_authoring_import(original, argv[arg], options);
    check(original.encode() == before && imported.objects.size() == 19,
          "Actual external CC0 VRM import is detached and includes all19 "
          "primitives");
    std::shared_ptr<const ImportedAsset> asset;
    std::shared_ptr<const std::vector<uint8_t>> raw;
    size_t native_vertices = 0;
    for (auto id : imported.objects) {
      const auto &o = imported.scene.object(id);
      const auto &a = *o.appearance;
      if (!asset) {
        asset = a.asset;
        raw = a.original_bytes;
      }
      check(a.asset == asset && a.original_bytes == raw && *raw == bytes,
            "External avatar parts share one exact immutable captured source "
            "without baseline hash substitution");
      check(a.vrm_json &&
                nlohmann::json::parse(*a.vrm_json) ==
                    json.at("extensions").at("VRM") &&
                a.mtoon_json == json.at("extensions")
                                    .at("VRM")
                                    .at("materialProperties")
                                    .at(a.source_material)
                                    .dump(),
            "Complete version0 metadata and exact original material role "
            "properties remain immutable");
      check(o.mesh_active && o.painted && o.deformation &&
                o.deformation->skin && o.deformation->humanoid &&
                o.material_bindings->has_mtoon &&
                o.material_bindings->normal_frames->mtoon,
            "Actual external model has native editable geometry, authored "
            "binding, humanoid and typed MToon frames");
      const auto &origin = *o.material_bindings->uv_origin;
      check(origin.material && origin.material->encode() ==
                                   o.material_bindings->material.encode(),
            "First original UV provenance retains the exact typed MToon "
            "descriptor before any later adoption");
      for (size_t role = 0; role < origin.roles.size(); ++role)
        check(origin.roles[role].image ==
                      o.material_bindings->roles[role].image &&
                  origin.roles[role].wrap_s ==
                      o.material_bindings->roles[role].wrap_s &&
                  origin.roles[role].min_filter ==
                      o.material_bindings->roles[role].min_filter,
              "First typed role image/sampler identity remains immutable "
              "alongside raw appearance");
      check(imported.scene.has_affine_placement(id) &&
                a.source_basis_quaternion && a.source_basis_quaternion->w == 1,
            "External model retains its original node hierarchy independently "
            "of native edit/basis authority");
      const ImportedPrimitive *primitive = nullptr;
      for (const auto &p : asset->primitives)
        if (p.source_mesh == a.source_mesh &&
            p.source_primitive == a.source_primitive)
          primitive = &p;
      check(primitive && o.painted->mesh().indices.size() ==
                             primitive->mesh.indices.size(),
            "Used-vertex compaction preserves exact ordered original corners");
      for (size_t c = 0; c < primitive->mesh.indices.size(); ++c)
        check(a.source_vertices.at(o.painted->mesh().indices[c]) ==
                  primitive->mesh.indices[c],
              "Each editable corner keeps its exact original accessor vertex "
              "identity");
      check(o.deformation->humanoid->joints.size() >= 15 &&
                a.source_joint_nodes.size() ==
                    o.deformation->skeleton->bones.size(),
            "Every skinned part retains complete source-node humanoid/ancestor "
            "closure");
      native_vertices += o.painted->mesh().positions.size();
    }
    std::cout << "VRM_IMPORT " << std::filesystem::path(argv[arg]).filename()
              << " native_vertices=" << native_vertices
              << " original_nodes=" << asset->nodes.size() << std::endl;
    const auto encoded = imported.scene.encode();
    auto path = home / (std::to_string(arg) + ".dsculpt");
    imported.scene.save(path);
    auto reopened = Scene::load(path);
    check(reopened.encode() == encoded,
          "Actual external avatar metadata/material/frame/skin/morph source "
          "restarts byte-exact through native archive");
  }
  // Rewriting metadata creates an external, nonbaseline file while keeping the
  // exact original accessor/image bytes. Native admission must not
  // hash-substitute.
  auto bytes = read(argv[1]);
  auto j = nlohmann::json::parse(bytes.begin() + 20,
                                 bytes.begin() + 20 + word(bytes, 12));
  auto binary_start = size_t(20) + word(bytes, 12);
  std::vector<uint8_t> tail(bytes.begin() + binary_start, bytes.end());
  j["extensions"]["VRM"]["meta"]["title"] = "External avatar — 独立";
  j["extensions"]["VRM"]["exporterVersion"] =
      "Independent native-import fixture";
  j["nodes"][0]["name"] = "External root";
  auto text = j.dump();
  while (text.size() % 4)
    text += ' ';
  std::vector<uint8_t> external;
  auto add = [&](uint32_t v) {
    for (int b = 0; b < 4; ++b)
      external.push_back(uint8_t(v >> (8 * b)));
  };
  add(0x46546c67);
  add(2);
  add(uint32_t(20 + text.size() + tail.size()));
  add(uint32_t(text.size()));
  add(0x4e4f534a);
  external.insert(external.end(), text.begin(), text.end());
  external.insert(external.end(), tail.begin(), tail.end());
  auto folder = home / "sources";
  std::filesystem::create_directories(folder);
  auto source = folder / "external.vrm";
  {
    std::ofstream f(source, std::ios::binary);
    f.write(reinterpret_cast<const char *>(external.data()), external.size());
  }
  Document d;
  d.replace(Scene{});
  ready(d);
  auto baseline = d.copy_scene().encode();
  ImportSettings settings;
  settings.paint_resolution = 64;
  struct Release {
    ~Release() {
      gate = false;
      released = true;
      gate_changed.notify_all();
    }
  } release;
  gate = true;
  d.import_asset(source, settings);
  wait([] { return entered.load(); });
  d.cancel_asset_work();
  gate = false;
  released = true;
  gate_changed.notify_all();
  ready(d);
  check(d.copy_scene().encode() == baseline && !d.frame().state->can_undo,
        "Cancel at actual external VRM descriptor read preserves scene and "
        "history");
  d.import_asset(source, settings);
  ready(d);
  auto accepted = d.copy_scene().encode();
  auto active = d.copy_scene().active();
  check(d.copy_scene().objects().size() == 20 &&
            *active.appearance->original_bytes == external,
        "Real worker imports external metadata without substituting bundled "
        "avatar source");
  auto saved = home / "external.dsculpt";
  d.save_async(saved).get();
  check(Scene::load(saved).encode() == accepted,
        "Ordered native Save preserves actual imported VRM endpoint");
  check(d.undo() && d.copy_scene().encode() == baseline && d.redo() &&
            d.copy_scene().encode() == accepted,
        "One native Undo/Redo restores complete external avatar in one "
        "operation");
  ready(d);
  d.replace(Scene{});
  ready(d);
  d.configure_import(settings);
  d.open_import_browser(folder);
  wait([&] {
    auto v = d.frame().state->importing;
    return v && !v->busy;
  });
  ImportDriver controller(d);
  controller.open();
  controller.sector(8);
  controller.sector(9);
  controller.sector(10);
  wait([&] {
    auto v = d.frame().state->importing;
    return v && !v->busy;
  });
  check(d.frame().state->menu_page == 21 &&
            d.frame().state->importing->entries.size() == 1,
        "Shared controller browser lists external VRM source");
  controller.sector(6);
  ready(d);
  check(d.copy_scene().encode() == accepted,
        "Actual controller Apply uses the same typed source-preserving import "
        "worker");
  // Independently authored VRM1 envelope over licensed CC0 geometry. Every
  // version-specific material and humanoid identity is explicitly
  // reconstructed; this is not a runtime VRM0-to-VRM1 conversion or a baseline
  // hash exception.
  auto v1 = j;
  auto legacy = v1["extensions"]["VRM"];
  v1["extensions"].erase("VRM");
  nlohmann::json bones = nlohmann::json::object();
  for (const auto &bone : legacy["humanoid"]["humanBones"]) {
    auto role = bone["bone"].get<std::string>();
    for (const char *side : {"left", "right"}) {
      if (role == std::string(side) + "ThumbProximal")
        role = std::string(side) + "ThumbMetacarpal";
      else if (role == std::string(side) + "ThumbIntermediate")
        role = std::string(side) + "ThumbProximal";
    }
    bones[role] = {{"node", bone["node"]}};
  }
  v1["extensions"]["VRMC_vrm"] = {
      {"specVersion", "1.0"},
      {"meta",
       {{"name", "Independent VRM1"},
        {"authors", {"CC0 fixture"}},
        {"licenseUrl", "https://creativecommons.org/publicdomain/zero/1.0/"}}},
      {"humanoid", {{"humanBones", bones}}}};
  int expression_node = -1;
  for (size_t n = 0; n < v1["nodes"].size(); ++n)
    if (v1["nodes"][n].contains("mesh")) {
      auto mesh = v1["nodes"][n]["mesh"].get<size_t>();
      if (v1["meshes"][mesh]["primitives"][0].contains("targets")) {
        expression_node = int(n);
        break;
      }
    }
  check(expression_node >= 0,
        "Independent VRM1 fixture chooses a real morph-bearing mesh instance");
  v1["extensions"]["VRMC_vrm"]["expressions"]["preset"] = {
      {"aa",
       {{"morphTargetBinds",
         {{{"node", expression_node}, {"index", 0}, {"weight", .35f}}}}}},
      {"ih",
       {{"isBinary", true},
        {"morphTargetBinds",
         {{{"node", expression_node}, {"index", 0}, {"weight", 1.f}}}}}}};
  auto root_count = v1["nodes"].size();
  auto roots = v1["scenes"][v1.value("scene", 0)]["nodes"];
  v1["nodes"].push_back({{"name", "External anisotropic root"},
                         {"children", roots},
                         {"scale", {1.2f, .8f, 1.1f}},
                         {"rotation", {0, 0, std::sin(.2f), std::cos(.2f)}},
                         {"translation", {.1f, -.2f, .05f}}});
  v1["scenes"][v1.value("scene", 0)]["nodes"] = {root_count};
  for (auto &skin : v1["skins"])
    skin.erase("skeleton"); // This NEW source declares no legacy advisory hint.
  for (size_t m = 0; m < v1["materials"].size(); ++m) {
    auto &material = v1["materials"][m];
    auto &typed = material["extensions"]["VRMC_materials_mtoon"];
    typed = {{"specVersion", "1.0"}, {"shadeColorFactor", {.73f, .65f, .59f}}};
    const auto &textures = legacy["materialProperties"][m]["textureProperties"];
    if (textures.contains("_ShadeTexture"))
      typed["shadeMultiplyTexture"] = {{"index", textures["_ShadeTexture"]}};
  }
  v1["extensionsUsed"] = {"KHR_materials_unlit", "VRMC_vrm",
                          "VRMC_materials_mtoon"};
  v1["extensionsRequired"] = {"VRMC_vrm", "VRMC_materials_mtoon"};
  text = v1.dump();
  while (text.size() % 4)
    text += ' ';
  external.clear();
  add(0x46546c67);
  add(2);
  add(uint32_t(20 + text.size() + tail.size()));
  add(uint32_t(text.size()));
  add(0x4e4f534a);
  external.insert(external.end(), text.begin(), text.end());
  external.insert(external.end(), tail.begin(), tail.end());
  auto v1path = home / "version1.vrm";
  {
    std::ofstream f(v1path, std::ios::binary);
    f.write(reinterpret_cast<const char *>(external.data()), external.size());
  }
  ImportLimits limits;
  limits.mode = ImportMode::PreserveGltf;
  bool ordinary_reject = false;
  try {
    (void)import_glb(external, limits);
  } catch (const std::runtime_error &) {
    ordinary_reject = true;
  }
  check(ordinary_reject,
        "Unrequested required VRM extensions retain the ordinary parser guard");
  limits.allow_vrm_metadata = true;
  auto parsed = import_glb(external, limits);
  check(parsed.nodes.size() == v1["nodes"].size(),
        "Explicit preserving parser authenticates exact VRM1 required metadata "
        "and typed MToon");
  ImportAuthoringOptions version1options;
  version1options.paint_resolution = 64;
  auto version1 = prepare_authoring_import(Scene{}, v1path, version1options);
  check(version1.objects.size() == 19,
        "VRM1 native authoring admits all actual source primitives through "
        "version-specific adapter");
  for (auto id : version1.objects) {
    const auto &o = version1.scene.object(id);
    if (o.appearance->source_node == expression_node && o.deformation->morphs)
      check(o.deformation->expressions &&
                o.deformation->expressions->visemes.contains("aa") &&
                !o.deformation->expressions->visemes.contains("ih"),
            "VRM1 native vowel identity follows exact node/target; binary "
            "driver is retained raw rather than linearized");
    check(o.material_bindings->has_mtoon &&
              o.material_bindings->material.version() == MToonVersion::V1 &&
              !o.appearance->vrm_json &&
              *o.appearance->original_bytes == external,
          "VRM1 typed materials and raw authority survive without pretending "
          "to be VRM0 metadata");
  }
  auto v1save = home / "version1.dsculpt";
  version1.scene.save(v1save);
  check(Scene::load(v1save).encode() == version1.scene.encode(),
        "VRM1 required extension parser is also applied to retained native "
        "source reload");
  auto bad = v1;
  bad["extensionsRequired"].push_back("UNSUPPORTED_required");
  text = bad.dump();
  while (text.size() % 4)
    text += ' ';
  external.clear();
  add(0x46546c67);
  add(2);
  add(uint32_t(20 + text.size() + tail.size()));
  add(uint32_t(text.size()));
  add(0x4e4f534a);
  external.insert(external.end(), text.begin(), text.end());
  external.insert(external.end(), tail.begin(), tail.end());
  bool unknown_reject = false;
  try {
    (void)import_glb(external, limits);
  } catch (const std::runtime_error &) {
    unknown_reject = true;
  }
  check(unknown_reject,
        "VRM permission never skips an unknown required extension");
  auto pack = [&](const nlohmann::json &json,
                  const std::filesystem::path &path) {
    text = json.dump();
    while (text.size() % 4)
      text += ' ';
    external.clear();
    add(0x46546c67);
    add(2);
    add(uint32_t(20 + text.size() + tail.size()));
    add(uint32_t(text.size()));
    add(0x4e4f534a);
    external.insert(external.end(), text.begin(), text.end());
    external.insert(external.end(), tail.begin(), tail.end());
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char *>(external.data()), external.size());
  };
  for (int fault = 0; fault < 5; ++fault) {
    auto invalid = v1;
    if (fault == 0)
      invalid["extensions"]["VRMC_vrm"]["meta"].erase("licenseUrl");
    if (fault == 1)
      invalid["extensions"]["VRMC_vrm"]["expressions"]["custom"]["WrongNode"] =
          {{"morphTargetBinds",
            {{{"node", v1["nodes"].size()}, {"index", 0}, {"weight", 1.f}}}}};
    if (fault == 2)
      invalid["extensions"]["VRMC_vrm"]["expressions"]["preset"]["aa"]
             ["morphTargetBinds"][0]["index"] = 99999;
    if (fault == 3)
      invalid["extensions"]["VRMC_vrm"]["expressions"]["custom"]
             ["WrongMaterial"] = {{"materialColorBinds",
                                   {{{"material", v1["materials"].size()},
                                     {"type", "color"},
                                     {"targetValue", {1, 1, 1, 1}}}}}};
    if (fault == 4) {
      auto &primitive = invalid["meshes"][0]["primitives"][0];
      auto material = primitive["material"].get<size_t>();
      primitive["attributes"].erase("TEXCOORD_0");
      auto &m = invalid["materials"][material];
      m.erase("normalTexture");
      m.erase("emissiveTexture");
      m.erase("occlusionTexture");
      m["pbrMetallicRoughness"].erase("baseColorTexture");
      m["pbrMetallicRoughness"].erase("metallicRoughnessTexture");
      m["extensions"]["VRMC_materials_mtoon"]["shadeMultiplyTexture"] = {
          {"index", 0}};
    }
    auto badpath = home / "invalid.vrm";
    pack(invalid, badpath);
    Scene unchanged;
    auto exact = unchanged.encode();
    bool refused = false;
    try {
      (void)prepare_authoring_import(unchanged, badpath, version1options);
    } catch (const std::exception &error) {
      refused =
          fault != 4 ||
          std::string_view(error.what())
                  .find(
                      "VRM typed texture role requires original TEXCOORD_0") !=
              std::string_view::npos;
    }
    check(refused && unchanged.encode() == exact,
          "Malformed VRM1 licensing/expression references reject before "
          "partial source publication");
  }
  std::cout << checks << " actual generic VRM import checks passed\n";
  return 0;
} catch (const std::exception &e) {
  gate = false;
  released = true;
  gate_changed.notify_all();
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
