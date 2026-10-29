// SPDX-License-Identifier: GPL-3.0-or-later
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "dome/scene.hpp"
#include <iostream>
#include <memory>
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
  std::filesystem::remove(path);
  std::cout << "PASS Independent cgltf validation, attributes, indices and "
               "exact metre-space geometry\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
