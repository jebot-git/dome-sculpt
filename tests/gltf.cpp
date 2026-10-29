// SPDX-License-Identifier: GPL-3.0-or-later
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "dome/scene.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <png.h>
#include <unistd.h>
int main() try {
  dome::Volume field(.04f);
  field.begin_stroke();
  dome::Brush b;
  b.radius = .4f;
  field.stamp(b);
  field.end_stroke();
  auto mesh = dome::surface_mesh(field);
  dome::unwrap(mesh, 512);
  auto path = std::filesystem::temp_directory_path() /
              ("dome-gltf-interop-" + std::to_string(getpid()) + ".glb");
  dome::export_glb(mesh, path);
  cgltf_options options{};
  cgltf_data *raw = nullptr;
  auto require = [](bool ok, const char *what) {
    if (!ok)
      throw std::runtime_error(what);
  };
  require(cgltf_parse_file(&options, path.c_str(), &raw) ==
              cgltf_result_success,
          "Independent glTF parser rejected the GLB");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw, cgltf_free);
  require(cgltf_load_buffers(&options, raw, path.c_str()) ==
              cgltf_result_success,
          "Independent glTF buffer loading failed");
  require(cgltf_validate(raw) == cgltf_result_success,
          "Independent glTF validation failed");
  require(raw->meshes_count == 1 && raw->meshes[0].primitives_count == 1,
          "Exported mesh hierarchy mismatch");
  auto &p = raw->meshes[0].primitives[0];
  require(p.indices && p.indices->count == mesh.indices.size() &&
              p.attributes_count == 3,
          "Exported attributes mismatch");
  auto positions = cgltf_find_accessor(&p, cgltf_attribute_type_position, 0);
  require(positions && positions->count == mesh.positions.size(),
          "Position accessor mismatch");
  for (size_t i = 0; i < mesh.positions.size(); ++i) {
    float value[3];
    require(cgltf_accessor_read_float(positions, i, value, 3),
            "Cannot read exported position");
    require(value[0] == mesh.positions[i].x &&
                value[1] == mesh.positions[i].y &&
                value[2] == mesh.positions[i].z,
            "UV normalization changed the exported metre-space geometry");
  }
  for (size_t i = 0; i < mesh.indices.size(); ++i)
    require(cgltf_accessor_read_index(p.indices, i) == mesh.indices[i],
            "Exported index differs from the sculpt surface");
  require(p.material && p.material->has_pbr_metallic_roughness,
          "PBR metallic-roughness material is missing");
  dome::Material blue;
  blue.name = "Blue metal";
  blue.color = {.04f, .12f, .8f, .5f};
  blue.metallic = .85f;
  blue.roughness = .2f;
  blue.emissive = {.01f, .02f, .03f};
  blue.double_sided = true;
  std::vector<dome::ObjectSurface> objects = {
      {1, "Left sculpt", {}, dome::Material{}, mesh},
      {2,
       "Right sculpt",
       {{.6f, .1f, -.2f}, {0, 0, .6f, .8f}, .75f},
       blue,
       mesh}};
  dome::export_glb(objects, path);
  raw = nullptr;
  require(cgltf_parse_file(&options, path.c_str(), &raw) ==
              cgltf_result_success,
          "Independent parser rejected a multi-object scene");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> multi(raw, cgltf_free);
  require(cgltf_load_buffers(&options, raw, path.c_str()) ==
                  cgltf_result_success &&
              cgltf_validate(raw) == cgltf_result_success,
          "Independent validator rejected a multi-object scene");
  require(raw->nodes_count == 2 && raw->meshes_count == 2 &&
              raw->materials_count == 2 && raw->scene->nodes_count == 2,
          "Scene export preserves separate meshes, nodes and materials");
  auto &node = raw->nodes[1];
  auto &material = raw->materials[1];
  require(node.has_translation && node.has_rotation && node.has_scale &&
              node.translation[0] == .6f && node.rotation[2] == .6f &&
              node.scale[0] == .75f,
          "Object transforms use native glTF node transforms");
  require(
      material.pbr_metallic_roughness.base_color_factor[2] == blue.color[2] &&
          material.pbr_metallic_roughness.metallic_factor == blue.metallic &&
          material.pbr_metallic_roughness.roughness_factor == blue.roughness &&
          material.emissive_factor[1] == blue.emissive[1] &&
          material.double_sided &&
          material.alpha_mode == cgltf_alpha_mode_blend,
      "glTF preserves per-object PBR color, metalness, roughness, emission, "
      "alpha and culling");
  auto right = cgltf_find_accessor(&raw->meshes[1].primitives[0],
                                   cgltf_attribute_type_position, 0);
  float local[3];
  require(cgltf_accessor_read_float(right, 0, local, 3) &&
              local[0] == mesh.positions[0].x,
          "Second mesh accessors retain correct buffer offsets and local "
          "coordinates");
  dome::PaintedMesh painting(mesh, 128);
  dome::PaintSettings ink;
  ink.color = {.01f, .03f, .95f, 1};
  b.from = b.to = {0, 0, .38f};
  b.normal = {0, 0, 1};
  b.radius = .15f;
  painting.begin_stroke();
  painting.stamp(b, ink);
  painting.end_stroke();
  auto maps = painting.composite(dome::Material{});
  objects.resize(1);
  objects[0].textures = maps;
  dome::export_glb(objects, path);
  raw = nullptr;
  require(cgltf_parse_file(&options, path.c_str(), &raw) ==
              cgltf_result_success,
          "Independent parser rejected painted GLB");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> textured(raw, cgltf_free);
  require(cgltf_load_buffers(&options, raw, path.c_str()) ==
                  cgltf_result_success &&
              cgltf_validate(raw) == cgltf_result_success,
          "Independent validator rejected painted GLB");
  require(raw->images_count == 3 && raw->textures_count == 3,
          "Painted GLB embeds three PBR PNG textures");
  auto &pbr = raw->materials[0].pbr_metallic_roughness;
  require(
      pbr.base_color_texture.texture &&
          pbr.metallic_roughness_texture.texture &&
          raw->materials[0].emissive_texture.texture &&
          pbr.base_color_factor[0] == 1 && pbr.metallic_factor == 1 &&
          pbr.roughness_factor == 1,
      "glTF binds composite textures without applying material factors twice");
  const dome::Image *expected[] = {&maps->color, &maps->metal_rough,
                                   &maps->emission};
  for (size_t i = 0; i < 3; ++i) {
    auto &image = raw->images[i];
    require(image.buffer_view && image.mime_type &&
                std::string(image.mime_type) == "image/png",
            "Embedded texture references a PNG buffer view");
    auto view = image.buffer_view;
    auto bytes =
        static_cast<const uint8_t *>(view->buffer->data) + view->offset;
    png_image decoded{};
    decoded.version = PNG_IMAGE_VERSION;
    require(png_image_begin_read_from_memory(&decoded, bytes, view->size),
            "Independent PNG decoder rejected embedded image bytes");
    decoded.format = PNG_FORMAT_RGBA;
    std::vector<uint8_t> rgba(PNG_IMAGE_SIZE(decoded));
    require(png_image_finish_read(&decoded, nullptr, rgba.data(), 0, nullptr) &&
                decoded.width == expected[i]->width &&
                decoded.height == expected[i]->height &&
                rgba == expected[i]->rgba,
            "Embedded PNG preserves exact PBR texels and image orientation");
    png_image_free(&decoded);
  }
  auto detailed_maps = std::make_shared<dome::TextureMaps>(*maps);
  detailed_maps->normal = {128,128,std::vector<uint8_t>(128*128*4)};
  for (size_t i=0;i<detailed_maps->normal.rgba.size();++i)
    detailed_maps->normal.rgba[i]=std::array<uint8_t,4>{128,204,230,255}[i%4];
  objects[0].textures=detailed_maps;
  dome::export_glb(objects,path);
  raw=nullptr;
  require(cgltf_parse_file(&options,path.c_str(),&raw)==cgltf_result_success,
          "Independent parser accepts normal/AO baked GLB");
  std::unique_ptr<cgltf_data,decltype(&cgltf_free)> detailed(raw,cgltf_free);
  require(cgltf_load_buffers(&options,raw,path.c_str())==cgltf_result_success &&
          cgltf_validate(raw)==cgltf_result_success && raw->images_count==4 &&
          raw->materials[0].normal_texture.texture &&
          raw->materials[0].occlusion_texture.texture==raw->materials[0].pbr_metallic_roughness.metallic_roughness_texture.texture,
          "glTF exports a tangent normal map and binds AO to the shared linear ORM red channel");
  auto image=raw->materials[0].normal_texture.texture->image;
  auto view=image->buffer_view;
  auto normal_data=static_cast<const uint8_t*>(view->buffer->data)+view->offset;
  png_image normal{};normal.version=PNG_IMAGE_VERSION;
  require(png_image_begin_read_from_memory(&normal,normal_data,view->size),"Independent PNG decoder reads the normal map");
  normal.format=PNG_FORMAT_RGBA;std::vector<uint8_t> normal_bytes(PNG_IMAGE_SIZE(normal));
  require(png_image_finish_read(&normal,nullptr,normal_bytes.data(),0,nullptr) &&
          normal_bytes==detailed_maps->normal.rgba,
          "Normal-map PNG bytes preserve linear XYZ values without an sRGB conversion");
  png_image_free(&normal);

  // Full deformation is exported from the untouched authoring surface, never
  // from the already posed preview. Five influences require a second set.
  dome::DeformationState rig(mesh);
  rig.skeleton = dome::Skeleton{};
  for (int i = 0; i < 5; ++i) {
    dome::Bone bone;
    bone.name = "joint" + std::to_string(i);
    bone.parent = i ? 0 : -1;
    bone.rest.translation = {float(i) * .03f, .1f, 0};
    rig.skeleton->bones.push_back(bone);
  }
  rig.skin = dome::SkinWeights{};
  rig.skin->vertices.assign(mesh.positions.size(),
                            {{0, .2f}, {1, .2f}, {2, .2f}, {3, .2f}, {4, .2f}});
  rig.morphs = dome::MorphSet(mesh);
  auto target_positions = mesh.positions;
  for (auto &v : target_positions)
    v.z += .04f;
  rig.morphs->set_target(mesh, "forward", target_positions);
  rig.morph_weights = {.6f};
  for (auto &bone : rig.skeleton->bones)
    rig.pose.push_back(bone.rest);
  rig.pose[0].translation.y += .15f;
  rig.pose[2].rotation =
      dome::arcball({0, 1, 0}, dome::normalized({.2f, 1, .1f}));
  rig.expressions = dome::AvatarExpressions{{{"aa", {{"forward", 1}}}}};
  objects[0].transform = {{.3f, .2f, -.1f}, {0, 0, .6f, .8f}, .75f};
  objects[0].deformation = std::make_shared<const dome::DeformationState>(rig);
  dome::export_glb(objects, path);
  raw = nullptr;
  require(cgltf_parse_file(&options, path.c_str(), &raw) ==
              cgltf_result_success,
          "Independent parser accepts skin/morph GLB");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> skinned(raw, cgltf_free);
  require(cgltf_load_buffers(&options, raw, path.c_str()) ==
                  cgltf_result_success &&
              cgltf_validate(raw) == cgltf_result_success,
          "Independent validator accepts skin/morph GLB");
  require(raw->skins_count == 1 && raw->skins[0].joints_count == 5 &&
              raw->nodes_count == 7,
          "Skinned hierarchy has one shared object parent, mesh node and all "
          "joints");
  auto &skin = raw->skins[0];
  auto &primitive = raw->meshes[0].primitives[0];
  require(
      raw->nodes[1].skin == &skin && raw->nodes[1].parent == nullptr &&
          skin.joints[0]->parent == &raw->nodes[0],
      "Object TRS affects joints and skinned surface remains an identity root");
  auto j0 = cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0),
       j1 = cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 1);
  auto w0 = cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0),
       w1 = cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 1);
  require(j0 && j1 && w0 && w1 &&
              j0->component_type == cgltf_component_type_r_16u,
          "All five native influences survive JOINTS/WEIGHTS sets without "
          "truncation");
  float joints0[4], joints1[4], weights0[4], weights1[4];
  require(cgltf_accessor_read_float(j0, 0, joints0, 4) &&
              cgltf_accessor_read_float(j1, 0, joints1, 4) &&
              cgltf_accessor_read_float(w0, 0, weights0, 4) &&
              cgltf_accessor_read_float(w1, 0, weights1, 4),
          "Skin attribute accessors can be read independently");
  require(joints1[0] == 4 && weights1[0] == .2f && weights1[1] == 0 &&
              joints1[1] == 0,
          "Additional joint sets preserve weights and zero-pad unused slots");
  require(primitive.targets_count == 1 && raw->meshes[0].weights_count == 1 &&
              raw->meshes[0].weights[0] == .6f,
          "Morph deltas and default authored morph values are exported");
  auto extras = nlohmann::json::parse(raw->json, raw->json + raw->json_size)
                    .at("meshes")[0]
                    .at("extras");
  require(extras.at("targetNames")[0] == "forward" &&
              extras.at("domeSculptExpressions")["visemes"]["aa"][0]["shape"] ==
                  "forward",
          "Morph and vowel bindings preserve stable target identities in "
          "application extras");
  const cgltf_accessor *delta = nullptr;
  for (size_t i = 0; i < primitive.targets[0].attributes_count; ++i)
    if (primitive.targets[0].attributes[i].type ==
        cgltf_attribute_type_position)
      delta = primitive.targets[0].attributes[i].data;
  auto base = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
  float base_value[3], delta_value[3];
  require(delta && cgltf_accessor_read_float(base, 0, base_value, 3) &&
              cgltf_accessor_read_float(delta, 0, delta_value, 3) &&
              base_value[2] == mesh.positions[0].z &&
              std::abs(delta_value[2] - .04f) < 1e-6f,
          "GLB positions remain rest coordinates while targets contain only "
          "displacements");
  auto transform = [](const float *m, dome::Vec3 p) {
    return dome::Vec3{m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
                      m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
                      m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
  };
  auto preview = rig.preview(mesh);
  dome::Vec3 position{base_value[0] + delta_value[0] * .6f,
                      base_value[1] + delta_value[1] * .6f,
                      base_value[2] + delta_value[2] * .6f},
      weighted{};
  for (size_t i = 0; i < 5; ++i) {
    float inverse_bind[16], world[16];
    require(cgltf_accessor_read_float(skin.inverse_bind_matrices, i,
                                      inverse_bind, 16),
            "Inverse-bind MAT4 reads correctly");
    cgltf_node_transform_world(skin.joints[i], world);
    weighted += transform(world, transform(inverse_bind, position)) * .2f;
  }
  float object_world[16];
  cgltf_node_transform_world(&raw->nodes[0], object_world);
  require(dome::length(weighted -
                       transform(object_world, preview.positions[0])) < 2e-5f,
          "Independent glTF skin/morph evaluation matches native posed preview "
          "under object rotation/scale");
  require(raw->images_count == 4 && raw->materials[0].normal_texture.texture,
          "Rig export retains painted PBR and normal images");
  if (const char *fixture = std::getenv("DOME_GLTF_FIXTURE"))
    std::filesystem::copy_file(
        path, std::string(fixture) + ".skin.glb",
        std::filesystem::copy_options::overwrite_existing);
  auto incomplete = rig;
  incomplete.humanoid = dome::HumanoidMapping{{{"hips", 0}}};
  objects[0].deformation =
      std::make_shared<const dome::DeformationState>(incomplete);
  bool failed = false;
  try {
    dome::export_glb(objects, path);
  } catch (const std::exception &) {
    failed = true;
  }
  require(failed, "Incomplete humanoid semantics cannot masquerade as complete "
                  "export metadata");
  dome::Skeleton human;
  dome::HumanoidMapping mapping;
  for (const auto &role : dome::human_roles())
    if (role.required) {
      std::string parent = role.parent;
      while (!parent.empty() && !mapping.joints.contains(parent)) {
        for (const auto &r : dome::human_roles())
          if (r.name == parent) {
            parent = r.parent;
            break;
          }
      }
      dome::Bone bone;
      bone.name = role.name;
      bone.parent = parent.empty() ? -1 : int(mapping.joints.at(parent));
      bone.rest.translation = {.01f, .03f, .02f};
      mapping.joints[role.name] = uint32_t(human.bones.size());
      human.bones.push_back(bone);
    }
  auto avatar = rig;
  avatar.skeleton = human;
  avatar.pose.clear();
  avatar.humanoid = mapping;
  avatar.skin->vertices.assign(mesh.positions.size(), {{0, 1}});
  objects[0].deformation =
      std::make_shared<const dome::DeformationState>(avatar);
  dome::export_glb(objects, path);
  raw = nullptr;
  require(cgltf_parse_file(&options, path.c_str(), &raw) ==
              cgltf_result_success,
          "Independent parser accepts mapped humanoid GLB");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> humanoid(raw, cgltf_free);
  require(cgltf_load_buffers(&options, raw, path.c_str()) ==
                  cgltf_result_success &&
              cgltf_validate(raw) == cgltf_result_success,
          "Independent validator accepts mapped humanoid GLB");
  auto metadata = nlohmann::json::parse(raw->nodes[0].extras.data);
  auto mapped = metadata.at("domeSculptHumanoid").at("humanBones");
  require(mapped.size() == 15 &&
              mapped.at("head").at("node").get<size_t>() < raw->nodes_count &&
              raw->extensions_used_count == 0,
          "Complete humanoid roles reference actual joints without claiming "
          "full VRM export");
  if (const char *fixture = std::getenv("DOME_GLTF_FIXTURE"))
    std::filesystem::copy_file(
        path, std::string(fixture) + ".humanoid.glb",
        std::filesystem::copy_options::overwrite_existing);
  std::filesystem::remove(path);
  std::cout << "PASS Independent cgltf validation, attributes, indices and "
               "exact metre-space geometry\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
