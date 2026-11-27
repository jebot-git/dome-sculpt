// SPDX-License-Identifier: GPL-3.0-or-later
#include "cgltf.h"
#include "dome/document.hpp"
#include "dome/import-authoring.hpp"
#include "dome/rig-edit.hpp"
#include "dome/rig-rest.hpp"
#include "dome/tangents.hpp"
#include "dome/texture-sets.hpp"
#include "dome/vrm.hpp"
#include "dome/emission-native.hpp"
#include "dome/native-authoring.hpp"
#include "dome/native-scene-append.hpp"
#include "dome/source-placement.hpp"
#include "emission-export.hpp"
#include "native-surface-heat.hpp"
#include "source-rigid-pose.hpp"
#include "source-skin-link.hpp"
#include "vrm1-metadata.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#define main matcap_fixture_main
#include "vrm-matcap-import.cpp"
#undef main
namespace {
using J = nlohmann::json;
std::vector<uint8_t> bytes(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
J json(const std::filesystem::path &p) {
  auto b = bytes(p);
  auto count = uint32_t(b.at(12)) | uint32_t(b.at(13)) << 8 |
               uint32_t(b.at(14)) << 16 | uint32_t(b.at(15)) << 24;
  return J::parse(b.begin() + 20, b.begin() + 20 + count);
}
import_fixture::Fixture metadata_source() {
  auto f = source(false);
  f.j["meshes"][0]["extras"]["authorOpaque"] = {
      {"text", "Original mesh 日本語"}, {"values", {1, 3, 7}}};
  f.j["meshes"][0]["primitives"][0]["extras"] = {
      {"authorOpaque", {{"flag", true}, {"value", .125f}}}};
  import_fixture::Fixture original;
  for (const char *uv : {"TEXCOORD_0", "TEXCOORD_1"})
    f.j["meshes"][0]["primitives"][0]["attributes"][uv] =
        original.j["meshes"][0]["primitives"][0]["attributes"][uv];
  f.j["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"] = {
      {"index", 0},
      {"extensions",
       {{"KHR_texture_transform",
         {{"offset", {.07f, -.11f}},
          {"rotation", .19f},
          {"scale", {.8f, 1.2f}},
          {"texCoord", 1}}}}}};
  f.j["materials"][0]["extensions"]["VRMC_materials_mtoon"]
     ["shadeMultiplyTexture"] = {{"index", 0}, {"texCoord", 1}};
  f.j["extensionsUsed"].push_back("KHR_texture_transform");
  auto &vrm = f.j["extensions"]["VRMC_vrm"];
  vrm["firstPerson"]["meshAnnotations"] = {
      {{"node", 1}, {"type", "both"}},
      {{"node", 2}, {"type", "thirdPersonOnly"}}};
  vrm["meta"]["thumbnailImage"] = 0;
  vrm["lookAt"] = {{"type", "expression"},
                   {"offsetFromHeadBone", {.01f, .02f, -.03f}},
                   {"rangeMapHorizontalInner",
                    {{"inputMaxValue", 90}, {"outputScale", .7f}}}};
  vrm["expressions"]["preset"]["happy"] = {
      {"isBinary", true},
      {"overrideBlink", "blend"},
      {"morphTargetBinds", {{{"node", 1}, {"index", 0}, {"weight", .35f}}}},
      {"materialColorBinds",
       {{{"material", 0},
         {"type", "color"},
         {"targetValue", {.4f, .5f, .6f, .7f}}}}},
      {"textureTransformBinds",
       {{{"material", 0}, {"scale", {.7f, .9f}}, {"offset", {.2f, -.1f}}}}}};
  const auto head = vrm["humanoid"]["humanBones"]["head"]["node"].get<size_t>();
  const auto root = f.j["nodes"].size();
  // Authored, genuinely weighted skins with different mesh bind spaces. The
  // third part below is genuinely rigid, attached to that same weighted head.
  while (f.bin.size() % 4)
    f.bin.push_back(0);
  auto start = f.bin.size();
  for (size_t v = 0; v < 4; ++v)
    for (size_t k = 0; k < 4; ++k) {
      f.bin.push_back(0);
      f.bin.push_back(0);
    }
  const auto view = f.j["bufferViews"].size();
  f.j["bufferViews"].push_back({{"buffer", 0},
                                {"byteOffset", start},
                                {"byteLength", f.bin.size() - start}});
  const auto joints = f.j["accessors"].size();
  f.j["accessors"].push_back({{"bufferView", view},
                              {"componentType", 5123},
                              {"count", 4},
                              {"type", "VEC4"}});
  auto weights =
      f.values({1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, 4, "VEC4");
  f.j["meshes"][0]["primitives"][0]["attributes"]["JOINTS_0"] = joints;
  f.j["meshes"][0]["primitives"][0]["attributes"]["WEIGHTS_0"] = weights;
  auto one =
      f.values({1.2f, 0, 0, 0, 0, 1.2f, 0, 0, 0, 0, 1.2f, 0, .2f, -.3f, 0, 1},
               16, "MAT4");
  auto two = f.values({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -.3f, -.3f, -.2f, 1},
                      16, "MAT4");
  f.j["skins"] = {{{"name", "First exact head bind"},
                   {"joints", {head}},
                   {"inverseBindMatrices", one}},
                  {{"name", "Second exact head bind"},
                   {"joints", {head}},
                   {"inverseBindMatrices", two}}};
  f.j["nodes"][1]["skin"] = 0;
  f.j["nodes"][2]["skin"] = 1;
  f.j["nodes"][head]["children"].push_back(root);
  f.j["nodes"].push_back({{"name", "Actual spring root"},
                          {"translation", {0, .03f, 0}},
                          {"children", {root + 1}}});
  f.j["nodes"].push_back(
      {{"name", "Actual spring tail"}, {"translation", {0, .04f, 0}}});
  f.j["extensions"]["VRMC_springBone"] = {
      {"specVersion", "1.0"},
      {"colliders",
       {{{"node", head},
         {"shape",
          {{"sphere", {{"offset", {.01f, .02f, .03f}}, {"radius", .015f}}}}}}}},
      {"colliderGroups", {{{"name", "Actual collider"}, {"colliders", {0}}}}},
      {"springs",
       {{{"name", "Explicit ordered spring"},
         {"joints",
          {{{"node", root},
            {"stiffness", .4f},
            {"gravityDir", {.2f, -.9f, .1f}}},
           {{"node", root + 1}, {"dragForce", .3f}}}},
         {"center", head},
         {"colliderGroups", {0}}}}}};
  for (const char *kind : {"roll", "aim", "rotation"}) {
    auto id = f.j["nodes"].size();
    J constraint = {{"source", head}, {"weight", .7f}};
    if (std::string_view(kind) == "roll")
      constraint["rollAxis"] = "Y";
    if (std::string_view(kind) == "aim")
      constraint["aimAxis"] = "PositiveZ";
    f.j["nodes"][0]["children"].push_back(id);
    f.j["nodes"].push_back(
        {{"name", std::string("Actual ") + kind + " constraint"},
         {"extensions",
          {{"VRMC_node_constraint",
            {{"specVersion", "1.0"}, {"constraint", {{kind, constraint}}}}}}}});
  }
  const auto accessory = f.j["nodes"].size();
  f.j["nodes"][head]["children"].push_back(accessory);
  f.j["nodes"].push_back({{"name", "Genuine rigid head accessory"},
                          {"mesh", 0},
                          {"translation", {.1f, .2f, -.1f}},
                          {"weights", {.6f}}});
  // A real unweighted source-joint chain exceeds the editable-group cap, while
  // remaining within the unchanged rig/node quotas. Every original node and
  // authored IBM stays in source authority; only mesh-ancestor placement groups
  // are needed to present the three actual surfaces.
  const auto hair = f.j["nodes"].size();
  for (size_t i = 0; i < 130; ++i) {
    auto n = f.j["nodes"].size();
    auto parent = i ? n - 1 : head;
    f.j["nodes"][parent]["children"].push_back(n);
    f.j["nodes"].push_back(
        {{"name", "Original unweighted hair joint " + std::to_string(i)},
         {"translation", {0, .001f, 0}}});
  }
  for (size_t skin = 0; skin < 2; ++skin) {
    while (f.bin.size() % 4)
      f.bin.push_back(0);
    auto offset = f.bin.size();
    for (size_t joint = 0; joint <= 130; ++joint) {
      std::array<float, 16> matrix{1, 0, 0, 0, 0, 1, 0, 0,
                                   0, 0, 1, 0, 0, 0, 0, 1};
      if (!joint) {
        if (skin == 0) {
          matrix[0] = matrix[5] = matrix[10] = 1.2f;
          matrix[12] = .2f;
          matrix[13] = -.3f;
        } else {
          matrix[12] = -.3f;
          matrix[13] = -.3f;
          matrix[14] = -.2f;
        }
      }
      for (float value : matrix)
        import_fixture::word(f.bin, std::bit_cast<uint32_t>(value));
      if (joint)
        f.j["skins"][skin]["joints"].push_back(hair + joint - 1);
    }
    auto view = f.j["bufferViews"].size();
    f.j["bufferViews"].push_back({{"buffer", 0},
                                  {"byteOffset", offset},
                                  {"byteLength", f.bin.size() - offset}});
    auto accessor = f.j["accessors"].size();
    f.j["accessors"].push_back({{"bufferView", view},
                                {"componentType", 5126},
                                {"count", 131},
                                {"type", "MAT4"}});
    f.j["skins"][skin]["inverseBindMatrices"] = accessor;
  }
  for (auto &node : f.j["nodes"])
    if (node.value("name", std::string{}) == "Actual rotation constraint")
      node["extensions"]["VRMC_node_constraint"]["constraint"]["rotation"]
          ["source"] = hair + 129;
  f.j["extensionsUsed"].push_back("VRMC_springBone");
  f.j["extensionsUsed"].push_back("VRMC_node_constraint");
  return f;
}
void independent(const Scene &scene, const std::filesystem::path &output) {
  cgltf_options options{};
  cgltf_data *raw = nullptr;
  check(cgltf_parse_file(&options, output.c_str(), &raw) ==
            cgltf_result_success,
        "Independent cgltf consumer parses actual source-version V1 output");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> owner(raw, cgltf_free);
  check(cgltf_load_buffers(&options, raw, output.c_str()) ==
                cgltf_result_success &&
            cgltf_validate(raw) == cgltf_result_success,
        "Independent output accessors, index tables and buffers are valid");
  auto surfaces = scene.preview_surfaces();
  size_t parts = 0, corners = 0;
  float error = 0;
  const auto original_bytes = surfaces.front().appearance->original_bytes;
  cgltf_data *original = nullptr;
  check(cgltf_parse(&options, original_bytes->data(), original_bytes->size(),
                    &original) == cgltf_result_success,
        "Independent reader captures the same immutable original GLB bytes");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> original_owner(original,
                                                                    cgltf_free);
  check(
      cgltf_load_buffers(&options, original, nullptr) == cgltf_result_success,
      "Independent original accessor buffers load without filesystem re-read");
  for (size_t n = 0; n < raw->nodes_count; ++n) {
    const auto &node = raw->nodes[n];
    if (!node.mesh)
      continue;
    const auto mesh = size_t(node.mesh - raw->meshes);
    check(mesh < surfaces.size(),
          "Every emitted current primitive has one native surface witness");
    const auto &surface = surfaces[mesh];
    auto expected = surface.preview_mesh();
    auto world = surface.world_matrix();
    check(bool(node.skin) ==
              bool(surface.deformation && surface.deformation->skin),
          "Real source skins and genuine rigid parts retain their respective "
          "ownership without dummy skins");
    std::array<float, 16> node_world;
    cgltf_node_transform_world(&node, node_world.data());
    const auto &primitive = node.mesh->primitives[0];
    auto attribute = [](const cgltf_attribute *attrs, size_t count,
                        cgltf_attribute_type type,
                        int set = 0) -> const cgltf_accessor * {
      for (size_t i = 0; i < count; ++i)
        if (attrs[i].type == type && attrs[i].index == set)
          return attrs[i].data;
      return nullptr;
    };
    const auto *position =
        attribute(primitive.attributes, primitive.attributes_count,
                  cgltf_attribute_type_position);
    check(position, "Emitted surface has actual POSITION accessor");
    auto count = primitive.indices ? primitive.indices->count : position->count;
    check(
        count == expected.indices.size(),
        "Every ordered current triangle corner survives source graph emission");
    const auto &appearance = *surface.appearance;
    check(appearance.source_mesh >= 0 && appearance.source_primitive >= 0 &&
              size_t(appearance.source_mesh) < original->meshes_count &&
              size_t(appearance.source_primitive) < original->meshes[appearance.source_mesh].primitives_count,
          "Original mesh/primitive identities authorize the independent sampling oracle");
    const auto &original_primitive = original->meshes[appearance.source_mesh].primitives[appearance.source_primitive];
    for (int set = 0; set < 8; ++set) {
      const auto *before_uv = attribute(original_primitive.attributes, original_primitive.attributes_count, cgltf_attribute_type_texcoord, set);
      if (!before_uv) continue;
      const auto *current_uv = attribute(primitive.attributes, primitive.attributes_count, cgltf_attribute_type_texcoord, set);
      check(current_uv != nullptr, "Every real authored UV set remains an emitted accessor");
      const auto original_count = original_primitive.indices ? original_primitive.indices->count : before_uv->count;
      check(original_count == count, "Original ordered source corners match the actual emitted sampling domain");
      for (size_t corner = 0; corner < count; ++corner) {
        const auto old_vertex = original_primitive.indices ? cgltf_accessor_read_index(original_primitive.indices, corner) : corner;
        const auto new_vertex = primitive.indices ? cgltf_accessor_read_index(primitive.indices, corner) : corner;
        float before_value[2], current_value[2];
        check(cgltf_accessor_read_float(before_uv, old_vertex, before_value, 2) &&
                  cgltf_accessor_read_float(current_uv, new_vertex, current_value, 2) &&
                  std::bit_cast<uint32_t>(before_value[0]) == std::bit_cast<uint32_t>(current_value[0]) &&
                  std::bit_cast<uint32_t>(before_value[1]) == std::bit_cast<uint32_t>(current_value[1]),
              "Independent source UV float bits remain exact at every ordered emitted corner");
      }
    }
    auto point = [](const std::array<float, 16> &m, Vec3 p) {
      return Vec3{m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
                  m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
                  m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
    };
    std::vector<std::array<float, 16>> skin;
    if (node.skin)
      for (size_t j = 0; j < node.skin->joints_count; ++j) {
        std::array<float, 16> w,
            b{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, product{};
        cgltf_node_transform_world(node.skin->joints[j], w.data());
        if (node.skin->inverse_bind_matrices)
          check(cgltf_accessor_read_float(node.skin->inverse_bind_matrices, j,
                                          b.data(), 16),
                "Independent authored per-skin inverse bind decodes");
        for (size_t c = 0; c < 4; ++c)
          for (size_t r = 0; r < 4; ++r)
            for (size_t k = 0; k < 4; ++k)
              product[c * 4 + r] += w[k * 4 + r] * b[c * 4 + k];
        skin.push_back(product);
      }
    if (node.skin) {
      const auto &a = *surface.appearance;
      const auto *before = original->nodes[size_t(a.source_node)].skin;
      check(before && a.source_joint_nodes.size() == node.skin->joints_count,
            "Skin/joint correspondence uses authenticated original node IDs, "
            "not names");
      for (size_t j = 0; j < a.source_joint_nodes.size(); ++j) {
        std::array<float, 16> wanted{1, 0, 0, 0, 0, 1, 0, 0,
                                     0, 0, 1, 0, 0, 0, 0, 1},
            actual{};
        for (size_t old = 0; old < before->joints_count; ++old)
          if (before->joints[old] ==
                  original->nodes + a.source_joint_nodes[j] &&
              before->inverse_bind_matrices)
            check(cgltf_accessor_read_float(before->inverse_bind_matrices, old,
                                            wanted.data(), 16),
                  "Original authored IBM accessor independently decodes");
        check(cgltf_accessor_read_float(node.skin->inverse_bind_matrices, j,
                                        actual.data(), 16),
              "Current bone-aligned IBM accessor independently decodes");
        check(std::equal(actual.begin(), actual.end(), wanted.begin(),
                         [](float x, float y) {
                           return std::bit_cast<uint32_t>(x) ==
                                  std::bit_cast<uint32_t>(y);
                         }),
              "Each original skin retains exact IBM float bits; unweighted "
              "closure is explicit identity");
      }
    }
    for (size_t c = 0; c < count; ++c) {
      auto v = primitive.indices
                   ? cgltf_accessor_read_index(primitive.indices, c)
                   : c;
      float components[3];
      check(cgltf_accessor_read_float(position, v, components, 3),
            "Independent current POSITION accessor decodes");
      for (size_t t = 0; t < primitive.targets_count; ++t) {
        float weight = node.weights_count         ? node.weights[t]
                       : node.mesh->weights_count ? node.mesh->weights[t]
                                                  : 0;
        auto delta = attribute(primitive.targets[t].attributes,
                               primitive.targets[t].attributes_count,
                               cgltf_attribute_type_position);
        if (delta) {
          float d[3];
          check(cgltf_accessor_read_float(delta, v, d, 3),
                "Independent POSITION target accessor decodes");
          for (size_t k = 0; k < 3; ++k)
            components[k] += weight * d[k];
        }
      }
      Vec3 p{components[0], components[1], components[2]}, actual{};
      if (node.skin)
        for (int set = 0;; ++set) {
          auto joints =
              attribute(primitive.attributes, primitive.attributes_count,
                        cgltf_attribute_type_joints, set);
          if (!joints)
            break;
          auto weights =
              attribute(primitive.attributes, primitive.attributes_count,
                        cgltf_attribute_type_weights, set);
          float js[4], ws[4];
          check(weights && cgltf_accessor_read_float(joints, v, js, 4) &&
                    cgltf_accessor_read_float(weights, v, ws, 4),
                "Independent original joint slots and real nonzero influences "
                "decode");
          for (size_t k = 0; k < 4; ++k)
            if (ws[k] != 0)
              actual += point(skin.at(size_t(js[k])), p) * ws[k];
        }
      else
        actual = point(node_world, p);
      auto native = expected.positions[expected.indices[c]];
      auto wanted = world.point({native.x, native.y, native.z});
      error = std::max(error,
                       length(actual - Vec3{float(wanted[0]), float(wanted[1]),
                                            float(wanted[2])}));
      ++corners;
    }
    ++parts;
  }
  check(parts == surfaces.size() && error < 2e-6f,
        "Independent posed skin/rigid world corners match actual native "
        "geometry within two micrometres");
  std::cout << "VRM1 independent corners=" << corners << " maxerror=" << error
            << '\n';
  auto emitted = json(output);
  check(emitted["extensions"]["VRMC_vrm"]["specVersion"] == "1.0" &&
            !emitted["extensions"].contains("VRM"),
        "Output preserves actual VRM1 version without legacy relabel");
}
void spatial_bone_accessory(const std::filesystem::path &out, const Scene &original,
                            const std::filesystem::path &source, uint64_t accessory) {
  using namespace dome;
  check(spatial_action(avatar_attachment_page, 2) == SpatialCommand::AttachPreviousBone &&
        spatial_action(avatar_attachment_page, 8) == SpatialCommand::AttachBoneCancel &&
        spatial_action(avatar_attachment_page, 10) == SpatialCommand::AttachBoneApply &&
        !spatial_action(spatial_page_count, 0), "Attachment owns distinct commands and keeps the radial page boundary");
  ImportAuthoringOptions options;
  options.paint_resolution = 64;
  auto second = prepare_authoring_import(original, source, options);
  auto &scene = second.scene;
  const auto rig = second.objects.front();
  const auto identity = detail::source_rig_instance(scene, scene.object(rig));
  if (!identity) throw std::runtime_error("Missing second imported rig identity");
  scene.group(identity->source_instance).name = "Second imported avatar";
  scene.group(identity->source_instance).transform.position.x = .22f;
  scene.select(accessory);

  Document doc;
  doc.replace(scene);
  doc.begin_spatial();
  doc.save_async(out / "xr-before-attach.dsculpt").get();
  const auto before = scene.encode();
  SpatialFrame frame;
  frame.focused = frame.head_valid = true;
  frame.head.position = {0, 1.6f, 0};
  for (size_t i = 0; i < 2; ++i) {
    auto &hand = frame.hands[i];
    hand.tracked.valid = hand.tracked.aimValid = true;
    hand.tracked.grip.position = hand.tracked.aim.position = {i ? 0.f : -.3f, 1.48f, -.52f};
  }
  auto submit = [&] {
    ++frame.sequence;
    frame.time += .02;
    doc.submit(frame);
    if (!doc.wait_input()) throw std::runtime_error("Controller attachment input did not drain");
  };
  auto click = [&](int sector, float trigger = 0) {
    auto &hand = frame.hands[1];
    const auto angle = float(sector) * 3.14159265358979323846f / 6;
    hand.x = sector < 0 ? 0 : std::cos(angle);
    hand.y = sector < 0 ? 0 : std::sin(angle);
    hand.joystick = false;
    hand.tracked.trigger = 0;
    submit();
    hand.joystick = true;
    hand.tracked.trigger = trigger;
    submit();
    hand.joystick = false;
    hand.x = hand.y = 0;
    submit();
  };
  auto open = [&] {
    click(-1);
    click(8);
    click(10);
    click(5);
    if (doc.frame().state->menu_page != 11 || !doc.frame().state->menu_enabled[0])
      throw std::runtime_error("The VR Rig page did not expose attachment for the selected accessory");
    click(0);
    const auto state = doc.frame().state;
    if (!state->avatar_attachment || state->menu_page != avatar_attachment_page || state->menu_index != 8 ||
        state->avatar_attachment->owner != 2 || state->avatar_attachment->rigs->size() != 2)
      throw std::runtime_error("The controller did not open its owned two-avatar picker");
  };
  submit();
  open();
  click(1);
  click(3);
  auto state = doc.frame().state;
  check(state->avatar_attachment && state->avatar_attachment->avatar == 1 && state->avatar_attachment->bone == 1 &&
        state->avatar_attachment->rigs->at(1).object == rig && doc.copy_scene().encode() == before,
        "Controller cycles exact avatar instances and joints without editing the scene");
  frame.hands[0].undo = true;
  frame.hands[0].joystick = true;
  submit();
  frame.hands[0].joystick = false;
  check(doc.frame().state->avatar_attachment->bone == 1 && doc.copy_scene().encode() == before,
        "The other controller cannot change the picker or trigger document history");
  click(8, .9f);
  check(!doc.frame().state->avatar_attachment && !doc.frame().state->menu &&
        !doc.frame().state->native_pending && doc.copy_scene().encode() == before,
        "Explicit Cancel and a carried trigger leave attachment and paint unchanged");
  frame.hands[0].undo = false;
  frame.hands[1].tracked.trigger = 0;

  open();
  frame.hands[1].tracked.valid = false;
  submit();
  check(!doc.frame().state->avatar_attachment && doc.copy_scene().encode() == before,
        "Loss of the owning controller retires its uncommitted choice");
  frame.hands[1].tracked.valid = true;
  open();
  frame.focused = false;
  submit();
  check(!doc.frame().state->avatar_attachment && doc.copy_scene().encode() == before,
        "OpenXR focus loss cancels the picker without queuing attachment work");
  frame.focused = true;
  open();
  doc.select_object(rig);
  check(!doc.frame().state->avatar_attachment && !doc.frame().state->menu,
        "A different selected object retires stale attachment authority");
  doc.select_object(accessory);

  open();
  click(1);
  click(3);
  click(10, .9f);
  const auto saved = out / "xr-attached.dsculpt";
  doc.save_async(saved).get();
  auto expected = scene.snapshot();
  detail::attach_source_accessory(expected, accessory, rig, 1, {});
  check(doc.wait_asset() && !doc.frame().state->avatar_attachment && doc.frame().state->error.empty() &&
        !doc.frame().state->native_pending && doc.copy_scene().encode() == expected.encode() &&
        Scene::load(saved).encode() == expected.encode(),
        "Controller Apply and ordered Save match the shared worker without a carried paint press");
  const auto target = out / "xr-attachment-editable.vrm";
  doc.export_asset(target, AssetExportFormat::EditableAvatar512).get();
  auto ids = second.objects;
  ids.push_back(accessory);
  const auto native = extract_native_authoring(bytes(target));
  check(native && native->encode() == detail::emission_export_subset(expected, ids).encode(),
        "Export from the newly attached selection includes the chosen avatar instance only");
  check(doc.undo() && doc.copy_scene().encode() == before && doc.redo() &&
        doc.copy_scene().encode() == expected.encode(), "Controller attachment publishes one complete Undo and Redo");
  doc.finish_spatial();
}

void shape_texture_accessory(const std::filesystem::path &out, dome::Scene scene,
                             uint64_t accessory, uint64_t rig, uint32_t head) {
  using namespace dome;
  check(edit_texture_set(scene, accessory, 1, [](PaintedMesh &paint, Material &) {
    BakedTextures baked;
    const auto size = paint.resolution();
    const auto map = [size](std::array<uint8_t, 4> color) {
      Image image{size, size, std::vector<uint8_t>(size_t(size) * size * 4)};
      for (size_t p = 0; p < image.rgba.size(); p += 4)
        std::copy(color.begin(), color.end(), image.rgba.begin() + p);
      return image;
    };
    baked.maps.color = map({64, 96, 128, 255});
    baked.maps.metal_rough = map({255, 128, 0, 255});
    baked.maps.emission = map({0, 0, 0, 255});
    baked.maps.normal = map({149, 132, 252, 255});
    baked.coverage.assign(size_t(size) * size, 255);
    paint.install_bake(std::move(baked));
    Brush brush;
    brush.from = brush.to = {-.045f, .04f, 0};
    brush.normal = {0, 0, 1};
    brush.radius = .05f;
    PaintSettings ink;
    ink.color = {.8f, .4f, .2f, 1};
    ink.front_only = false;
    paint.begin_stroke();
    paint.stamp(brush, ink);
    return paint.end_stroke();
  }), "The secondary shape field has its own normal Bake and writable paint");
  Scene initial = scene.snapshot();
  auto paint_exact = [&](const Scene &after) {
    for (TextureSetId set : {0u, 1u}) {
      const auto before = texture_set_view(initial.object(accessory), set);
      const auto current = texture_set_view(after.object(accessory), set);
      if (before.domain != current.domain || before.painted->mesh_owner() != current.painted->mesh_owner() ||
          before.painted->baked() != current.painted->baked() ||
          before.painted->active_layer() != current.painted->active_layer())
        return false;
      const auto same_layer = [](const PaintLayer &a, const PaintLayer &b) {
        return a.id == b.id && a.name == b.name && a.visible == b.visible && a.opacity == b.opacity &&
               a.tiles == b.tiles && a.pbr_tiles == b.pbr_tiles && a.emission_tiles == b.emission_tiles;
      };
      if (!std::ranges::equal(before.painted->layers(), current.painted->layers(), same_layer))
        return false;
    }
    return true;
  };

  Document doc;
  auto settle = [&] {
    if (!doc.wait_native_stroke() || !doc.wait_asset() || !doc.wait_mesh() || !doc.frame().state->error.empty())
      throw std::runtime_error("Texture-set shapekey did not settle: " + doc.frame().state->error);
  };
  doc.replace(scene);
  settle();
  doc.save_async(out / "shape-before.dsculpt").get();
  doc.author_shape({}, doc.draft_scene().source);
  doc.save_async(out / "shape-created.dsculpt").get();
  settle();
  const auto before = doc.copy_scene().encode();
  auto old_archive = before;
  for (auto &object : old_archive.at("objects")) object.erase("rig_source");
  check(before.at("polygon_sources").at("version") == 8 && Scene::decode(old_archive).encode() == before,
        "Legacy texture-only history without nullable live rig fields reopens and normalizes on Save");
  old_archive.at("objects").at(0)["rig_source"] = nullptr;
  bool incomplete = false;
  try { (void)Scene::decode(old_archive); } catch (const std::invalid_argument &) { incomplete = true; }
  check(incomplete, "Partial rig-field omission is refused in the Scene40 archive");
  doc.configure_shape_sculpt(true);
  settle();
  const auto world = scene.world_matrix(accessory);
  const auto point = world.point({-.09, .09, .75});
  NativeStrokeSample ray;
  ray.origin = {float(point[0]), float(point[1]), float(point[2])};
  ray.direction = {0, 0, -1};
  ray.brush.radius = .06f;
  check(doc.begin_native_stroke() && doc.sample_native_stroke(ray),
        "Desktop shapekey sculpt admits a ray on the inactive secondary paint field");
  const auto preview_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (doc.frame().state->native_samples == 0 && doc.frame().state->error.empty() &&
         std::chrono::steady_clock::now() < preview_deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    if (!doc.sample_native_stroke(ray)) break;
  }
  check(doc.frame().state->native_samples > 0 && doc.wait_mesh() &&
        doc.frame().surface->native_samples > 0 && doc.copy_scene().encode() == before,
        "Live partitioned preview rebuilds the material-aware query without publishing the draft into history");
  doc.end_native_stroke();
  doc.save_async(out / "shape-sculpted.dsculpt").get();
  settle();
  const auto accepted = doc.copy_scene();
  const auto &delta = accepted.object(accessory).deformation->morphs->targets.at(0).delta;
  check(delta.at(3).z > .002f && delta.at(0).z == 0 && paint_exact(accepted) &&
        accepted.object(accessory).deformation->morph_weights.empty(),
        "A ray on the inactive material shapes its actual corner with exact native paint and saved weights");
  check(Scene::load(out / "shape-sculpted.dsculpt").encode() == accepted.encode() &&
        doc.undo() && doc.copy_scene().encode() == before && doc.redo() && doc.copy_scene().encode() == accepted.encode(),
        "Ordered Save, reopen, Undo and Redo preserve the whole partitioned sculpt transaction");
  settle();
  check(doc.begin_native_stroke() && doc.sample_native_stroke(ray), "The accepted texture-set shape can be edited again");
  doc.end_native_stroke(false);
  settle();
  check(doc.copy_scene().encode() == accepted.encode(), "Cancelled sculpt preserves the exact accessory project");

  doc.configure(Tool::Clay, .07f);
  SpatialFrame input;
  input.focused = input.head_valid = true;
  input.head.position = {0, 1.6f, 0};
  auto &hand = input.hands[1].tracked;
  hand.valid = hand.aimValid = true;
  const auto submit = [&](float pressure) {
    const auto canvas = doc.frame().state->canvas;
    hand.grip.position = hand.aim.position = ob::transform(canvas, {ray.origin.x, ray.origin.y, ray.origin.z});
    hand.grip.orientation = hand.aim.orientation = canvas.orientation;
    hand.trigger = pressure;
    ++input.sequence;
    input.time += .05;
    doc.submit(input);
    if (!doc.wait_input()) throw std::runtime_error("Texture-set shape controller did not settle");
  };
  submit(0);
  submit(.85f);
  submit(0);
  settle();
  const auto controller = doc.copy_scene();
  check(controller.object(accessory).deformation->morphs->targets[0].delta[3].z > delta[3].z && paint_exact(controller),
        "Controller pressure sculpts the same mesh across paint partitions");
  submit(.8f);
  input.focused = false;
  submit(.8f);
  settle();
  check(doc.copy_scene().encode() == controller.encode(), "XR focus loss rolls back the entire active shape gesture");
  doc.configure_shape_sculpt(false);
  settle();

  auto current = controller.snapshot();
  RigEdit pose;
  pose.joint = JointAdjustment{head, {0, 4, 8}, {}};
  edit_rig(current, rig, pose);
  RigEdit slider;
  slider.morph = MorphWeightEdit{0, .75f};
  edit_rig(current, accessory, slider);
  const auto &object = current.object(accessory);
  const auto &rest = object.painted->mesh();
  const auto &morphs = *object.deformation->morphs;
  const auto shaped = morphs.evaluate(rest, std::array{1.f});
  const auto tangents = tangent_corners(shaped);
  const auto rest_tangents = tangent_corners(rest);
  check(length(shaped.normals[3] - rest.normals[3]) > .001f &&
        std::abs(tangents[5][2] - rest_tangents[5][2]) > .001f,
        "The sculpt fixture exercises changed native normals and baked-normal tangents");
  check(current.world_matrix(accessory).matrix() != world.matrix() && paint_exact(current),
        "The reshaped accessory still follows the original joint with exact paint and Bake");
  auto mirrored = current.snapshot();
  ShapeKeyEdit mirror;
  mirror.action = ShapeKeyAction::Mirror;
  mirror.mirror_frames = ShapeKeyMirrorFrames::VertexPositions;
  const auto reflected = author_shapekey(mirrored, accessory, mirror);
  check(reflected.changed && reflected.target == 1 && paint_exact(mirrored) &&
        mirrored.object(accessory).deformation->morphs->targets[1].delta[2].z == morphs.targets[0].delta[3].z,
        "Mirroring the sculpted target preserves both paint fields and creates its real geometric counterpart");
  doc.replace(current);
  settle();
  doc.save_async(out / "shape-ready-to-export.dsculpt").get();
  for (const auto format : {AssetExportFormat::Avatar, AssetExportFormat::EditableAvatar512}) {
    const bool editable = format == AssetExportFormat::EditableAvatar512;
    const auto path = out / (editable ? "shape-accessory-editable.vrm" : "shape-accessory.vrm");
    doc.export_asset(path, format).get();
    const auto payload = json(path);
    const auto bone = payload.at("extensions").at("VRMC_vrm").at("humanoid").at("humanBones").at("head").at("node").get<size_t>();
    cgltf_options options{};
    cgltf_data *raw = nullptr;
    check(cgltf_parse_file(&options, path.c_str(), &raw) == cgltf_result_success,
          "The independent reader opens the authored morph in exported VRM1");
    const auto reader = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>(raw, cgltf_free);
    check(cgltf_load_buffers(&options, raw, path.c_str()) == cgltf_result_success && cgltf_validate(raw) == cgltf_result_success,
          "The authored VRM morph frames and material resources pass independent glTF validation");
    const auto *leaf = std::find_if(raw->nodes, raw->nodes + raw->nodes_count, [](const auto &node) {
      return node.mesh && node.name && std::string_view(node.name) == "Native head light";
    });
    bool attached = false;
    if (leaf != raw->nodes + raw->nodes_count)
      for (auto parent = leaf->parent; parent; parent = parent->parent) attached |= parent == raw->nodes + bone;
    check(attached && !leaf->skin &&
          leaf->mesh->primitives_count == 2 && leaf->mesh->weights_count == 1 && leaf->mesh->weights[0] == .75f,
          "One unskinned shape mesh with both fields and the saved weight remains attached to the real bone");
    auto attribute = [](const cgltf_morph_target &target, cgltf_attribute_type type) {
      const auto it = std::find_if(target.attributes, target.attributes + target.attributes_count,
                                   [=](const auto &value) { return value.type == type; });
      if (it == target.attributes + target.attributes_count) throw std::runtime_error("Missing authored morph attribute");
      return it->data;
    };
    auto vector = [](const cgltf_accessor *accessor, size_t vertex) -> Vec3 {
      std::array<float, 4> values{};
      if (!accessor || !cgltf_accessor_read_float(accessor, vertex, values.data(), values.size()))
        throw std::runtime_error("Unable to independently read morph vertex accessor");
      return {values[0], values[1], values[2]};
    };
    for (size_t part = 0; part < 2; ++part) {
      const auto &primitive = leaf->mesh->primitives[part];
      if (primitive.indices->count != 3 || primitive.targets_count != 1) throw std::runtime_error("Wrong shape material partition");
      const auto &target = primitive.targets[0];
      for (size_t c = 0; c < 3; ++c) {
        const auto corner = part * 3 + c;
        const auto original = rest.indices[corner];
        const auto vertex = cgltf_accessor_read_index(primitive.indices, c);
        const auto position = vector(cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0), vertex) +
            vector(attribute(target, cgltf_attribute_type_position), vertex);
        const auto normal = vector(cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0), vertex) +
            vector(attribute(target, cgltf_attribute_type_normal), vertex);
        check(length(position - shaped.positions[original]) < 1e-6f && length(normal - shaped.normals[original]) < 1e-6f,
              "Portable position and derived normal match the exact native weight-one corner");
        if (part == 1) {
          const auto tangent = vector(cgltf_find_accessor(&primitive, cgltf_attribute_type_tangent, 0), vertex) +
              vector(attribute(target, cgltf_attribute_type_tangent), vertex);
          const auto &value = tangents[corner];
          check(length(tangent - Vec3{value[0], value[1], value[2]}) < 1e-6f,
                "Normal-mapped corner preserves the native morph tangent in the actual exported buffer");
        }
      }
    }
    const auto restored = editable ? extract_native_authoring(bytes(path)) : extract_native_emission(bytes(path));
    check(restored && restored->encode() == current.encode(), "The portable result restores the exact attached native shape, field layers and original avatar");
  }
}

void surface_heat_texture_accessory(const std::filesystem::path &out, Scene scene, uint64_t accessory, uint64_t rig) {
  for (TextureSetId set : {0u, 1u}) {
    check(edit_texture_set(scene, accessory, set, [set](PaintedMesh &paint, Material &) {
      paint.add_layer(set ? "Secondary finish" : "Primary finish");
      const auto &mesh = paint.mesh();
      const auto first = set * 3;
      Brush brush;
      brush.from = brush.to = (mesh.positions[mesh.indices[first]] +
          mesh.positions[mesh.indices[first + 1]] + mesh.positions[mesh.indices[first + 2]]) / 3.f;
      brush.normal = {0, 0, 1};
      brush.radius = .035f;
      brush.strength = .8f;
      PaintSettings ink;
      ink.color = set ? std::array{.21f, .6f, .9f, .8f} : std::array{.8f, .22f, .36f, .75f};
      paint.begin_stroke();
      paint.stamp(brush, ink);
      return paint.end_stroke();
    }), "Each material partition receives an actual editable paint layer");
  }
  const auto donor = scene.add("Hidden Surface Heat bake donor", Volume{});
  auto &source = scene.object(donor);
  source.visible = false;
  source.locked = true;
  source.mesh_active = true;
  source.transform = scene.object(accessory).transform;
  source.material.emissive = {3.f, .7f, .25f};
  auto donor_mesh = scene.object(accessory).painted->mesh();
  for (auto &p : donor_mesh.positions) p.z += .007f;
  for (auto &n : donor_mesh.normals) n = normalized(Vec3{.15f, .2f, 1.f});
  source.painted = std::make_shared<PaintedMesh>(std::move(donor_mesh), 64);
  scene.select(accessory);
  select_texture_set(scene, accessory, 1);

  Document doc;
  auto settled = [&] {
    if (!doc.wait_asset() || !doc.wait_mesh() || !doc.frame().state->error.empty())
      throw std::runtime_error("Surface Heat workflow did not settle: " + doc.frame().state->error);
  };
  doc.replace(scene);
  settled();
  detail::NativeSurfaceHeatOptions bounds;
  const auto input_budget = bounds.maximum_total - bounds.maximum_workspace;
  const auto unbaked = detail::native_weight_retention(scene, input_budget);
  ObjectBakeSettings bake;
  bake.ao_samples = 2;
  bake.allow_backfaces = true;
  bake.corresponding_topology = true;
  doc.bake_from_object(donor, bake);
  settled();
  scene = doc.copy_scene();
  select_texture_set(scene, accessory, 0);
  doc.replace(scene);
  settled();
  const auto before = scene.encode();
  const auto collection = scene.object(accessory).texture_sets;
  const auto field = texture_set_view(scene.object(accessory), 1);
  check(field.bake_recipe && field.painted->has_paint_data() && field.painted->baked() &&
        field.painted->baked()->emission_field && !field.painted->baked()->maps.normal.rgba.empty(),
        "Surface Heat input has a real non-primary layered normal and physical emission Bake");
  doc.save_async(out / "heat-before.dsculpt").get();

  const std::array selected{accessory};
  const auto retained = detail::native_weight_retention(scene, input_budget);
  check(retained.bytes >= unbaked.bytes + field.painted->baked()->emission_field->storage_bytes() +
        field.painted->baked()->maps.normal.rgba.size(),
        "Native owning admission includes the secondary FLOAT field");
  auto limited = bounds;
  limited.maximum_total = limited.maximum_workspace + retained.bytes - 1;
  bool refused = false;
  try { detail::capture_native_surface_heat(scene, selected, rig, limited); }
  catch (const std::length_error &) { refused = true; }
  check(refused && scene.encode() == before, "One byte below the retained budget refuses before staging");

  const auto request = detail::capture_native_surface_heat(scene, selected, rig, bounds);
  auto stale = scene.snapshot();
  edit_texture_set(stale, accessory, 1, [](PaintedMesh &paint, Material &) {
    paint.layer_properties("Secondary finish", true, .5f);
    return true;
  });
  check(!request.matches(stale), "The captured lease detects edits to an inactive paint owner");
  auto prepared = detail::prepare_native_surface_heat(request);
  const auto stale_before = stale.encode();
  refused = false;
  try { detail::adopt_native_surface_heat(stale, request, std::move(prepared)); }
  catch (const std::runtime_error &) { refused = true; }
  check(refused && stale.encode() == stale_before, "A stale worker cannot overwrite a newer texture-set edit");
  auto cancelled = scene;
  refused = false;
  try { detail::adopt_native_surface_heat(cancelled, request, std::move(prepared), [] { return true; }); }
  catch (const std::runtime_error &) { refused = true; }
  check(refused && cancelled.encode() == before, "Cancellation during linked bind publication preserves the whole project");

  SkinAuthoringOptions options;
  options.reference = rig;
  options.mode = SkinAuthoringMode::SurfaceHeat;
  doc.bind_skin(options, doc.draft_scene().source);
  auto saved = out / "heat-bound.dsculpt";
  doc.save_async(saved).get();
  settled();
  const auto bound = doc.copy_scene();
  const auto &current = bound.object(accessory);
  const auto &native = *current.deformation;
  const auto other = texture_set_view(current, 1);
  check(native.skin && native.source_skin_joints == scene.object(rig).appearance->source_joint_nodes &&
        native.morphs->encode() == scene.object(accessory).deformation->morphs->encode() &&
        current.texture_sets == collection && other.painted == field.painted &&
        other.painted->baked() == field.painted->baked() && other.bake_recipe == field.bake_recipe,
        "Surface Heat publishes one skin link while preserving the complete paint and morph owners");
  check(Scene::load(saved).encode() == bound.encode() && doc.undo() && doc.copy_scene().encode() == before &&
        doc.redo() && doc.copy_scene().encode() == bound.encode(),
        "Ordered Save and UndoRedo preserve both texture-set and surface-weight authority");
  settled();
  const auto portable = out / "heat-bound-editable.vrm";
  doc.export_asset(portable, AssetExportFormat::EditableAvatar512).get();
  auto replay = extract_native_authoring(bytes(portable));
  check(replay && replay->active().id == accessory &&
        replay->active().deformation->encode(replay->active().painted->mesh()) == native.encode(current.painted->mesh()) &&
        texture_set_view(replay->active(), 1).painted->baked()->emission_field->encode() ==
          field.painted->baked()->emission_field->encode(),
        "Editable VRM retains the solved weights and physical emission Bake");

  const auto &first_row = native.skin->vertices.front();
  uint32_t unused_joint = 0;
  while (std::ranges::any_of(first_row,[&](const auto &influence){return influence.joint == unused_joint;}))
    ++unused_joint;
  WeightToolSettings weight_tool;
  weight_tool.enabled = true;
  weight_tool.linked = false;
  doc.weight_settings(weight_tool, unused_joint);
  settled();
  check(doc.begin_desktop(), "The baked and Surface Heat-bound accessory admits an isolated weight brush");
  const auto vertex = native.preview(current.painted->mesh()).positions.front();
  const auto p = bound.world_matrix(accessory).point({vertex.x,vertex.y,vertex.z});
  Brush stamp;
  stamp.from = stamp.to = {float(p[0]),float(p[1]),float(p[2])};
  stamp.radius = .04f;
  doc.stamp_desktop(stamp);
  doc.end_desktop();
  settled();
  const auto edited = doc.copy_scene();
  const auto edited_field = texture_set_view(edited.object(accessory), 1);
  check(edited.object(accessory).deformation->skin->vertices.front() != first_row &&
        edited_field.painted == field.painted && edited_field.bake_recipe == field.bake_recipe &&
        edited.object(rig).deformation == bound.object(rig).deformation,
        "A baked multi-material accessory retains its editable layers and exact Bake while its isolated weights change");
  const auto weighted_portable = out / "heat-weight-painted-editable.vrm";
  doc.export_asset(weighted_portable, AssetExportFormat::EditableAvatar512).get();
  auto weighted_replay = extract_native_authoring(bytes(weighted_portable));
  check(weighted_replay && weighted_replay->active().deformation->skin->vertices ==
          edited.object(accessory).deformation->skin->vertices &&
        texture_set_view(weighted_replay->active(), 1).painted->baked()->emission_field->encode() ==
          field.painted->baked()->emission_field->encode(),
        "Editable VRM replays the painted weights and baked FLOAT emission together");
}

void skinned_texture_accessory(const std::filesystem::path &out, Scene initial) {
  const auto accessory = initial.active().id;
  initial.active().name = "Native skinned collar";
  initial.active().transform.position = {.48f, .10f, -.045f};
  initial.active().transform.scale = .86f;
  ShapeKeyEdit key;
  key.name = "Flare";
  author_shapekey(initial, accessory, key);
  key.action = ShapeKeyAction::Replace;
  key.positions = initial.active().painted->mesh().positions;
  key.positions[2].z += .04f;
  author_shapekey(initial, accessory, key);
  const auto paint = initial.active().texture_sets;

  auto fixture = metadata_source();
  auto &used = fixture.j["extensionsUsed"].get_ref<J::array_t &>();
  for (const auto *extension : {"VRMC_springBone", "VRMC_node_constraint"}) std::erase(used, J(extension));
  fixture.j["extensions"].erase("VRMC_springBone");
  for (auto &node : fixture.j["nodes"]) node.erase("extensions");
  const auto &roles = fixture.j["extensions"]["VRMC_vrm"]["humanoid"]["humanBones"];
  const auto hips = roles.at("hips").at("node");
  auto &primitive = fixture.j["meshes"][0]["primitives"][0]["attributes"];
  const auto joint_view = fixture.j["accessors"][primitive.at("JOINTS_0").get<size_t>()]["bufferView"].get<size_t>();
  const auto offset = fixture.j["bufferViews"][joint_view]["byteOffset"].get<size_t>();
  for (size_t v = 0; v < 4; ++v) fixture.bin[offset + v * 8 + 2] = 1;
  primitive["WEIGHTS_0"] = fixture.values({.75f,.25f,0,0, .60f,.40f,0,0, .35f,.65f,0,0, .8f,.2f,0,0}, 4, "VEC4");
  for (auto &skin : fixture.j["skins"])
    skin["joints"][1] = hips;
  const auto source_path = out / "skinned-source.vrm";
  fixture.save(source_path);
  ImportAuthoringOptions options;
  options.paint_resolution = 64;
  const auto imported = prepare_authoring_import(initial, source_path, options);
  auto scene = imported.scene.snapshot();
  scene.select(accessory);
  const auto rig = imported.objects.front();
  const auto head = scene.object(rig).deformation->humanoid->joints.at("head");
  const auto original_world = scene.world_matrix(accessory);
  const auto before = scene.encode();

  Document doc;
  auto settle = [&] {
    if (!doc.wait_asset() || !doc.wait_mesh() || !doc.frame().state->error.empty())
      throw std::runtime_error("Native skin link did not settle: " + doc.frame().state->error);
  };
  doc.replace(scene);
  settle();
  doc.save_async(out / "skin-before.dsculpt").get();
  SkinAuthoringOptions bind;
  bind.mode = SkinAuthoringMode::Transfer;
  bind.reference = rig;
  doc.bind_skin(bind, doc.draft_scene().source);
  const auto saved = out / "skin-bound.dsculpt";
  doc.save_async(saved).get();
  settle();
  const auto bound = doc.copy_scene();
  const auto &collar = bound.object(accessory);
  const auto &deformation = *collar.deformation;
  const auto link = detail::source_skin_link(bound.group_path(collar.group), deformation);
  const auto &reference = bound.object(rig);
  check(link && link == detail::source_rig_instance(bound, reference) &&
        deformation.source_skin_joints == reference.appearance->source_joint_nodes &&
        std::ranges::all_of(deformation.skin->vertices, [](const auto &row) { return row.size() == 2; }),
        "Weight transfer creates a durable link to the actual instance and retains both genuine joint influences");
  double placement_error = 0;
  for (size_t i = 0; i < 16; ++i)
    placement_error = std::max(placement_error, std::abs(original_world.matrix()[i] - bound.world_matrix(accessory).matrix()[i]));
  check(placement_error < 1e-9 && collar.texture_sets == paint &&
        deformation.morphs->encode() == scene.object(accessory).deformation->morphs->encode(),
        "Linking preserves world placement, native material ownership and existing shapekeys");
  check(Scene::load(saved).encode() == bound.encode() && doc.undo() && doc.copy_scene().encode() == before &&
        doc.redo() && doc.copy_scene().encode() == bound.encode(),
        "The linked bind is one undoable asset operation and the ordered Save restores its exact source authority");
  auto appended = append_native_scene(bound, bound);
  const auto copied = appended.scene.active().id;
  const auto &appended_skin = *appended.scene.active().deformation;
  check(appended_skin.source_skin_instance != deformation.source_skin_instance &&
        appended_skin.source_skin_joints == deformation.source_skin_joints &&
        Scene::decode(appended.scene.encode()).encode() == appended.scene.encode(),
        "Appending the native package namespaces the imported avatar instance and retains joint and paint authority");
  appended.scene.object(copied).group = collar.group;
  bool cross_instance = false;
  try { appended.scene.validate(false); } catch (const std::invalid_argument &) { cross_instance = true; }
  check(cross_instance, "Moving a linked skin to another instance of the same file cannot transfer its rig authority");
  auto restored_accessory = bound.snapshot();
  restore_rig_source(restored_accessory, accessory, false);
  check(restored_accessory.object(accessory).deformation->encode(restored_accessory.object(accessory).painted->mesh()) ==
        scene.object(accessory).deformation->encode(scene.object(accessory).painted->mesh()) &&
        restored_accessory.object(accessory).texture_sets == paint,
        "Restoring the accessory's original source removes its skin link and keeps its native shape and paint");
  auto automatic = scene.snapshot();
  auto automatic_options = bind;
  automatic_options.mode = SkinAuthoringMode::Automatic;
  automatic_options.seed_distance = 6;
  check(author_skin(automatic, accessory, automatic_options) &&
        automatic.object(accessory).deformation->source_skin_joints == deformation.source_skin_joints &&
        automatic.object(accessory).texture_sets == paint,
        "The automatic bind route adopts the same original joint identities and native paint");
  settle();
  auto linked_ids = rig_rest_linked_objects(bound, rig);
  check(std::ranges::find(linked_ids, accessory) != linked_ids.end() && linked_ids == rig_rest_linked_objects(bound, accessory),
        "Original-rig fitting captures the same exact recipients from both the avatar and its linked accessory");
  RigEdit edit;
  edit.joint = JointAdjustment{head, {0, 22, 4}, {.02f, -.01f, .04f}};
  edit.morph = MorphWeightEdit{0, .6f};
  doc.edit_scene("Pose the linked garment", [accessory, edit](Scene &value, const auto &cancelled) {
    edit_rig(value, accessory, edit, cancelled);
  }, doc.draft_scene().source);
  settle();
  const auto posed = doc.copy_scene();
  const auto &current = *posed.object(accessory).deformation;
  check(current.skeleton->local_matrices(current.pose) ==
        posed.object(rig).deformation->skeleton->local_matrices(posed.object(rig).deformation->pose) &&
        current.morph_weights == std::vector{.6f},
        "Pose from the linked accessory updates the original avatar and keeps morph edits on the selected mesh");
  auto locked = bound.snapshot();
  locked.object(accessory).locked = true;
  const auto locked_before = locked.encode();
  auto joint = edit;
  joint.morph.reset();
  bool refused = false;
  try { edit_rig(locked, rig, joint); } catch (const std::invalid_argument &) { refused = true; }
  check(refused && locked.encode() == locked_before, "A locked native skin refuses the complete linked pose transaction");
  joint.linked_pose = false;
  auto unlinked = bound.snapshot();
  refused = false;
  try { edit_rig(unlinked, rig, joint); } catch (const std::invalid_argument &) { refused = true; }
  check(refused && unlinked.encode() == bound.encode(), "Per-part pose edits cannot leave an attached native skin behind");

  auto fitted = posed.snapshot();
  const auto &current_mesh = posed.object(accessory).painted->mesh();
  const auto expected = current.preview(current_mesh);
  const auto head_world = fitted.world_matrix(rig) * AffineTransform(fitted.object(rig).deformation->skeleton->world().at(head));
  const auto pivot = head_world.point({0, 0, 0});
  RigRestJointEdit rest;
  rest.joint = head;
  rest.world_position = Vec3{float(pivot[0] + .008), float(pivot[1] - .003), float(pivot[2])};
  check(author_rig_rest(fitted, accessory, {&rest, 1}) && fitted.object(accessory).texture_sets == paint,
        "A current rest edit from the linked accessory fits the original avatar and retains native paint");
  const auto fitted_preview = fitted.object(accessory).deformation->preview(current_mesh);
  bool matches = expected.positions.size() == fitted_preview.positions.size();
  for (size_t i = 0; i < expected.positions.size(); ++i)
    matches &= length(expected.positions[i] - fitted_preview.positions[i]) < 2e-5f;
  check(matches, "Original rest fitting transports the accessory bind without moving its evaluated surface");
  auto restored_avatar = fitted.snapshot();
  restore_rig_source(restored_avatar, rig, true);
  const auto &restored_skin = *restored_avatar.object(accessory).deformation;
  check(restored_skin.source_skin_joints == deformation.source_skin_joints &&
        restored_skin.skeleton->encode() == deformation.skeleton->encode() &&
        restored_skin.morph_weights == current.morph_weights,
        "Restoring the avatar's first rig transports the linked skin and preserves its authored morph weight");

  const auto posed_file = out / "skin-ready-to-export.dsculpt";
  doc.save_async(posed_file).get();
  auto verify = [&](const Scene &state, const std::filesystem::path &path) {
    const auto &native = *state.object(accessory).deformation;
    const auto local = native.preview(current_mesh);
    const auto model = state.world_matrix(accessory);
    cgltf_options opts{};
    cgltf_data *raw = nullptr;
    check(cgltf_parse_file(&opts, path.c_str(), &raw) == cgltf_result_success, "Independent cgltf opens the exported skinned accessory");
    const auto reader = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>(raw, cgltf_free);
    check(cgltf_load_buffers(&opts, raw, path.c_str()) == cgltf_result_success && cgltf_validate(raw) == cgltf_result_success,
          "Independent cgltf validates the shared rig and material resources");
    const auto *leaf = std::find_if(raw->nodes, raw->nodes + raw->nodes_count, [](const auto &node) {
      return node.mesh && node.name && std::string_view(node.name) == "Native skinned collar surface";
    });
    const auto payload = json(path);
    const auto &avatar = payload.at("extensions").at("VRMC_vrm");
    const auto shared_head = avatar.at("humanoid").at("humanBones").at("head").at("node").get<size_t>();
    check(leaf != raw->nodes + raw->nodes_count && leaf->skin && leaf->mesh->primitives_count == 2 &&
          leaf->skin->joints[head] == raw->nodes + shared_head && leaf->mesh->weights_count == 1 &&
          leaf->mesh->weights[0] == .6f, "Both primitives share one real skin and the original humanoid head node");
    const auto &expressions = avatar.at("expressions").at("preset").at("happy");
    check(std::ranges::none_of(expressions.at("morphTargetBinds"), [&](const auto &bind) {
      return bind.at("node") == size_t(leaf - raw->nodes);
    }), "Native accessory morphs retain their own identity separate from original expression binds");
    for (size_t part = 0; part < leaf->mesh->primitives_count; ++part) {
      const auto &primitive = leaf->mesh->primitives[part];
      const auto joints = cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0);
      const auto weights = cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0);
      const auto positions = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
      check(joints && weights && positions && primitive.targets_count == 1, "Every material receives its real weight and morph accessors");
      const auto *attributes = primitive.targets[0].attributes;
      const auto *attributes_end = attributes + primitive.targets[0].attributes_count;
      const auto *position_delta = std::find_if(attributes, attributes_end,
          [](const auto &attribute) { return attribute.type == cgltf_attribute_type_position; });
      if (position_delta == attributes_end || !position_delta->data)
        throw std::runtime_error("Missing portable morph position accessor");
      const auto *morph = position_delta->data;
      for (size_t corner = 0; corner < primitive.indices->count; ++corner) {
        const auto vertex = cgltf_accessor_read_index(primitive.indices, corner);
        std::array<float, 3> p{}, delta{};
        std::array<cgltf_uint, 4> slots{};
        std::array<float, 4> influence{};
        check(cgltf_accessor_read_float(positions, vertex, p.data(), p.size()) &&
              cgltf_accessor_read_float(morph, vertex, delta.data(), delta.size()) &&
              cgltf_accessor_read_uint(joints, vertex, slots.data(), slots.size()) &&
              cgltf_accessor_read_float(weights, vertex, influence.data(), influence.size()), "Independent reader reads actual portable skin and morph attributes");
        std::array<double, 3> sum{};
        for (size_t n = 0; n < slots.size(); ++n) {
          if (!influence[n]) continue;
          RigMatrix joint_world, inverse_bind;
          cgltf_node_transform_world(leaf->skin->joints[slots[n]], joint_world.data());
          if (!cgltf_accessor_read_float(leaf->skin->inverse_bind_matrices, slots[n], inverse_bind.data(), inverse_bind.size()))
            throw std::runtime_error("Invalid portable inverse bind accessor");
          const auto value = (AffineTransform(joint_world) * AffineTransform(inverse_bind)).point(
              {p[0] + .6f * delta[0], p[1] + .6f * delta[1], p[2] + .6f * delta[2]});
          for (size_t axis = 0; axis < 3; ++axis) sum[axis] += value[axis] * influence[n];
        }
        const auto native_position = local.positions.at(current_mesh.indices.at(part * 3 + corner));
        const auto world = model.point({native_position.x, native_position.y, native_position.z});
        check(std::hypot(sum[0] - world[0], sum[1] - world[1], sum[2] - world[2]) < 2e-5,
              "Actual portable joint/IBM/weight/morph evaluation agrees with the posed native vertex");
      }
    }
  };
  for (const auto &[stage, state] : std::array<std::pair<const char *, const Scene *>, 2>{{{"pose", &posed}, {"rest", &fitted}}}) {
    doc.replace(*state);
    settle();
    for (const auto format : {AssetExportFormat::Avatar, AssetExportFormat::EditableAvatar512}) {
      const bool editable = format != AssetExportFormat::Avatar;
      const auto path = out / (std::string("skin-") + stage + (editable ? "-editable.vrm" : ".vrm"));
      doc.export_asset(path, format).get();
      verify(*state, path);
      const auto restored = editable ? extract_native_authoring(bytes(path)) : extract_native_emission(bytes(path));
      check(restored && restored->encode() == state->encode(), "Both VRM modes restore the exact linked native authoring state");
    }
  }

  auto foreign = posed.snapshot();
  auto invalid = std::make_shared<DeformationState>(*foreign.object(accessory).deformation);
  std::swap(invalid->source_skin_joints.front(), invalid->source_skin_joints.back());
  foreign.object(accessory).deformation = invalid;
  doc.replace(foreign);
  const auto existing = out / "skin-pose-editable.vrm";
  const auto durable = bytes(existing);
  refused = false;
  try { doc.export_asset(existing, AssetExportFormat::EditableAvatar512).get(); } catch (const std::invalid_argument &) { refused = true; }
  check(refused && bytes(existing) == durable, "Forged source joint slots refuse to replace a durable avatar export");

  const auto recipients = weight_objects(posed, accessory, true);
  check(std::ranges::find(recipients, rig) != recipients.end() &&
        weight_joint_slot(posed.object(accessory),posed.object(rig),head) == head,
        "Weight painting resolves the native accessory and original skin by the shared source joint");
  const auto repeated_avatar = append_native_scene(posed, posed);
  check(weight_objects(repeated_avatar.scene,accessory,true) == recipients,
        "Linked weight painting excludes another instance of the same original file");
  const auto corner = expected.positions.front();
  const auto brush_center = posed.world_matrix(accessory).point({corner.x,corner.y,corner.z});
  Brush weight;
  weight.from = weight.to = {float(brush_center[0]),float(brush_center[1]),float(brush_center[2])};
  weight.radius = .045f;
  WeightToolSettings weight_settings;
  weight_settings.enabled = true;
  weight_settings.mode = WeightBrushMode::Replace;
  weight_settings.value = .15f;
  weight_settings.strength = .5f;
  doc.replace(posed);
  settle();
  doc.weight_settings(weight_settings,head);
  settle();
  check(doc.frame().state->can_paint_weights && doc.begin_desktop(),
        "The normal desktop brush acquires the actual affine-linked accessory");
  doc.stamp_desktop(weight);
  doc.end_desktop();
  const auto weights_path = out / "skin-weight-painted.dsculpt";
  doc.save_async(weights_path).get();
  settle();
  const auto weighted = doc.copy_scene();
  const auto &painted_accessory = weighted.object(accessory);
  check(painted_accessory.deformation->skin->vertices != current.skin->vertices &&
        painted_accessory.deformation->source_skin_joints == current.source_skin_joints &&
        painted_accessory.texture_sets == paint && Scene::load(weights_path).encode() == weighted.encode(),
        "Weight painting persists edited accessory influences with the original joint link and paint intact");
  const auto weight_export = out / "skin-weight-painted-editable.vrm";
  doc.export_asset(weight_export,AssetExportFormat::EditableAvatar512).get();
  verify(weighted, weight_export);
  const auto replay = extract_native_authoring(bytes(weight_export));
  check(replay && replay->encode() == weighted.encode(),
        "Editable VRM retains the weight gesture and the linked accessory's exact authored state");
  surface_heat_texture_accessory(out, scene, accessory, rig);
}

void bone_accessory(const std::filesystem::path &out) {
  using namespace dome;
  std::filesystem::create_directories(out);
  auto fixture = metadata_source();
  for (auto &node : fixture.j["nodes"]) {
    if (!node.contains("extensions")) continue;
    node["extensions"].erase("VRMC_node_constraint");
    if (node["extensions"].empty()) node.erase("extensions");
  }
  auto &used = fixture.j["extensionsUsed"].get_ref<J::array_t &>();
  std::erase(used, J("VRMC_node_constraint"));
  const auto source_path = out / "bone-source.vrm";
  fixture.save(source_path);

  Scene initial;
  const auto accessory = initial.active().id;
  initial.active().name = "Native head light";
  initial.active().transform.position = {.13f, .14f, -.12f};
  Mesh mesh;
  mesh.positions = {{-.1f, -.1f, 0}, {.1f, -.1f, 0}, {.1f, .1f, 0}, {-.1f, .1f, 0}};
  mesh.normals.assign(4, {0, 0, 1});
  mesh.uv = {{{.1f, .1f}}, {{.9f, .1f}}, {{.9f, .9f}}, {{.1f, .9f}}};
  mesh.indices = {0, 1, 2, 0, 2, 3};
  std::array<TextureSetInitialField, 2> fields;
  fields[0].resolution = fields[1].resolution = 64;
  fields[0].material.color = {.4f, .3f, .2f, 1};
  fields[1].material.emissive = {1.5f, .5f, .125f};
  initialize_texture_sets(initial, accessory, mesh, fields, std::array<TextureSetId, 2>{0, 1});

  ImportAuthoringOptions settings;
  settings.paint_resolution = 64;
  auto imported = prepare_authoring_import(initial, source_path, settings);
  auto &scene = imported.scene;
  const auto rig = imported.objects.front();
  const auto head = scene.object(rig).deformation->humanoid->joints.at("head");
  const auto original_world = scene.world_matrix(accessory);
  scene.select(accessory);
  Document doc;
  doc.replace(scene);
  doc.save_async(out / "pre-attach.dsculpt").get();
  doc.attach_avatar_accessory(rig, 0, doc.draft_scene().source);
  doc.save_async(out / "default-bone.dsculpt").get();
  check(doc.wait_asset() && doc.frame().state->error.empty() && doc.undo() &&
        doc.copy_scene().encode() == scene.encode(), "Default bone attachment and ordered Save have one exact Undo");
  check(doc.wait_mesh() && doc.frame().state->can_attach_avatar_accessory,
        "Document enables the attachment tool for a selected native mesh beside a real avatar");
  doc.attach_avatar_accessory(rig, head, doc.draft_scene().source);
  const auto attached_path = out / "head-attached.dsculpt";
  doc.save_async(attached_path).get();
  check(doc.wait_asset() && doc.frame().state->error.empty(), "Real attachment worker completes before ordered Save");
  const auto attached = doc.copy_scene();
  const auto endpoint = detail::source_accessory_attachment(attached.group_path(attached.object(accessory).group));
  const auto near = [](const auto &a, const auto &b) {
    for (size_t i = 0; i < a.size(); ++i) if (std::abs(a[i] - b[i]) > 4e-6) return false;
    return true;
  };
  check(endpoint && endpoint->source_node == scene.object(rig).appearance->source_joint_nodes[head] &&
        near(original_world.matrix(), attached.world_matrix(accessory).matrix()) &&
        Scene::load(attached_path).encode() == attached.encode(),
        "Attachment preserves exact native records and current placement under the real head bone");
  check(doc.undo() && doc.copy_scene().encode() == scene.encode() && doc.redo() &&
        doc.copy_scene().encode() == attached.encode(), "Attachment is a single exact native Undo and Redo");
  doc.attach_avatar_accessory(rig, head, doc.draft_scene().source);
  check(doc.wait_asset() && doc.copy_scene().encode() == attached.encode() && doc.undo() &&
        doc.copy_scene().encode() == scene.encode() && doc.redo(), "Reattaching to the same bone adds no history entry");
  auto other_bone = scene.snapshot();
  const auto hand = scene.object(rig).deformation->humanoid->joints.at("rightHand");
  detail::attach_source_accessory(other_bone, accessory, rig, hand, {});
  const auto hand_endpoint = detail::source_accessory_attachment(other_bone.group_path(other_bone.object(accessory).group));
  check(hand_endpoint && hand_endpoint->source_node == scene.object(rig).appearance->source_joint_nodes[hand] &&
        other_bone.groups().size() > scene.groups().size() && near(original_world.matrix(), other_bone.world_matrix(accessory).matrix()),
        "A non-rendered source joint gains its actual missing placement chain without moving the accessory");

  RigEdit pose;
  pose.joint = JointAdjustment{head, {0, 15, 8}, {.02f, 0, -.01f}};
  doc.select_object(rig);
  doc.edit_scene("Edit rig and shapekeys", [rig, pose](Scene &value, const auto &cancelled) {
    edit_rig(value, rig, pose, cancelled);
  }, doc.draft_scene().source);
  check(doc.wait_asset() && doc.frame().state->error.empty(), "Native rig editor worker publishes the linked pose");
  doc.select_object(accessory);
  const auto posed = doc.copy_scene();
  auto bone_world = [&](const Scene &state) {
    const auto &deformation = *state.object(rig).deformation;
    return state.world_matrix(rig) * AffineTransform(deformation.skeleton->world(deformation.pose).at(head));
  };
  const auto predicted = bone_world(posed) * bone_world(attached).inverse() * attached.world_matrix(accessory);
  check(!near(original_world.matrix(), predicted.matrix()) && near(predicted.matrix(), posed.world_matrix(accessory).matrix()),
        "Linked joints move the painted rigid accessory using the actual posed bone transform");
  check(posed.encode().at("texture_set_geometry") == attached.encode().at("texture_set_geometry") &&
        posed.object(accessory).texture_sets == attached.object(accessory).texture_sets,
        "Moving the original joint preserves accessory paint owners and fields");
  auto locked = attached.snapshot();
  locked.object(accessory).locked = true;
  const auto locked_before = locked.encode();
  bool refused = false;
  try { edit_rig(locked, rig, pose); } catch (const std::invalid_argument &) { refused = true; }
  check(refused && locked.encode() == locked_before, "A locked attached mesh refuses the whole linked pose");
  auto unlinked = attached.snapshot();
  auto separate = pose;
  separate.linked_pose = false;
  refused = false;
  try { edit_rig(unlinked, rig, separate); } catch (const std::invalid_argument &) { refused = true; }
  check(refused && unlinked.encode() == attached.encode(), "An attached accessory requires one shared pose across linked skin views");

  check(doc.wait_mesh() && doc.frame().state->can_export_avatar && doc.frame().state->can_export_editable_avatar,
        "The selected bone attachment enables both ordinary and editable export");
  doc.save_async(out / "head-posed.dsculpt").get();
  auto ids = imported.objects;
  ids.push_back(accessory);
  const auto subset = detail::emission_export_subset(posed, ids);
  export_vrm(detail::emission_export_subset(posed, imported.objects).export_surfaces(), out / "head-source-only.vrm");
  const auto verify_attachment = [&](const auto &path, const J &vrm, const AffineTransform &expected) {
    const auto joint = vrm.at("humanoid").at("humanBones").at("head").at("node").get<size_t>();
    cgltf_options options{};
    cgltf_data *raw = nullptr;
    check(cgltf_parse_file(&options, path.c_str(), &raw) == cgltf_result_success,
          "Independent cgltf opens the exported bone attachment");
    const auto owner = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>(raw, cgltf_free);
    check(joint < raw->nodes_count && cgltf_load_buffers(&options, raw, path.c_str()) == cgltf_result_success &&
          cgltf_validate(raw) == cgltf_result_success, "Independent reader validates the emitted skin and accessory resources");
    const auto *find = std::find_if(raw->nodes, raw->nodes + raw->nodes_count, [](const auto &node) {
      return node.mesh && node.name && std::string_view(node.name) == "Native head light";
    });
    check(find != raw->nodes + raw->nodes_count && !find->skin && find->mesh->primitives_count == 2,
          "Both material partitions remain on one genuinely rigid accessory node");
    bool child = false;
    for (auto parent = find->parent; parent; parent = parent->parent) child |= parent == raw->nodes + joint;
    std::array<float, 16> world;
    cgltf_node_transform_world(find, world.data());
    check(child && near(world, expected.float_matrix()), "Portable accessory is a real child of the posed head and has the same complete affine transform");
    return joint;
  };
  for (const auto format : {AssetExportFormat::Avatar, AssetExportFormat::EditableAvatar512}) {
    const bool editable = format != AssetExportFormat::Avatar;
    const auto path = out / (editable ? "head-editable.vrm" : "head-standard.vrm");
    doc.export_asset(path, format).get();
    const auto j = json(path);
    const auto &vrm = j.at("extensions").at("VRMC_vrm");
    const auto joint = verify_attachment(path, vrm, predicted);
    const auto &spring = j.at("extensions").at("VRMC_springBone").at("springs").at(0);
    check(spring.at("center") == joint && spring.at("joints").size() == 2 &&
          vrm.at("expressions").at("preset").at("happy").at("materialColorBinds").size() == imported.objects.size(),
          "Original spring and expression references remain on their imported owners");
    const auto restored = editable ? extract_native_authoring(bytes(path)) : extract_native_emission(bytes(path));
    check(restored && restored->encode() == subset.encode(), "Export reopens with the exact attachment, paint and source hierarchy");
  }

  fixture.j["extensions"].erase("VRMC_springBone");
  std::erase(used, J("VRMC_springBone"));
  const auto fitted_source = out / "fitted-source.vrm";
  fixture.save(fitted_source);
  auto fitted_import = prepare_authoring_import(initial, fitted_source, settings);
  auto &fitted = fitted_import.scene;
  check(fitted_import.objects.front() == rig, "The fitted accessory fixture keeps the same native rig identity and slots");
  detail::attach_source_accessory(fitted, accessory, rig, head, {});
  const auto fit_before = fitted.world_matrix(accessory);
  const auto pivot = bone_world(fitted).point({0, 0, 0});
  RigRestJointEdit fit;
  fit.joint = head;
  fit.world_position = Vec3{float(pivot[0] + .006), float(pivot[1] + .002), float(pivot[2])};
  check(author_rig_rest(fitted, rig, {&fit, 1}) && fitted.world_matrix(accessory).matrix() == fit_before.matrix(),
        "Fitting the original rest pivot preserves the exact current accessory placement");
  const auto fitted_bone = bone_world(fitted);
  edit_rig(fitted, rig, pose);
  const auto fitted_predicted = bone_world(fitted) * fitted_bone.inverse() * fit_before;
  check(near(fitted_predicted.matrix(), fitted.world_matrix(accessory).matrix()),
        "A linked pose uses the fitted head's actual pivot to move the native accessory");
  fitted.select(accessory);
  doc.replace(fitted);
  for (const auto format : {AssetExportFormat::Avatar, AssetExportFormat::EditableAvatar512}) {
    const bool editable = format != AssetExportFormat::Avatar;
    const auto path = out / (editable ? "fitted-editable.vrm" : "fitted-standard.vrm");
    doc.export_asset(path, format).get();
    verify_attachment(path, json(path).at("extensions").at("VRMC_vrm"), fitted_predicted);
    const auto restored = editable ? extract_native_authoring(bytes(path)) : extract_native_emission(bytes(path));
    check(restored && restored->encode() == detail::emission_export_subset(fitted, ids).encode(),
          "Fitted export restores the exact current and first-rest owners with the attached mesh");
  }
  RigEdit reset;
  reset.reset_pose = true;
  edit_rig(fitted, rig, reset);
  check(near(fit_before.matrix(), fitted.world_matrix(accessory).matrix()) &&
        Scene::decode(fitted.encode()).encode() == fitted.encode(),
        "Reset follows the fitted rest attachment and the result survives a native restart");
  spatial_bone_accessory(out, scene, source_path, accessory);
  shape_texture_accessory(out, attached, accessory, rig, head);
  skinned_texture_accessory(out, initial);
}
} // namespace
#ifndef DOME_VRM1_SOURCE_EXPORT_NO_MAIN
int main(int argc, char **argv) try {
  if (argc < 3)
    return 2;
  if (std::string_view(argv[1]) == "--bone-accessory") {
    bone_accessory(argv[2]);
    std::cout << "PASS native VRM1 bone accessory workflow\n";
    return 0;
  }
  if (std::string_view(argv[1]) == "--fixture") {
    source(false).save(argv[2]);
    return 0;
  }
  const bool selftest = std::string_view(argv[1]) == "--selftest";
  std::filesystem::path input = argv[1], output = argv[2];
  if (selftest) {
    std::filesystem::create_directories(output);
    input = output / "source.vrm";
    metadata_source().save(input);
    output /= "current.vrm";
  }
  dome::ImportAuthoringOptions options;
  options.paint_resolution = 64;
  options.prepare_texture_atlas = false;
  auto imported = dome::prepare_authoring_import(dome::Scene{}, input, options);
  auto before = imported.scene.encode();
  std::cout << "Native VRM1 imported " << imported.objects.size() << " parts\n"
            << std::flush;
  try {
    auto report = dome::export_vrm(imported.scene.preview_surfaces(), output);
    if (imported.scene.encode() != before)
      throw std::runtime_error("Export modified native source");
    std::cout << "Native VRM1 export meshes=" << report.meshes
              << " joints=" << report.joints << '\n';
    independent(imported.scene, output);
    const auto project = output.parent_path() / "retained.dsculpt";
    imported.scene.save(project);
    check(Scene::load(project).encode() == before,
          "Native source/typed roles/transforms/morphs and raw V1 metadata "
          "restart exactly");
    if (selftest) {
      auto fixture = metadata_source();
      auto expected = fixture.j;
      detail::validate_vrm1_metadata(expected);
      const auto emitted = json(output);
      const auto &actual = emitted["extensions"]["VRMC_vrm"];
      for (const auto &mesh : emitted.at("meshes"))
        check(
            mesh.at("extras").at("authorOpaque") ==
                    expected.at("meshes")[0].at("extras").at("authorOpaque") &&
                mesh.at("primitives")[0].at("extras") ==
                    expected.at("meshes")[0].at("primitives")[0].at("extras"),
            "Opaque authored mesh and primitive extras retain exact JSON "
            "values on every current instance without replacing target names");
      check(actual["meta"]["name"] ==
                    expected["extensions"]["VRMC_vrm"]["meta"]["name"] &&
                actual["meta"]["authors"] ==
                    expected["extensions"]["VRMC_vrm"]["meta"]["authors"] &&
                actual["lookAt"] ==
                    expected["extensions"]["VRMC_vrm"]["lookAt"],
            "Actual author/license/look-at payloads retain version-specific "
            "source meaning");
      for (const auto &bone : actual["humanoid"]["humanBones"].items()) {
        auto source_node = expected["extensions"]["VRMC_vrm"]["humanoid"]
                                   ["humanBones"][bone.key()]["node"]
                                       .get<size_t>();
        auto current = bone.value()["node"].get<size_t>();
        check(emitted["nodes"][current]["name"] ==
                  expected["nodes"][source_node]["name"],
              "V1 humanoid object/thumb role references map to their actual "
              "original nodes");
      }
      for (const auto &annotation : actual["firstPerson"]["meshAnnotations"])
        check(
            emitted["nodes"][annotation["node"].get<size_t>()].contains("mesh"),
            "V1 first-person binds real emitted mesh nodes");
      const auto &expression = actual["expressions"]["preset"]["happy"];
      check(expression["isBinary"] == true &&
                expression["overrideBlink"] == "blend" &&
                expression["morphTargetBinds"].size() == 1 &&
                expression["materialColorBinds"].size() == 3 &&
                expression["textureTransformBinds"].size() == 3,
            "V1 expressions expand material forks without multiplying "
            "node-specific morph binds");
      for (const auto &bind : expression["materialColorBinds"])
        check(bind["targetValue"] == J({.4f, .5f, .6f, .7f}),
              "V1 absolute material target RGBA remains exact");
      const auto &spring = emitted["extensions"]["VRMC_springBone"];
      check(spring["specVersion"] == "1.0" &&
                spring["springs"][0]["joints"].size() == 2,
            "Typed spring version and explicit joint order survive emission");
      for (size_t k = 0; k < 2; ++k) {
        auto n = spring["springs"][0]["joints"][k]["node"].get<size_t>();
        check(emitted["nodes"][n]["name"] ==
                  (k ? "Actual spring tail" : "Actual spring root"),
              "Spring joints retain exact source-node identities and order");
      }
      size_t constraints = 0;
      for (const auto &node : emitted["nodes"])
        if (node.contains("extensions") &&
            node["extensions"].contains("VRMC_node_constraint")) {
          ++constraints;
          for (const auto &kind :
               node["extensions"]["VRMC_node_constraint"]["constraint"]
                   .items()) {
            const auto n = kind.value()["source"].get<size_t>();
            check(emitted["nodes"][n]["name"] ==
                      (kind.key() == "rotation"
                           ? "Original unweighted hair joint 129"
                           : "head"),
                  "Roll/aim/rotation constraints reference the actual emitted "
                  "source endpoint");
          }
        }
      check(constraints == 3, "All three real constraint kinds survive export");
      check(imported.scene.groups().size() < 128 &&
                imported.scene.object(imported.objects.front())
                        .appearance->asset->nodes.size() > 128,
            "Full original joint/reference graph survives without expanding "
            "the editable Scene group cap");
      for (size_t i = 0; i < 130; ++i)
        check(std::any_of(emitted["nodes"].begin(), emitted["nodes"].end(),
                          [&](const auto &node) {
                            return node.value("name", std::string{}) ==
                                   "Original unweighted hair joint " +
                                       std::to_string(i);
                          }),
              "Every unweighted original joint outside mesh-ancestor closure "
              "survives source export");
      check(imported.scene.object(imported.objects.front())
                        .appearance->asset->avatar_version ==
                    ImportedAvatarVersion::Vrm1 &&
                Scene::load(project)
                        .object(imported.objects.front())
                        .appearance->asset->avatar_version ==
                    ImportedAvatarVersion::Vrm1,
            "Derived V1 kind comes from actual immutable metadata and is "
            "recomputed on native replay");
      for (int invalid = 0; invalid < 6; ++invalid) {
        auto bad = fixture;
        if (invalid == 0)
          bad.j["extensions"]["VRMC_springBone"]["springs"][0]["joints"][1]
               ["node"] = 0;
        if (invalid == 1)
          for (size_t n = 0; n < bad.j["nodes"].size(); ++n)
            if (bad.j["nodes"][n].value("name", std::string{}) ==
                "Actual rotation constraint")
              bad.j["nodes"][n]["extensions"]["VRMC_node_constraint"]
                   ["constraint"]["rotation"]["source"] = n;
        if (invalid == 2)
          bad.j["extensions"]["VRMC_vrm"]["expressions"]["preset"]["happy"]
               ["morphTargetBinds"][0]["index"] = 99;
        if (invalid == 3)
          bad.j["extensions"]["VRMC_springBone"]["specVersion"] = "1.1";
        if (invalid == 4)
          bad.j["extensions"]["VRMC_vrm"]["specVersion"] = "1.1";
        if (invalid == 5)
          bad.j["extensions"]["VRMC_vrm"]["humanoid"]["humanBones"]["head"]
               ["node"] = 100000;
        const auto file = output.parent_path() /
                          ("invalid" + std::to_string(invalid) + ".vrm");
        bad.save(file);
        bool refused = false;
        try {
          (void)prepare_authoring_import(imported.scene, file, options);
        } catch (const std::exception &) {
          refused = true;
        }
        check(refused && imported.scene.encode() == before,
              "Malformed spring order/constraint ownership/morph "
              "identity/version refuses atomically");
      }
      for (int unsupported = 0; unsupported < 5; ++unsupported) {
        auto unrepresented = metadata_source();
        J opaque = {{"sourceNode", 0}};
        if (unsupported == 0)
          unrepresented.j["extensions"]["DOME_unrepresented"] = opaque;
        if (unsupported == 1)
          unrepresented.j["nodes"][0]["extensions"]["DOME_unrepresented"] =
              opaque;
        if (unsupported == 2)
          unrepresented.j["meshes"][0]["extensions"]["DOME_unrepresented"] =
              opaque;
        if (unsupported == 3)
          unrepresented.j["meshes"][0]["primitives"][0]["extensions"]
                         ["DOME_unrepresented"] = opaque;
        if (unsupported == 4)
          unrepresented.j["scenes"].push_back({{"nodes", {0}}});
        const auto file =
            output.parent_path() /
            ("unrepresented" + std::to_string(unsupported) + ".vrm");
        unrepresented.save(file);
        auto retained = prepare_authoring_import(Scene{}, file, options);
        auto exact = retained.scene.encode();
        const auto previous = bytes(output);
        bool refused = false;
        try {
          (void)export_vrm(retained.scene.preview_surfaces(), output);
        } catch (const std::invalid_argument &) {
          refused = true;
        }
        check(refused && bytes(output) == previous &&
                  retained.scene.encode() == exact,
              "Unrepresented actual extension/reference or multi-scene state "
              "refuses without losing raw source or replacing prior output");
      }
      import_fixture::Fixture unused;
      unused.j["extensionsUsed"].push_back("VRMC_vrm");
      const auto unused_path = output.parent_path() / "unused-tag.glb";
      unused.save(unused_path);
      auto ordinary = prepare_authoring_import(Scene{}, unused_path, options);
      check(ordinary.scene.object(ordinary.objects.front())
                    .appearance->asset->avatar_version ==
                ImportedAvatarVersion::None,
            "A mere unused extensionsUsed declaration creates no avatar "
            "metadata authority");
      Document ordinary_doc;
      ordinary_doc.replace(ordinary.scene);
      check(
          !ordinary_doc.frame().state->can_export_avatar,
          "Ordinary glTF with an unused VRM tag does not enable avatar export");
      const auto ordinary_path = output.parent_path() / "ordinary.dsculpt";
      ordinary.scene.save(ordinary_path);
      check(Scene::load(ordinary_path)
                    .object(ordinary.objects.front())
                    .appearance->asset->avatar_version ==
                ImportedAvatarVersion::None,
            "Unused-tag None kind survives real archive replay without "
            "namespace inference");
      auto invalid_meta = metadata_source();
      invalid_meta.j["extensions"]["VRMC_vrm"]["meta"]["references"] =
          J::array();
      const auto invalid_meta_path =
          output.parent_path() / "preserved-invalid-meta.vrm";
      invalid_meta.save(invalid_meta_path);
      auto retained_meta =
          prepare_authoring_import(Scene{}, invalid_meta_path, options);
      auto prior_output = bytes(output);
      bool meta_refused = false;
      try {
        (void)export_vrm(retained_meta.scene.preview_surfaces(), output);
      } catch (const std::invalid_argument &e) {
        meta_refused =
            std::string_view(e.what()).find("empty optional meta.references") !=
            std::string_view::npos;
      }
      check(meta_refused && bytes(output) == prior_output &&
                *retained_meta.scene.object(retained_meta.objects.front())
                        .appearance->original_bytes == invalid_meta.bytes(),
            "Schema-invalid empty reference list stays exact in native source "
            "while conforming export refuses atomically");
      auto atlas_options = options;
      atlas_options.prepare_texture_atlas = true;
      auto atlas = prepare_authoring_import(Scene{}, input, atlas_options);
      const auto atlas_before = atlas.scene.encode();
      const auto atlas_output = output.parent_path() / "source-aware-atlas.vrm";
      (void)export_vrm(atlas.scene.export_surfaces(), atlas_output);
      independent(atlas.scene, atlas_output);
      check(atlas.scene.encode() == atlas_before && imported.scene.encode() == before,
            "Authenticated pristine Source4 export keeps native writable UVs and complete caller sources exact");
      const auto atlas_json = json(atlas_output);
      const auto &atlas_expression = atlas_json["extensions"]["VRMC_vrm"]["expressions"]["preset"]["happy"];
      check(atlas_expression["textureTransformBinds"].size() == 3 &&
                atlas_expression["materialColorBinds"].size() == 3,
            "Actual source-aware V1 export retains expression reference closure across all real material forks");
      for (const auto &bind : atlas_expression["textureTransformBinds"])
        check(bind["scale"] == expected["extensions"]["VRMC_vrm"]["expressions"]["preset"]["happy"]["textureTransformBinds"][0]["scale"] &&
                  bind["offset"] == expected["extensions"]["VRMC_vrm"]["expressions"]["preset"]["happy"]["textureTransformBinds"][0]["offset"],
              "Actual source-aware expression target transforms retain exact authored values");
      bool atlas_refused = false;
      try { (void)export_vrm(atlas.scene.preview_surfaces(), output); }
      catch (const std::invalid_argument &) { atlas_refused = true; }
      check(atlas_refused && bytes(output) == prior_output && atlas.scene.encode() == atlas_before,
            "Uncompiled preview packets refuse atomically instead of silently reinterpreting source UV-expression targets");
      Document atlas_worker;
      const auto atlas_empty = atlas_worker.copy_scene().encode();
      ImportSettings atlas_settings;atlas_settings.paint_resolution = 64;atlas_settings.prepare_texture_atlas = true;
      atlas_worker.import_asset(input, atlas_settings);
      atlas_worker.save_async(output.parent_path() / "source-aware-worker.dsculpt").get();
      check(atlas_worker.wait_asset() && atlas_worker.wait_mesh() && atlas_worker.frame().state->error.empty(),
            "Actual source-aware V1 worker import and immediately ordered Save complete");
      const auto atlas_accepted = atlas_worker.copy_scene().encode();
      check(atlas_accepted == atlas_before && Scene::load(output.parent_path() / "source-aware-worker.dsculpt").encode() == atlas_accepted,
            "Actual V1 worker Save/restart retains exact original metadata and independent writable authority");
      const auto atlas_worker_output = output.parent_path() / "source-aware-worker.vrm";
      atlas_worker.export_asset(atlas_worker_output, AssetExportFormat::Avatar).get();
      independent(atlas_worker.copy_scene(), atlas_worker_output);
      check(bytes(atlas_worker_output) == bytes(atlas_output) && atlas_worker.copy_scene().encode() == atlas_accepted &&
                atlas_worker.undo() && atlas_worker.copy_scene().encode() == atlas_empty &&
                atlas_worker.redo() && atlas_worker.copy_scene().encode() == atlas_accepted,
            "Actual source-aware Avatar FileJob agrees with detached export and preserves one exact Undo/Redo endpoint");
      auto painted_atlas = atlas.scene.snapshot();
      auto &painted_object = painted_atlas.object(atlas.objects.front());
      auto actual_paint = painted_object.painted->snapshot();
      PaintSettings ink;ink.color = {.95f, .01f, .8f, 1};ink.front_only = false;
      Brush brush;brush.from = brush.to = {0, 0, 0};brush.radius = 1;brush.strength = 1;
      actual_paint->begin_stroke();actual_paint->stamp(brush, ink);
      check(actual_paint->end_stroke(), "Genuine prepared Source4 V1 paint is a real changed authority");
      painted_object.painted = actual_paint;
      const auto painted_before = painted_atlas.encode();
      bool painted_preview_refused = false, painted_standard_refused = false;
      try { (void)export_vrm(painted_atlas.preview_surfaces(), atlas_output); }
      catch (const std::exception &) { painted_preview_refused = true; }
      try { (void)export_vrm(painted_atlas.export_surfaces(), atlas_output); }
      catch (const std::exception &) { painted_standard_refused = true; }
      check(painted_before != atlas_before && painted_preview_refused && painted_standard_refused &&
                bytes(atlas_output) == bytes(atlas_worker_output) &&
                painted_atlas.encode() == painted_before && atlas.scene.encode() == atlas_before,
            "Actual nonempty V1 paint requires its real animated/compiler adapter; neither strict route drops it or replaces prior output");
      Document doc;
      auto baseline = doc.copy_scene().encode();
      ImportSettings settings;
      settings.paint_resolution = 64;
      doc.import_asset(input, settings);
      auto saved = doc.save_async(output.parent_path() / "worker.dsculpt");
      saved.get();
      check(doc.wait_asset() && doc.frame().state->error.empty() &&
                doc.copy_scene().encode() == before,
            "Actual native import plus immediate ordered Save preserves exact "
            "V1 source");
      check(doc.frame().state->can_export_avatar,
            "Existing native avatar export command recognizes authenticated V1 "
            "source groups");
      auto future = doc.export_asset(output.parent_path() / "worker.vrm",
                                     AssetExportFormat::Avatar);
      future.get();
      check(doc.copy_scene().encode() == before,
            "Actual accepted native export leaves Scene and immutable source "
            "unchanged");
      check(doc.undo() && doc.copy_scene().encode() == baseline,
            "Native V1 admission is one complete global Undo");
      check(doc.redo() && doc.copy_scene().encode() == before,
            "Native V1 Redo retains every source/typed reference");
      auto posed_source = metadata_source();
      posed_source.j["extensions"].erase("VRMC_springBone");
      for (auto &node : posed_source.j["nodes"])
        if (node.contains("extensions")) {
          node["extensions"].erase("VRMC_node_constraint");
          if (node["extensions"].empty())
            node.erase("extensions");
        }
      auto &used = posed_source.j["extensionsUsed"];
      for (auto at = used.begin(); at != used.end();)
        if (*at == "VRMC_springBone" || *at == "VRMC_node_constraint")
          at = used.erase(at);
        else
          ++at;
      const auto posed_input = output.parent_path() / "kinematic-source.vrm";
      posed_source.save(posed_input);
      Document kinematic;
      kinematic.import_asset(posed_input, settings);
      check(kinematic.wait_asset() && kinematic.frame().state->error.empty(),
            "A separate genuine V1 source declares kinematic authoring without "
            "physics constraints");
      auto resting = kinematic.copy_scene();
      uint64_t active = 0;
      for (const auto &o : resting.objects())
        if (o.deformation && o.deformation->skin) {
          active = o.id;
          break;
        }
      check(active != 0,
            "Kinematic V1 fixture uses a real nonzero weighted source skin");
      kinematic.select_object(active);
      resting = kinematic.copy_scene();
      kinematic.edit_scene(
          "Pose actual V1 source",
          [active](Scene &scene, const std::function<bool()> &cancelled) {
            RigEdit edit;
            edit.joint = JointAdjustment{
                scene.object(active).deformation->humanoid->joints.at("head"),
                {0, 0, 13},
                {}};
            edit_rig(scene, active, edit, cancelled);
          });
      auto pose_saved =
          kinematic.save_async(output.parent_path() / "kinematic.dsculpt");
      pose_saved.get();
      check(kinematic.wait_asset() && kinematic.frame().state->error.empty(),
            "Native ordered worker transports one current source pose across "
            "distinct skins and a genuine rigid descendant");
      auto current = kinematic.copy_scene();
      check(current.encode() != resting.encode() &&
                Scene::load(output.parent_path() / "kinematic.dsculpt")
                        .encode() == current.encode(),
            "Current linked V1 pose and rigid compensation persist exactly "
            "before export");
      for (const auto &o : resting.objects())
        if (o.appearance)
          check(current.object(o.id).appearance == o.appearance &&
                    current.object(o.id).painted->mesh_owner() ==
                        o.painted->mesh_owner() &&
                    current.object(o.id).painted->encode() ==
                        o.painted->encode(),
                "Current source posing retains exact "
                "mesh/paint/raw-image/material/metadata owners");
      auto pose_export = kinematic.export_asset(
          output.parent_path() / "kinematic.vrm", AssetExportFormat::Avatar);
      pose_export.get();
      independent(current, output.parent_path() / "kinematic.vrm");
      check(kinematic.undo() &&
                kinematic.copy_scene().encode() == resting.encode() &&
                kinematic.redo() &&
                kinematic.copy_scene().encode() == current.encode(),
            "Linked kinematic V1 pose remains one exact native Undo/Redo");
      bone_accessory(output.parent_path() / "bone-accessory");
    }
    return argc > 3 ? 1 : 0;
  } catch (const std::exception &e) {
    std::cerr << "VRM1 export refusal: " << e.what() << '\n';
    if (argc > 3 &&
        std::string_view(e.what()).find("only original VRM0") !=
            std::string_view::npos &&
        imported.scene.encode() == before)
      return 0;
    return 1;
  }
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}

#endif // DOME_VRM1_SOURCE_EXPORT_NO_MAIN
