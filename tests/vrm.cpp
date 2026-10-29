// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/vrm.hpp"
#include "cgltf.h"
#include "dome/creation.hpp"
#include "dome/import.hpp"
#include <fstream>
#include <iostream>
#include <numeric>
#include <unistd.h>
using namespace dome;
namespace {
void check(bool ok, const char *what) {
  if (!ok)
    throw std::runtime_error(what);
  std::cout << "PASS " << what << '\n';
}
template <class F> bool rejects(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &e) {
    std::cout << "REJECT " << e.what() << '\n';
    return true;
  }
}
nlohmann::json fixture() {
  using J = nlohmann::json;
  J bones = J::array();
  for (const char *role :
       {"hips", "spine", "head", "leftUpperArm", "leftLowerArm", "leftHand",
        "rightUpperArm", "rightLowerArm", "rightHand", "leftUpperLeg",
        "leftLowerLeg", "leftFoot", "rightUpperLeg", "rightLowerLeg",
        "rightFoot"})
    bones.push_back({{"bone", role}, {"node", bones.size()}});
  return {{"specVersion", "0.0"},
          {"meta",
           {{"title", "Fixture"},
            {"author", "CC0 author"},
            {"licenseName", "CC0"},
            {"texture", 0}}},
          {"humanoid", {{"humanBones", bones}}},
          {"firstPerson",
           {{"firstPersonBone", 2},
            {"meshAnnotations", {{{"mesh", 0}, {"firstPersonFlag", "Both"}}}}}},
          {"blendShapeMaster",
           {{"blendShapeGroups",
             {{{"name", "Smile"},
               {"presetName", "joy"},
               {"isBinary", false},
               {"binds", {{{"mesh", 0}, {"index", 1}, {"weight", 100}}}}}}}}},
          {"secondaryAnimation",
           {{"boneGroups",
             {{{"center", 0},
               {"bones", {1, 2}},
               {"colliderGroups", {0}},
               {"stiffiness", .5},
               {"dragForce", .2}}}},
            {"colliderGroups",
             {{{"node", 1},
               {"colliders",
                {{{"offset", {{"x", 0}, {"y", 0}, {"z", 0}}},
                  {"radius", .1}}}}}}}}},
          {"materialProperties",
           {{{"name", "Mat"},
             {"shader", "VRM/MToon"},
             {"textureProperties", {{"_MainTex", 0}}}}}}};
}
Vec3 transformed(const float *m, Vec3 p) {
  return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
          m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
          m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
}
void compare_skin(cgltf_data *data, const std::vector<ObjectSurface> &objects) {
  for (size_t i = 0; i < objects.size(); ++i) {
    const auto &object = objects[i];
    auto expected = object.preview_mesh();
    auto &primitive = data->meshes[i].primitives[0];
    cgltf_skin *skin = nullptr;
    for (size_t n = 0; n < data->nodes_count; ++n)
      if (data->nodes[n].mesh == &data->meshes[i])
        skin = data->nodes[n].skin;
    if (!skin)
      throw std::runtime_error("Missing independent skin");
    for (size_t v : {size_t(0), object.mesh.positions.size() / 2,
                     object.mesh.positions.size() - 1}) {
      auto positions =
          cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
      float authored[3];
      if (!positions || !cgltf_accessor_read_float(positions, v, authored, 3))
        throw std::runtime_error("Missing independent source-basis position");
      Vec3 p{authored[0], authored[1], authored[2]};
      for (size_t t = 0; t < data->meshes[i].weights_count; ++t)
        if (data->meshes[i].weights[t]) {
          if (t >= primitive.targets_count)
            throw std::runtime_error("Missing independent morph target");
          cgltf_accessor *delta = nullptr;
          auto &target = primitive.targets[t];
          for (size_t attribute = 0; attribute < target.attributes_count;
               ++attribute)
            if (target.attributes[attribute].type ==
                cgltf_attribute_type_position)
              delta = target.attributes[attribute].data;
          float value[3];
          if (!delta || !cgltf_accessor_read_float(delta, v, value, 3))
            throw std::runtime_error("Invalid independent morph accessor");
          p += Vec3{value[0], value[1], value[2]} * data->meshes[i].weights[t];
        }
      Vec3 weighted{};
      for (size_t set = 0;; ++set) {
        auto joints = cgltf_find_accessor(
            &primitive, cgltf_attribute_type_joints, int(set));
        auto weights = cgltf_find_accessor(
            &primitive, cgltf_attribute_type_weights, int(set));
        if (!joints)
          break;
        float slots[4], values[4];
        if (!weights || !cgltf_accessor_read_float(joints, v, slots, 4) ||
            !cgltf_accessor_read_float(weights, v, values, 4))
          throw std::runtime_error("Invalid independent influence sets");
        for (size_t k = 0; k < 4; ++k)
          if (values[k]) {
            size_t joint = size_t(slots[k]);
            float ibm[16], world[16];
            if (joint >= skin->joints_count ||
                !cgltf_accessor_read_float(skin->inverse_bind_matrices, joint,
                                           ibm, 16))
              throw std::runtime_error("Invalid independent inverse bind");
            cgltf_node_transform_world(skin->joints[joint], world);
            weighted += transformed(world, transformed(ibm, p)) * values[k];
          }
      }
      auto reference = expected.positions[v];
      if (object.appearance && object.appearance->source_basis_quaternion)
        reference =
            object.appearance->source_basis_quaternion->inverse().rotate(
                reference);
      if (length(weighted - reference) > 3e-4f)
        throw std::runtime_error("Independent real-avatar joint/IBM evaluation "
                                 "differs from native preview");
    }
  }
}
uint32_t word(const std::vector<uint8_t> &b, size_t p) {
  return uint32_t(b.at(p)) | uint32_t(b.at(p + 1)) << 8 |
         uint32_t(b.at(p + 2)) << 16 | uint32_t(b.at(p + 3)) << 24;
}
std::vector<uint8_t> read(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    throw std::runtime_error("Cannot read fixture");
  return {std::istreambuf_iterator<char>(f), {}};
}
} // namespace
int main(int argc, char **argv) try {
  auto source = fixture();
  source["meta"]["customLicenseNote"] = "Keep exactly";
  auto metadata = Vrm0Metadata::parse(source, 15, 1, 1, 1);
  check(metadata.source() == source, "VRM0 native metadata retains complete "
                                     "source including custom license fields");
  Vrm0IdentityMap map;
  map.nodes.resize(15);
  std::iota(map.nodes.begin(), map.nodes.end(), 100);
  map.textures = {40};
  map.meshes = {{5, 7}};
  map.morph_targets = {{5, {0, 1}}, {7, {1, 0}}};
  auto result = metadata.remap(map);
  check(result["humanoid"]["humanBones"][0]["node"] == 100 &&
            result["firstPerson"]["firstPersonBone"] == 102 &&
            result["meta"]["texture"] == 40,
        "Humanoid, first-person and thumbnail use exact emitted identities");
  auto binds = result["blendShapeMaster"]["blendShapeGroups"][0]["binds"];
  check(binds.size() == 2 && binds[0]["mesh"] == 5 && binds[0]["index"] == 1 &&
            binds[1]["mesh"] == 7 && binds[1]["index"] == 0,
        "Split source meshes expand expressions with independently reordered "
        "morph identities");
  check(result["firstPerson"]["meshAnnotations"].size() == 2 &&
            result["secondaryAnimation"]["boneGroups"][0]["bones"][1] == 102 &&
            result["secondaryAnimation"]["colliderGroups"][0]["node"] == 101,
        "Mesh annotations, spring roots/centers and collider nodes remap "
        "explicitly");
  check(result["materialProperties"][0]["textureProperties"]["_MainTex"] ==
                40 &&
            result["meta"]["customLicenseNote"] == "Keep exactly",
        "MToon texture and full licensing provenance survive remapping");
  auto broken = source;
  broken["humanoid"]["humanBones"][0]["node"] = 1.5;
  check(rejects([&] { Vrm0Metadata::parse(broken, 15, 1, 1, 1); }),
        "Fractional node references reject");
  broken = source;
  broken["humanoid"]["humanBones"].erase(0);
  check(rejects([&] { Vrm0Metadata::parse(broken, 15, 1, 1, 1); }),
        "Incomplete humanoid mappings reject");
  broken = source;
  broken["meta"]["licenseName"] = "Unspecified";
  check(rejects([&] { Vrm0Metadata::parse(broken, 15, 1, 1, 1); }),
        "Unknown license enum rejects without changing source permissions");
  broken = source;
  broken["secondaryAnimation"]["boneGroups"][0]["colliderGroups"] = {5};
  check(rejects([&] { Vrm0Metadata::parse(broken, 15, 1, 1, 1); }),
        "Spring references outside original collider tables reject");
  broken = source;
  broken["specVersion"] = "1.0";
  check(rejects([&] { Vrm0Metadata::parse(broken, 15, 1, 1, 1); }),
        "VRM1 is never silently interpreted as VRM0");
  auto missing = map;
  missing.nodes[2] = -1;
  check(rejects([&] { metadata.remap(missing); }),
        "Missing source joint identities reject instead of inferring names");
  missing = map;
  missing.morph_targets[7] = {0, -1};
  check(rejects([&] { metadata.remap(missing); }),
        "Missing source morph identities reject even when target counts match");
  for (int arg = 1; arg < argc; ++arg) {
    std::string gender = argv[arg];
    auto directory = std::filesystem::path("artifacts/vrm-export-" + gender);
    std::filesystem::create_directories(directory);
    Scene scene;
    auto group = append_humanoid(
        scene, humanoid_baseline_defaults("vroid-beta-" + gender), "Avatar", {},
        64);
    auto native = directory / "source.dsculpt";
    scene.save(native);
    auto reopened = Scene::load(native);
    auto objects = reopened.preview_surfaces();
    std::erase_if(objects, [&](auto &o) {
      return o.groups.empty() || o.groups.back().id != group;
    });
    check(objects.size() == 19, "Actual CC0 avatar restarts from native Scene "
                                "with all19 source primitives");
    auto path = directory / "avatar.vrm";
    auto report = export_vrm(objects, path);
    std::cout << "NORMAL_POLICY " << gender
              << " recomputed=" << report.normal_targets_recomputed << '\n';
    cgltf_options options{};
    cgltf_data *raw = nullptr;
    check(cgltf_parse_file(&options, path.c_str(), &raw) ==
              cgltf_result_success,
          "Independent cgltf parser accepts emitted VRM container");
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> parsed(raw, cgltf_free);
    check(cgltf_load_buffers(&options, raw, path.c_str()) ==
                  cgltf_result_success &&
              cgltf_validate(raw) == cgltf_result_success,
          "Independent cgltf validates all avatar accessors, images and "
          "hierarchy");
    check(raw->skins_count == 1 && raw->meshes_count == 19 &&
              raw->skins[0].joints_count ==
                  objects[0].deformation->skeleton->bones.size(),
          "All19 avatar meshes bind one actual shared source-derived rig");
    compare_skin(raw, objects);
    check(true, "Independent real-avatar skin/IBM evaluation matches native "
                "preview on all19 parts");
    auto bytes = read(path);
    auto json = nlohmann::json::parse(bytes.begin() + 20,
                                      bytes.begin() + 20 + word(bytes, 12));
    auto vrm = json.at("extensions").at("VRM");
    auto original = nlohmann::json::parse(*objects[0].appearance->vrm_json);
    auto expected_meta = original.at("meta");
    expected_meta["texture"] = vrm.at("meta").at("texture");
    check(vrm.at("meta") == expected_meta,
          "Full original author, license, permissions and thumbnail metadata "
          "are retained");
    check(vrm.at("secondaryAnimation").at("boneGroups").size() ==
                  original.at("secondaryAnimation").at("boneGroups").size() &&
              vrm.at("firstPerson").at("lookAtHorizontalInner") ==
                  original.at("firstPerson").at("lookAtHorizontalInner"),
          "Spring groups and authored look-at curves survive actual source "
          "export");
    const auto &appearance = *objects[0].appearance;
    float fit_scale = 1;
    const auto &bones = *objects[0].deformation->skeleton;
    for (size_t joint = 0; joint < bones.bones.size(); ++joint) {
      auto original_node = appearance.source_joint_nodes[joint];
      auto t = appearance.asset->nodes[original_node].translation;
      float distance = length(Vec3{t[0], t[1], t[2]});
      if (bones.bones[joint].parent >= 0 && distance > .01f) {
        fit_scale = length(bones.bones[joint].rest.translation) / distance;
        break;
      }
    }
    auto original_offset =
        original.at("firstPerson").at("firstPersonBoneOffset");
    for (const char *axis : {"x", "y", "z"})
      check(std::abs(vrm.at("firstPerson")
                         .at("firstPersonBoneOffset")
                         .at(axis)
                         .get<float>() -
                     original_offset.at(axis).get<float>() * fit_scale) < 2e-6f,
            "First-person bone-local offset follows independently measured "
            "uniform source scale");
    if (!original["secondaryAnimation"]["colliderGroups"].empty()) {
      float old_radius = original.at("secondaryAnimation")
                             .at("colliderGroups")[0]
                             .at("colliders")[0]
                             .at("radius");
      check(std::abs(vrm.at("secondaryAnimation")
                         .at("colliderGroups")[0]
                         .at("colliders")[0]
                         .at("radius")
                         .get<float>() -
                     old_radius * fit_scale) < 2e-6f,
            "Collider radius scales with actual native/source joint distances");
    } else
      check(vrm["secondaryAnimation"]["colliderGroups"].empty(),
            "Avatar without authored colliders does not invent collision "
            "metadata");
    auto native_worlds = objects[0].deformation->skeleton->world();
    bool collider_frames = true;
    for (size_t g = 0;
         g < original["secondaryAnimation"]["colliderGroups"].size(); ++g) {
      auto source_node =
          original["secondaryAnimation"]["colliderGroups"][g]["node"]
              .get<uint32_t>();
      auto found = std::find(appearance.source_joint_nodes.begin(),
                             appearance.source_joint_nodes.end(), source_node);
      if (found == appearance.source_joint_nodes.end())
        throw std::runtime_error(
            "Source collider lacks explicit joint provenance");
      size_t joint = size_t(found - appearance.source_joint_nodes.begin());
      auto output_node =
          vrm["secondaryAnimation"]["colliderGroups"][g]["node"].get<size_t>();
      float output_world[16];
      cgltf_node_transform_world(&raw->nodes[output_node], output_world);
      auto &colliders =
          original["secondaryAnimation"]["colliderGroups"][g]["colliders"];
      for (size_t c = 0; c < colliders.size(); ++c) {
        auto offset = colliders[c]["offset"];
        Vec3 local{offset["x"], offset["y"], offset["z"]};
        auto native_local =
            appearance.source_basis_quaternion->rotate(local * fit_scale);
        auto reference = appearance.source_basis_quaternion->inverse().rotate(
            transformed(native_worlds[joint].data(), native_local));
        auto exported = vrm["secondaryAnimation"]["colliderGroups"][g]
                           ["colliders"][c]["offset"];
        auto actual = transformed(
            output_world, {exported["x"], exported["y"], exported["z"]});
        collider_frames &= length(actual - reference) < 2e-5f;
      }
    }
    check(collider_frames, "Independent collider world centers agree after "
                           "basis restoration and fitted local offsets");
    check(report.normal_targets_recomputed == 0,
          "Neutral source basis conversion preserves authored morph normals "
          "without geometric replacement");
    for (size_t i = 0; i < objects.size(); ++i) {
      auto normals = cgltf_find_accessor(&raw->meshes[i].primitives[0],
                                         cgltf_attribute_type_normal, 0);
      float normal[3];
      check(normals && cgltf_accessor_read_float(normals, 0, normal, 3),
            "Independent source-basis normal accessor is readable");
      auto expected =
          objects[i].appearance->source_basis_quaternion->inverse().rotate(
              objects[i].mesh.normals[0]);
      check(length(Vec3{normal[0], normal[1], normal[2]} - expected) < 1e-6f,
            "Exported normal follows explicit inverse source basis");
    }
    size_t morph_normals = 0;
    for (auto &m : json.at("meshes"))
      for (auto &p : m.at("primitives"))
        for (auto &t : p.value("targets", nlohmann::json::array()))
          morph_normals += t.contains("NORMAL");
    size_t expected_normals = 0;
    for (auto &p : objects[0].appearance->asset->primitives)
      for (auto &t : p.morph_targets)
        expected_normals += !t.normals.empty();
    check(morph_normals == expected_normals,
          "Every source NORMAL morph channel survives native compacted vertex "
          "export");
    auto raw_objects = objects;
    for (auto &part : raw_objects)
      part.textures = reopened.object(part.id).painted->preview(part.material);
    auto raw_path = directory / "raw-avatar.vrm";
    auto raw_report = export_vrm(raw_objects, raw_path);
    auto raw_bytes = read(raw_path);
    auto raw_json = nlohmann::json::parse(
        raw_bytes.begin() + 20, raw_bytes.begin() + 20 + word(raw_bytes, 12));
    bool source_colors = true;
    for (size_t i = 0; i < raw_objects.size(); ++i) {
      auto source_material = raw_objects[i].appearance->source_material;
      source_colors &= raw_json["extensions"]["VRM"]["materialProperties"][i]
                               ["vectorProperties"]["_Color"] ==
                       original["materialProperties"][source_material]
                               ["vectorProperties"]["_Color"];
    }
    check(source_colors && raw_report.normal_targets_recomputed == 0,
          "Raw-source VRM preserves original gamma MToon colors and authored "
          "morph normals exactly");
    size_t tinted_part = SIZE_MAX;
    for (size_t i = 0; i < raw_objects.size(); ++i)
      if (raw_objects[i].material.color[0] == 0) {
        tinted_part = i;
        break;
      }
    if (tinted_part == SIZE_MAX)
      throw std::runtime_error(
          "Actual fixture lacks explicit zero-factor channel");
    raw_objects[tinted_part].material.color[0] = .25f;
    auto tinted_path = directory / "tinted-avatar.vrm";
    export_vrm(raw_objects, tinted_path);
    auto tinted_bytes = read(tinted_path);
    auto tinted_json = nlohmann::json::parse(tinted_bytes.begin() + 20,
                                             tinted_bytes.begin() + 20 +
                                                 word(tinted_bytes, 12));
    auto tinted_properties =
        tinted_json["extensions"]["VRM"]["materialProperties"][tinted_part];
    float gamma = .25f <= .0031308f ? .25f * 12.92f
                                    : 1.055f * std::pow(.25f, 1 / 2.4f) - .055f;
    check(std::abs(
              tinted_properties["vectorProperties"]["_Color"][0].get<float>() -
              gamma) < 2e-6f,
          "Edited zero-source MToon channels use explicit absolute color "
          "without division or double gamma");
    auto posed = objects;
    for (auto &part : posed) {
      auto deformation = std::make_shared<DeformationState>(*part.deformation);
      deformation->pose.clear();
      for (auto &bone : deformation->skeleton->bones)
        deformation->pose.push_back(bone.rest);
      auto head = deformation->humanoid->joints.at("head");
      deformation->pose[head].rotation =
          (Quat{std::cos(.17f), std::sin(.17f), 0, 0} *
           deformation->pose[head].rotation)
              .unit();
      if (deformation->morphs && deformation->morphs->targets.size() > 37) {
        deformation->morph_weights.assign(deformation->morphs->targets.size(),
                                          0);
        deformation->morph_weights[37] = .6f;
      }
      part.deformation = deformation;
    }
    auto posed_path = directory / "posed.vrm";
    export_vrm(posed, posed_path);
    cgltf_data *pose_raw = nullptr;
    check(cgltf_parse_file(&options, posed_path.c_str(), &pose_raw) ==
              cgltf_result_success,
          "Independent parser accepts actual edited avatar pose");
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> pose_data(pose_raw,
                                                                 cgltf_free);
    check(cgltf_load_buffers(&options, pose_raw, posed_path.c_str()) ==
                  cgltf_result_success &&
              cgltf_validate(pose_raw) == cgltf_result_success,
          "Independent validator accepts shared avatar pose");
    compare_skin(pose_raw, posed);
    auto source_head =
        original["firstPerson"]["firstPersonBone"].get<uint32_t>();
    auto head_found =
        std::find(appearance.source_joint_nodes.begin(),
                  appearance.source_joint_nodes.end(), source_head);
    if (head_found == appearance.source_joint_nodes.end())
      throw std::runtime_error("Source camera head lacks identity");
    auto pose_worlds =
        posed[0].deformation->skeleton->world(posed[0].deformation->pose);
    Vec3 camera_offset{original_offset["x"], original_offset["y"],
                       original_offset["z"]};
    auto camera_reference =
        appearance.source_basis_quaternion->inverse().rotate(transformed(
            pose_worlds[size_t(head_found -
                               appearance.source_joint_nodes.begin())]
                .data(),
            appearance.source_basis_quaternion->rotate(camera_offset *
                                                       fit_scale)));
    size_t camera_node = vrm["firstPerson"]["firstPersonBone"].get<size_t>();
    float camera_world[16];
    cgltf_node_transform_world(&pose_raw->nodes[camera_node], camera_world);
    auto exported_camera = vrm["firstPerson"]["firstPersonBoneOffset"];
    check(length(transformed(camera_world,
                             {exported_camera["x"], exported_camera["y"],
                              exported_camera["z"]}) -
                 camera_reference) < 2e-5f,
          "Independent posed first-person camera anchor follows restored "
          "source bone-local frame");

    check(true, "Independent source-derived head pose evaluates consistently "
                "across all19 meshes");
    auto corrupted = objects;
    auto corrupt_appearance =
        std::make_shared<CreatorAppearance>(*corrupted[0].appearance);
    corrupt_appearance->source_joint_nodes.clear();
    corrupted[0].appearance = corrupt_appearance;
    auto no_basis = objects;
    auto missing_basis =
        std::make_shared<CreatorAppearance>(*objects[0].appearance);
    missing_basis->source_basis_quaternion.reset();
    no_basis[0].appearance = missing_basis;
    check(rejects([&] { export_vrm(no_basis, path); }),
          "Legacy unknown source basis cannot export coordinate-dependent VRM "
          "metadata");
    auto before = read(path);
    auto ambiguous = objects;
    ambiguous[0].textures =
        reopened.object(objects[0].id).painted->composite(objects[0].material);
    check(rejects([&] { export_vrm(ambiguous, path); }) && read(path) == before,
          "Derived MToon without coverage rejects instead of silently losing "
          "shadow paint");

    auto nonlinear = objects;
    nonlinear[0].mesh.positions[0].x += .02f;
    auto edited = std::make_shared<DeformationState>(*nonlinear[0].deformation);
    edited->source = morph_topology_id(nonlinear[0].mesh);
    if (edited->morphs)
      edited->morphs->topology = edited->source;
    nonlinear[0].deformation = edited;
    check(rejects([&] { export_vrm(nonlinear, path); }) && read(path) == before,
          "Nonlinear rest edits reject until local physics metadata is "
          "explicitly refitted");

    check(rejects([&] { export_vrm(corrupted, path); }) && read(path) == before,
          "Missing legacy joint map cannot replace an existing valid avatar "
          "file");
    VrmExportOptions cancelled;
    cancelled.cancelled = [] { return true; };
    check(rejects([&] { export_vrm(objects, path, cancelled); }) &&
              read(path) == before,
          "Cancellation preserves existing avatar output exactly");
    std::cout << "AVATAR " << gender << " bytes=" << bytes.size()
              << " joints=" << raw->skins[0].joints_count << " output=" << path
              << '\n';
  }
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
