// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-authoring.hpp"
#include "dome/document.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/material-bindings.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
using Json = nlohmann::json;
namespace {
unsigned checks = 0;
void check(bool b, const char *s) {
  if (!b)
    throw std::runtime_error(s);
  ++checks;
  std::cout << "PASS " << s << std::endl;
}
void word(std::vector<uint8_t> &out, uint32_t v) {
  for (int b = 0; b < 4; ++b)
    out.push_back(uint8_t(v >> (b * 8)));
}
struct Fixture {
  Json j = {{"asset", {{"version", "2.0"}}},
            {"accessors", Json::array()},
            {"bufferViews", Json::array()}};
  std::vector<uint8_t> bin;
  int values(std::initializer_list<float> data, int dimension,
             const char *type) {
    auto start = bin.size();
    for (float f : data)
      word(bin, std::bit_cast<uint32_t>(f));
    int view = int(j["bufferViews"].size());
    j["bufferViews"].push_back({{"buffer", 0},
                                {"byteOffset", start},
                                {"byteLength", bin.size() - start}});
    int a = int(j["accessors"].size());
    j["accessors"].push_back({{"bufferView", view},
                              {"componentType", 5126},
                              {"count", data.size() / dimension},
                              {"type", type}});
    return a;
  }
  Fixture() {
    int p = values({-.1f, -.1f, 0, .1f, -.1f, 0, .1f, .1f, 0, -.1f, .1f, 0}, 3,
                   "VEC3");
    j["accessors"][p]["min"] = {-.1f, -.1f, 0};
    j["accessors"][p]["max"] = {.1f, .1f, 0};
    int n = values({0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}, 3, "VEC3");
    int t =
        values({1, 0, 0, -1, 1, 0, 0, -1, 1, 0, 0, -1, 1, 0, 0, -1}, 4, "VEC4");
    int u = values({0, 0, 1, 0, 1, 1, 0, 1}, 2, "VEC2"),
        u1 = values({.2f, .3f, .7f, .3f, .7f, .8f, .2f, .8f}, 2, "VEC2");
    int c = values({.5f, .2f, .8f, .4f, .2f, .7f, .3f, .6f, .8f, .1f, .2f, .8f,
                    .3f, .4f, .5f, 1},
                   4, "VEC4");
    auto start = bin.size();
    for (auto v : {0u, 1u, 2u, 0u, 2u, 3u})
      word(bin, v);
    int view = int(j["bufferViews"].size());
    j["bufferViews"].push_back({{"buffer", 0},
                                {"byteOffset", start},
                                {"byteLength", bin.size() - start}});
    int index = int(j["accessors"].size());
    j["accessors"].push_back({{"bufferView", view},
                              {"componentType", 5125},
                              {"count", 6},
                              {"type", "SCALAR"}});
    int dp =
        values({0, .01f, 0, 0, .02f, 0, 0, .03f, 0, 0, .04f, 0}, 3, "VEC3");
    int dn = values({.1f, 0, 0, .2f, 0, 0, .3f, 0, 0, .4f, 0, 0}, 3, "VEC3");
    int dt = values({0, .1f, 0, 0, .2f, 0, 0, .3f, 0, 0, .4f, 0}, 3, "VEC3");
    Json primitive = {
        {"attributes",
         {{"POSITION", p},
          {"NORMAL", n},
          {"TANGENT", t},
          {"TEXCOORD_0", u},
          {"TEXCOORD_1", u1},
          {"COLOR_0", c}}},
        {"indices", index},
        {"material", 0},
        {"targets", {{{"POSITION", dp}, {"NORMAL", dn}, {"TANGENT", dt}}}}};
    j["meshes"] = {{{"name", "Source mesh"},
                    {"primitives", {primitive}},
                    {"weights", {.2f}},
                    {"extras", {{"targetNames", {"Lift"}}}}}};
    j["nodes"] = {{{"name", "Root 日本語"},
                   {"translation", {.3f, .1f, 0}},
                   {"children", {1, 2}}},
                  {{"name", "Instance one"},
                   {"mesh", 0},
                   {"translation", {.2f, 0, 0}},
                   {"scale", {1.2f, 1.2f, 1.2f}},
                   {"weights", {.7f}}},
                  {{"name", "Instance two"},
                   {"mesh", 0},
                   {"translation", {-.3f, 0, -.2f}},
                   {"weights", {.3f}}}};
    j["scenes"] = {{{"name", "Selected"}, {"nodes", {0}}}};
    j["scene"] = 0;
    Image image{
        2,
        2,
        {255, 0, 0, 32, 0, 255, 0, 96, 0, 0, 255, 160, 255, 255, 255, 255}};
    auto png = image.png();
    while (bin.size() % 4)
      bin.push_back(0);
    int image_view = int(j["bufferViews"].size());
    j["bufferViews"].push_back({{"buffer", 0},
                                {"byteOffset", bin.size()},
                                {"byteLength", png.size()}});
    bin.insert(bin.end(), png.begin(), png.end());
    j["images"] = {{{"name", "RGBA"},
                    {"bufferView", image_view},
                    {"mimeType", "image/png"}}};
    j["samplers"] = {{{"wrapS", 33648},
                      {"wrapT", 33071},
                      {"magFilter", 9728},
                      {"minFilter", 9986}}};
    j["textures"] = {{{"source", 0}, {"sampler", 0}}};
    j["materials"] = {
        {{"name", "Alpha source"},
         {"pbrMetallicRoughness",
          {{"baseColorFactor", {.7f, .8f, .9f, .6f}},
           {"baseColorTexture", {{"index", 0}}},
           {"metallicFactor", .25f},
           {"roughnessFactor", .65f}}},
         {"alphaMode", "MASK"},
         {"alphaCutoff", .3f},
         {"doubleSided", true},
         {"extensions", {{"KHR_materials_unlit", Json::object()}}}}};
    j["extensionsUsed"] = {"KHR_materials_unlit"};
  }
  std::vector<uint8_t> bytes() {
    auto json = j;
    json["buffers"] = {{{"byteLength", bin.size()}}};
    auto text = json.dump();
    while (text.size() % 4)
      text += ' ';
    auto binary = bin;
    while (binary.size() % 4)
      binary.push_back(0);
    std::vector<uint8_t> out;
    word(out, 0x46546c67);
    word(out, 2);
    word(out, uint32_t(28 + text.size() + binary.size()));
    word(out, uint32_t(text.size()));
    word(out, 0x4e4f534a);
    out.insert(out.end(), text.begin(), text.end());
    word(out, uint32_t(binary.size()));
    word(out, 0x004e4942);
    out.insert(out.end(), binary.begin(), binary.end());
    return out;
  }
  void save(const std::filesystem::path &p) {
    auto b = bytes();
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char *>(b.data()), b.size());
  }
};
} // namespace
int main(int argc, char **argv) try {
  if (argc == 3 && std::string(argv[1]) == "--empty-project") {
    Scene{}.save(argv[2]);
    return 0;
  }
  if (argc == 3 && std::string(argv[1]) == "--inspect") {
    std::cout << Scene::load(argv[2]).encode().dump() << std::endl;
    return 0;
  }
  Fixture fixture;
  if (argc == 3 && std::string(argv[1]) == "--ui-fixture") {
    fixture.save(argv[2]);
    return 0;
  }
  auto directory = std::filesystem::temp_directory_path() /
                   ("dome-authoring-import-" + std::to_string(getpid()));
  std::filesystem::create_directory(directory);
  struct Cleanup {
    std::filesystem::path p;
    ~Cleanup() {
      std::error_code e;
      std::filesystem::remove_all(p, e);
    }
  } cleanup{directory};
  auto path = directory / "scene.glb";
  fixture.save(path);
  Scene source;
  auto unchanged = source.encode();
  ImportAuthoringOptions options;
  options.paint_resolution = 64;
  auto prepared = prepare_authoring_import(source, path, options);
  check(source.encode() == unchanged && prepared.objects.size() == 2,
        "Detached authoring preparation preserves caller and instantiates both "
        "selected source nodes");
  check(prepared.scene.groups().size() == 4 &&
            prepared.scene.objects().size() == 3,
        "Original selected node hierarchy becomes editable native groups "
        "rather than flattened world geometry");
  const auto &a = prepared.scene.object(prepared.objects[0]);
  const auto &b = prepared.scene.object(prepared.objects[1]);
  check(a.appearance->asset == b.appearance->asset &&
            a.appearance->original_bytes == b.appearance->original_bytes &&
            a.appearance->source_node == 1 && b.appearance->source_node == 2 &&
            a.appearance->source_mesh == b.appearance->source_mesh,
        "Instances retain shared immutable exact source and distinct authored "
        "node identities");
  check(
      *a.appearance->original_bytes == fixture.bytes() &&
          a.appearance->source_vertices == std::vector<uint32_t>({0, 1, 2, 3}),
      "Raw source bytes and exact accessor vertex identities remain unchanged");
  for (auto id : prepared.objects) {
    const auto &o = prepared.scene.object(id);
    const auto &m =
        o.appearance->asset->nodes.at(o.appearance->source_node).world;
    auto p = o.painted->mesh().positions.at(0);
    Vec3 expected{m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
                  m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
                  m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
    auto actual =
        ob::transform(prepared.scene.world_transform(id), {p.x, p.y, p.z});
    if (length(expected - Vec3{actual.x, actual.y, actual.z}) > 1e-6f)
      throw std::runtime_error("Native imported hierarchy does not match "
                               "independent original world matrix");
  }
  check(true, "Native hierarchy world positions match original node matrices "
              "independently of native round-trip serialization");
  const auto &maps = *a.painted->source_maps();
  check(maps.color.image->rgba == Image{2,
                                        2,
                                        {255, 0, 0, 32, 0, 255, 0, 96, 0, 0,
                                         255, 160, 255, 255, 255, 255}}
                                      .rgba &&
            maps.color.wrap_s == 33648 && maps.color.wrap_t == 33071 &&
            maps.color.mag_filter == 9728 && maps.color.min_filter == 9986,
        "Encoded PNG decoding preserves alpha pixels and authored "
        "filtering/wrap modes");
  check(a.material.unlit && a.material.alpha == Material::AlphaMode::Mask &&
            a.material.alpha_cutoff == .3f && a.material.double_sided &&
            a.material.color[3] == .6f && a.material.metallic == .25f,
        "PBR factors, unlit override, explicit alpha mask and double-sided "
        "state are adopted without lossy substitutions");
  check(a.deformation->morph_weights == std::vector<float>{.7f} &&
            b.deformation->morph_weights == std::vector<float>{.3f},
        "Node morph weights override shared mesh defaults independently for "
        "each instance");
  auto pose = a.deformation->preview(a.painted->mesh());
  check(std::abs(pose.positions[0].y - (-.1f + .007f)) < 1e-7,
        "Native POSITION morph preview matches independent original accessor "
        "arithmetic");
  const auto &frames = *a.material_bindings->normal_frames->pbr;
  check(frames.tangents[0] == std::array<float, 4>{1, 0, 0, -1} &&
            frames.targets[0].normals[0].x == .1f &&
            frames.targets[0].tangents[0].y == .1f,
        "Authored normal/tangent directions, handedness and both direction "
        "morph channels remain exact");
  auto output = directory / "roundtrip.glb";
  export_glb(prepared.scene.surfaces(), output);
  auto emitted =
      import_mesh(output, ImportLimits{.mode = ImportMode::PreserveGltf});
  check(emitted.primitives.size() == 2 &&
            emitted.primitives[0].colors ==
                a.appearance->asset->primitives[0].colors &&
            emitted.primitives[0].uv1 ==
                a.appearance->asset->primitives[0].uv1 &&
            emitted.primitives[0].tangents ==
                a.appearance->asset->primitives[0].tangents,
        "GLB round trip preserves linear RGBA, UV1 and authored tangents "
        "through independent accessor reimport");
  auto saved = directory / "scene.dsculpt";
  prepared.scene.save(saved);
  auto restarted = Scene::load(saved);
  check(restarted.encode() == prepared.scene.encode(),
        "Native save/restart retains complete source/material/frame/morph "
        "archives and hierarchy");
  auto no_uv = fixture;
  no_uv.j["meshes"][0]["primitives"][0]["attributes"].erase("TEXCOORD_0");
  no_uv.j["materials"][0]["pbrMetallicRoughness"].erase("baseColorTexture");
  auto no_uv_path = directory / "no-uv.glb";
  no_uv.save(no_uv_path);
  auto atlased = prepare_authoring_import(source, no_uv_path, options);
  auto &uv_object = atlased.scene.object(atlased.objects[0]);
  check(!uv_object.painted->mesh().uv.empty() &&
            uv_object.appearance->source_vertices.size() ==
                uv_object.painted->mesh().positions.size() &&
            uv_object.deformation->morphs->targets[0].delta.size() ==
                uv_object.painted->mesh().positions.size(),
        "Missing UV0 generates a paint atlas with exact ordered-corner remaps "
        "for source channels and deformation");
  auto no_uv_saved = directory / "no-uv.dsculpt";
  atlased.scene.save(no_uv_saved);
  check(Scene::load(no_uv_saved).encode() == atlased.scene.encode(),
        "Untextured generated atlas and all original source/morph mappings "
        "survive actual native restart");
  auto missing = fixture;
  missing.j["meshes"][0]["primitives"][0]["attributes"].erase("NORMAL");
  missing.j["meshes"][0]["primitives"][0]["targets"][0].erase("NORMAL");
  auto missing_path = directory / "missing-normal.glb";
  missing.save(missing_path);
  bool missing_refused = false;
  try {
    prepare_authoring_import(source, missing_path, options);
  } catch (const std::runtime_error &e) {
    missing_refused = std::string(e.what()).find("generated flat-normal") !=
                      std::string::npos;
  }
  check(missing_refused && source.encode() == unchanged,
        "Absent NORMAL never promotes smoothed display fallback or ignored "
        "TANGENT into authored flat-normal authority");
  for (int cut : {1, 3, 8, 16, 24}) {
    auto cancelled = options;
    int calls = 0;
    cancelled.limits.cancelled = [&] { return ++calls >= cut; };
    bool refused = false;
    try {
      prepare_authoring_import(source, path, cancelled);
    } catch (const std::runtime_error &) {
      refused = true;
    }
    check(refused && source.encode() == unchanged,
          "Cancellation at staged native import checkpoints leaves source "
          "exact and retryable");
  }
  Document document;
  document.replace(source);
  check(document.wait_mesh(), "Initial native document surface ready");
  document.import_asset(path, 64);
  check(document.wait_asset() && document.wait_mesh() &&
            document.frame().state->error.empty() &&
            document.copy_scene().objects().size() == 3,
        "Native background Import publishes the actual preserved GLB as one "
        "operation");
  auto imported = document.copy_scene().encode();
  check(document.undo() && document.wait_mesh() &&
            document.copy_scene().encode() == unchanged,
        "One global Undo restores exact scene before import");
  check(document.redo() && document.wait_mesh() &&
            document.copy_scene().encode() == imported,
        "One global Redo restores exact imported source and authoring payload");
  document.undo();
  document.wait_mesh();
  document.import_asset(path, 64);
  document.cancel_asset_work();
  check(document.wait_asset() && document.wait_mesh() &&
            document.copy_scene().encode() == unchanged,
        "Native queued import cancellation publishes no partial groups, source "
        "or history");
  document.import_asset(path, 64);
  check(document.wait_asset() && document.wait_mesh() &&
            document.frame().state->error.empty(),
        "A cancelled native import can retry successfully");
  std::cout << checks << " import authoring checks passed" << std::endl;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
