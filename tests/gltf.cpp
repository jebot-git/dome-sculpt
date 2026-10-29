// SPDX-License-Identifier: GPL-3.0-or-later
// The native importer supplies the single MIT cgltf implementation.
#include "cgltf.h"
#include "dome/scene.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/source-maps.hpp"
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

  // Independent source samplers/normal/AO survive both raw and painted modes.
  auto source = std::make_shared<dome::PaintSourceMaps>();
  auto tiny = std::make_shared<dome::Image>();
  tiny->width=tiny->height=1; tiny->rgba={127,128,255,191};
  source->color.image=source->normal.image=source->occlusion.image=tiny;
  source->normal.wrap_s=33648; source->normal.wrap_t=33071;
  source->normal.min_filter=9987; source->normal.mag_filter=9728;
  source->normal.uv_transform={0,-2,3,0,.25f,-.5f};
  source->normal_scale=.6f; source->occlusion_strength=.35f;
  auto source_maps=std::make_shared<dome::TextureMaps>();
  source_maps->source_only=true; source_maps->source_maps=source;
  objects[0].textures=source_maps; objects[0].material=blue;
  dome::export_glb(objects,path);
  raw=nullptr;
  require(cgltf_parse_file(&options,path.c_str(),&raw)==cgltf_result_success,
          "Independent parser accepts raw source textures");
  std::unique_ptr<cgltf_data,decltype(&cgltf_free)> source_file(raw,cgltf_free);
  require(cgltf_load_buffers(&options,raw,path.c_str())==cgltf_result_success &&
          cgltf_validate(raw)==cgltf_result_success,"Source texture GLB is valid");
  auto &sm=raw->materials[0];
  require(raw->images_count==1 && raw->textures_count==2 && raw->samplers_count==2,
          "Image bytes are deduplicated independently of sampler identities");
  require(sm.pbr_metallic_roughness.base_color_factor[0]==blue.color[0] &&
          sm.emissive_factor[1]==blue.emissive[1],"Raw source textures apply authored factors once");
  require(sm.normal_texture.texture && sm.normal_texture.scale==.6f &&
          sm.normal_texture.has_transform && sm.normal_texture.transform.offset[0]==.25f &&
          sm.normal_texture.texture->sampler->wrap_s==33648 &&
          sm.normal_texture.texture->sampler->min_filter==9987 &&
          sm.occlusion_texture.texture && sm.occlusion_texture.scale==.35f,
          "Normal/AO scale, UV transform and independent samplers survive export");
  auto imported=std::make_shared<dome::ImportedAsset>();
  dome::ImportedPrimitive source_primitive; source_primitive.mesh=mesh; source_primitive.source_mesh=0; source_primitive.source_primitive=0;
  source_primitive.uv1=mesh.uv;
  source_primitive.colors.assign(mesh.positions.size(),{.2f,.4f,.6f,.8f});
  source_primitive.tangents.assign(mesh.positions.size(),{1,0,0,-1});
  imported->primitives.push_back(source_primitive);
  auto source_identity=std::make_shared<dome::CreatorAppearance>();source_identity->asset=imported;
  source_identity->source_mesh=source_identity->source_primitive=0;
  for(uint32_t v=0;v<mesh.positions.size();++v)source_identity->source_vertices.push_back(v);
  objects[0].appearance=source_identity;
  dome::export_glb(objects,path);raw=nullptr;
  require(cgltf_parse_file(&options,path.c_str(),&raw)==cgltf_result_success,
          "Independent parser accepts preserved secondary UV/color/tangent attributes");
  std::unique_ptr<cgltf_data,decltype(&cgltf_free)> attributes_file(raw,cgltf_free);
  require(cgltf_load_buffers(&options,raw,path.c_str())==cgltf_result_success &&
          cgltf_validate(raw)==cgltf_result_success &&
          raw->materials[0].pbr_metallic_roughness.base_color_texture.texcoord==0,
          "Source material retains its explicit supported UV binding");
  auto &source_p=raw->meshes[0].primitives[0];float source_value[4];
  require(cgltf_find_accessor(&source_p,cgltf_attribute_type_texcoord,1) &&
          cgltf_accessor_read_float(cgltf_find_accessor(&source_p,cgltf_attribute_type_color,0),0,source_value,4) && source_value[2]==.6f,
          "Original compacted vertex color and secondary UV accessors survive exactly");
  require(cgltf_accessor_read_float(cgltf_find_accessor(&source_p,cgltf_attribute_type_tangent,0),0,source_value,4) && source_value[3]==-1,
          "Source tangent handedness remains authored");
  auto derived=std::make_shared<dome::TextureMaps>(*maps); derived->source_maps=source;
  objects[0].textures=derived; dome::export_glb(objects,path);
  raw=nullptr;
  require(cgltf_parse_file(&options,path.c_str(),&raw)==cgltf_result_success,
          "Independent parser accepts derived textures with source normal/AO");
  std::unique_ptr<cgltf_data,decltype(&cgltf_free)> derived_file(raw,cgltf_free);
  require(cgltf_load_buffers(&options,raw,path.c_str())==cgltf_result_success &&
          cgltf_validate(raw)==cgltf_result_success &&
          raw->materials[0].normal_texture.texture && raw->materials[0].occlusion_texture.texture &&
          raw->materials[0].pbr_metallic_roughness.base_color_factor[0]==1,
          "Derived composites retain source normal/AO without double factors");
  auto invalid_source=std::make_shared<dome::PaintSourceMaps>(*source);
  invalid_source->normal.uv_transform={1,.5f,0,1,0,0};
  auto invalid_maps=std::make_shared<dome::TextureMaps>(*source_maps); invalid_maps->source_maps=invalid_source;
  objects[0].textures=invalid_maps;
  bool shear_rejected=false; try{dome::export_glb(objects,path);}catch(const std::invalid_argument&){shear_rejected=true;}
  require(shear_rejected,"Unrepresentable texture shear rejects instead of changing texture coordinates");
  objects[0].textures=maps; objects[0].material=dome::Material{};

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
  require(raw->images_count == 3, "Rig export retains painted PBR images");
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
  // Nested creator groups must affect joint world matrices once, while a
  // skinned surface stays an identity scene root per glTF skin semantics.
  objects[0].deformation = std::make_shared<const dome::DeformationState>(rig);
  dome::CanvasTransform group_transform{};
  group_transform.position = {1,2,-.5f}; group_transform.scale = 1.3f;
  group_transform.orientation = {0,std::sin(.2f),0,std::cos(.2f)};
  dome::CanvasTransform nested{}; nested.position = {.3f,-.2f,.1f}; nested.scale = .7f;
  objects[0].groups = {{71,0,"Avatar group",group_transform,
                         std::make_shared<const dome::HumanoidControls>()},
                       {72,71,"Nested group",nested,{}}};
  dome::export_glb(objects, path);
  raw = nullptr;
  require(cgltf_parse_file(&options, path.c_str(), &raw) == cgltf_result_success,
          "Independent parser accepts grouped skin export");
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> grouped(raw, cgltf_free);
  require(cgltf_load_buffers(&options, raw, path.c_str()) == cgltf_result_success &&
          cgltf_validate(raw) == cgltf_result_success,
          "Independent validator accepts nested group/skin hierarchy");
  auto &grouped_skin = raw->skins[0];
  require(raw->nodes_count == 9 && raw->nodes[1].parent == &raw->nodes[0] &&
          grouped_skin.skeleton->parent == &raw->nodes[1],
          "Native group parents are actual glTF hierarchy nodes");
  bool root_surface = false;
  for (size_t i = 0; i < raw->nodes_count; ++i)
    if (raw->nodes[i].skin) root_surface = raw->nodes[i].parent == nullptr;
  require(root_surface, "Grouped skinned surface retains identity root placement");
  dome::Vec3 grouped_weighted{};
  for (size_t i = 0; i < 5; ++i) {
    float inverse_bind[16], world[16];
    require(cgltf_accessor_read_float(grouped_skin.inverse_bind_matrices, i, inverse_bind, 16),
            "Grouped inverse binds remain independently readable");
    cgltf_node_transform_world(grouped_skin.joints[i], world);
    grouped_weighted += transform(world, transform(inverse_bind, position)) * .2f;
  }
  auto native_world = objects[0].world_transform();
  auto grouped_position = preview.positions[0];
  auto expected_grouped = ob::transform(native_world, {grouped_position.x,grouped_position.y,grouped_position.z});
  require(dome::length(grouped_weighted - dome::Vec3{expected_grouped.x,expected_grouped.y,expected_grouped.z}) < 3e-5f,
          "Independent grouped skin evaluation matches native preview exactly once");
  auto grouped_json = nlohmann::json::parse(raw->json, raw->json + raw->json_size);
  require(grouped_json["nodes"][0]["extras"]["domeSculptCreator"]["type"] == "humanoid",
          "Creator recipe metadata survives glTF grouping without claiming VRM");
  if (const char *fixture = std::getenv("DOME_GLTF_FIXTURE"))
    std::filesystem::copy_file(path, std::string(fixture) + ".grouped.glb",
                               std::filesystem::copy_options::overwrite_existing);
  auto appearance=std::make_shared<dome::CreatorAppearance>();
  appearance->asset=std::make_shared<dome::ImportedAsset>();
  objects[0].appearance=appearance;
  auto sibling=objects[0]; sibling.id=2; sibling.name="Other creator primitive";
  dome::export_glb({objects[0],sibling},path);
  raw=nullptr;
  require(cgltf_parse_file(&options,path.c_str(),&raw)==cgltf_result_success,
          "Independent parser accepts a multipart shared creator rig");
  std::unique_ptr<cgltf_data,decltype(&cgltf_free)> shared_file(raw,cgltf_free);
  require(cgltf_load_buffers(&options,raw,path.c_str())==cgltf_result_success &&
          cgltf_validate(raw)==cgltf_result_success && raw->skins_count==1,
          "Creator primitives share one skin and actual bone hierarchy");
  size_t shared_meshes=0; for(size_t i=0;i<raw->nodes_count;++i)
    if(raw->nodes[i].skin==&raw->skins[0]) ++shared_meshes;
  require(shared_meshes==2 && raw->skins[0].joints_count==5,
          "Both independent meshes bind the identical five joint nodes");
  sibling.transform.position.x+=.1f; dome::export_glb({objects[0],sibling},path);
  raw=nullptr;
  require(cgltf_parse_file(&options,path.c_str(),&raw)==cgltf_result_success,
          "Differently transformed creator parts remain exportable");
  std::unique_ptr<cgltf_data,decltype(&cgltf_free)> distinct_file(raw,cgltf_free);
  require(raw->skins_count==2,"Different object transforms cannot silently merge distinct rigs");
  objects[0].groups[1].parent = 71;
  auto conflict = objects[0]; conflict.groups[0].transform.scale = 2;
  bool conflict_rejected = false;
  try { dome::export_glb(std::vector<dome::ObjectSurface>{objects[0],conflict},path); }
  catch (const std::invalid_argument &) { conflict_rejected = true; }
  require(conflict_rejected, "Conflicting group definitions cannot export ambiguous parents");
  std::filesystem::remove(path);
  std::cout << "PASS Independent cgltf validation, attributes, indices and "
               "exact metre-space geometry\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
