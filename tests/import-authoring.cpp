// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-authoring.hpp"
#include "dome/document.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/material-bindings.hpp"
#include "import-authoring-fixtures.hpp"
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
using Fixture = import_fixture::Fixture;
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
  auto flat = prepare_authoring_import(source, missing_path, options);
  const auto &flat_object = flat.scene.object(flat.objects[0]);
  const auto &flat_frame = *flat_object.material_bindings->normal_frames->pbr;
  check(flat_object.painted->mesh().positions.size()==6 &&
      flat_object.appearance->source_vertices==std::vector<uint32_t>{0,1,2,0,2,3} &&
      !flat_object.appearance->asset->primitives[0].normals_authored &&
      flat_object.appearance->asset->primitives[0].tangents[0][3]==-1,
      "Absent NORMAL derives split editable geometry and preserves exact original absence, vertices and ignored tangents");
  bool flat_directions=flat_frame.tangents.empty() && flat_frame.normals.size()==6 &&
      flat_frame.targets[0].normals.size()==6;
  for(size_t c=0;c<flat_frame.normals.size();++c)
    flat_directions&=length(flat_frame.normals[c]-Vec3{0,0,1})<1e-7f &&
        length(flat_frame.targets[0].normals[c])==0;
  check(flat_directions && source.encode()==unchanged,
      "No-normal-texture import ignores supplied tangents and generates base and morph flat normal authority without mutating the source");
  auto flat_saved=directory/"flat.dsculpt";
  flat.scene.save(flat_saved);
  check(Scene::load(flat_saved).encode()==flat.scene.encode(),
      "Generated flat frames, immutable raw absence and complete source deformation survive native save/restart");
  auto flat_normal_map=missing;
  flat_normal_map.j["materials"][0]["normalTexture"]={{"index",0},{"texCoord",0}};
  auto flat_normal_map_path=directory/"missing-normal-map.glb";
  flat_normal_map.save(flat_normal_map_path);
  auto mapped_flat=prepare_authoring_import(source,flat_normal_map_path,options);
  const auto &mapped_frame=*mapped_flat.scene.object(mapped_flat.objects[0]).material_bindings->normal_frames->pbr;
  check(mapped_frame.generated_tangents && mapped_frame.generated_uv==0 &&
      mapped_frame.tangents.size()==6 && mapped_frame.targets[0].tangents.size()==6 &&
      mapped_frame.tangents[0][3]==1,
      "Missing NORMAL with a UV0 normal texture generates original-basis Mikk base and morph frames instead of the supplied opposite handedness");
  auto flat_uv1=flat_normal_map;
  flat_uv1.j["materials"][0]["normalTexture"]["texCoord"]=1;
  auto flat_uv1_path=directory/"missing-normal-uv1.glb";flat_uv1.save(flat_uv1_path);
  bool uv1_guard=false;
  try { prepare_authoring_import(source,flat_uv1_path,options); }
  catch(const std::invalid_argument &e) {
    uv1_guard=std::string_view(e.what()).find("require TEXCOORD_0")!=std::string_view::npos;
  }
  check(uv1_guard && source.encode()==unchanged,
      "Existing UV1 paint-source restriction stays explicit and source-exact; full UV1 sampling/provenance remains required");
  auto flat_no_uv=no_uv;
  flat_no_uv.j["meshes"][0]["primitives"][0]["attributes"].erase("NORMAL");
  flat_no_uv.j["meshes"][0]["primitives"][0]["targets"][0].erase("NORMAL");
  auto flat_no_uv_path=directory/"missing-normal-no-uv.glb";
  flat_no_uv.save(flat_no_uv_path);
  auto atlased_flat=prepare_authoring_import(source,flat_no_uv_path,options);
  const auto &atlased_flat_object=atlased_flat.scene.object(atlased_flat.objects[0]);
  check(!atlased_flat_object.painted->mesh().uv.empty() &&
      atlased_flat_object.material_bindings->normal_frames->pbr->source_vertices==std::vector<uint32_t>{0,1,2,0,2,3},
      "Missing NORMAL and UV0 can generate an editable atlas without changing ordered original corner identities");
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
