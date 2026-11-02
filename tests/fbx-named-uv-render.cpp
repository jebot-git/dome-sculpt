// SPDX-License-Identifier: GPL-3.0-or-later
#define main inherited_material_graphics_suite
#include "mtoon-render.cpp"
#undef main
#include "prepared-normal-consumer.hpp"
#include "dome/import-native.hpp"
#include "dome/import-authoring.hpp"
#include <ufbx.h>
#include <cstring>

namespace {
void write(const std::filesystem::path &path, const auto &bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  if (!out) throw std::runtime_error("Cannot write named-UV graphics fixture");
}
void place(std::string &text, const char *model, float x) {
  const std::string from = std::string(model) + " { Version:232 }";
  const auto at = text.find(from);
  if (at == std::string::npos) throw std::runtime_error("Missing fixture instance");
  const auto to = std::string(model) + " { Version:232\n Properties70: {\n"
      " P:\"Lcl Translation\",\"Lcl Translation\",\"\",\"A\"," + std::to_string(x) + ",-0.25,0\n"
      " P:\"Lcl Scaling\",\"Lcl Scaling\",\"\",\"A\",0.4,0.4,0.4\n }\n}";
  text.replace(at, from.size(), to);
}
std::map<std::string, int> independent_sets(const std::filesystem::path &path) {
  ufbx_load_opts options{};
  options.strict = true;
  options.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
  options.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
  options.target_axes = ufbx_axes_right_handed_y_up;
  options.target_unit_meters = 1;
  options.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
  std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> scene(
      ufbx_load_file(path.c_str(), &options, nullptr), ufbx_free_scene);
  check(bool(scene), "Independent raw FBX parser accepts separated source instances");
  std::map<std::string, int> sets;
  for (size_t n = 0; n < scene->nodes.count; ++n) {
    auto *node = scene->nodes[n];
    if (!node->mesh) continue;
    check(node->materials.count == 1, "Raw instance retains its material identity");
    const auto *material = node->materials[0];
    const auto *base = material->pbr.base_color.texture;
    const auto *normal = material->pbr.normal_map.texture;
    check(base && normal, "Independent raw material has colour and normal images");
    const std::string name(base->uv_set.data, base->uv_set.length);
    check(name == std::string(normal->uv_set.data, normal->uv_set.length),
          "This independent consumer fixture has one common named coordinate per role");
    int found = -1;
    for (size_t i = 0; i < node->mesh->uv_sets.count; ++i) {
      const auto &set = node->mesh->uv_sets[i];
      if (std::string(set.name.data, set.name.length) == name) {
        check(found < 0, "Independent raw named set is unique");
        found = int(i);
      }
    }
    check(found >= 0 && found <= 1, "Raw fixture resolves its actual named coordinate");
    sets.emplace(std::string(node->name.data, node->name.length), found);
  }
  check(sets.size() == 2 && sets.at("One") == 1 && sets.at("Two") == 0,
        "One shared source material independently means UV1 and UV0 on different meshes");
  return sets;
}
// The independent renderer consumes UV0. For this single-common-role fixture,
// bind that input to the accessor selected independently from the raw FBX.
// The general numeric fixture separately checks every coordinate/tangent corner.
void bind_coordinate(cgltf_data *data, const std::map<std::string, int> &sets,
                     bool wrong = false, bool wrong_tangent = false) {
  for (size_t n = 0; n < data->nodes_count; ++n) {
    auto &node = data->nodes[n];
    if (!node.mesh) continue;
    const auto expected = sets.at(node.name);
    for (size_t k = 0; k < node.mesh->primitives_count; ++k) {
      auto &primitive = node.mesh->primitives[k];
      check(primitive.material && primitive.material->normal_texture.texcoord == expected &&
            primitive.material->pbr_metallic_roughness.base_color_texture.texcoord == expected,
            "Parsed canonical role indices agree with independent raw named bindings");
      auto uv = prepared_normal_consumer::attr(primitive.attributes, primitive.attributes_count,
                                              cgltf_attribute_type_texcoord, wrong ? 1 - expected : expected);
      check(uv, "Independent requested coordinate accessor exists");
      for (size_t a = 0; a < primitive.attributes_count; ++a)
        if (primitive.attributes[a].type == cgltf_attribute_type_texcoord && primitive.attributes[a].index == 0)
          primitive.attributes[a].data = const_cast<cgltf_accessor *>(uv);
      if (wrong_tangent && expected == 1) {
        auto tangent = prepared_normal_consumer::attr(primitive.attributes, primitive.attributes_count,
                                                      cgltf_attribute_type_tangent);
        check(tangent && tangent->component_type == cgltf_component_type_r_32f &&
              tangent->type == cgltf_type_vec4, "Tangent negative has explicit float4 authority");
        const std::array<float, 4> opposite_basis{1, 0, 0, 1};
        auto *bytes = static_cast<unsigned char *>(tangent->buffer_view->buffer->data) +
                      tangent->buffer_view->offset + tangent->offset;
        for (size_t v = 0; v < tangent->count; ++v)
          std::memcpy(bytes + v * tangent->stride, opposite_basis.data(), sizeof(opposite_basis));
      }
    }
  }
}
}

int main(int argc, char **argv) try {
  if (argc < 3) return 2;
  const auto out = std::filesystem::absolute(argv[2]);
  std::filesystem::create_directories(out);
  std::ifstream file(argv[1], std::ios::binary);
  std::string source{std::istreambuf_iterator<char>(file), {}};
  place(source, "Model:21,\"Model::One\",\"Mesh\"", -.65f);
  place(source, "Model:22,\"Model::Two\",\"Mesh\"", .15f);
  const auto input = out / "named.fbx";
  write(input, source);
  Image color{16, 16, std::vector<uint8_t>(16 * 16 * 4)};
  for (size_t y = 0; y < 16; ++y) for (size_t x = 0; x < 16; ++x) {
    const auto i = (y * 16 + x) * 4;
    color.rgba[i] = uint8_t(30 + x * 12);
    color.rgba[i + 1] = uint8_t(40 + y * 11);
    color.rgba[i + 2] = uint8_t(215 - x * 8);
    color.rgba[i + 3] = 255;
  }
  write(out / "color.png", color.png());
  write(out / "normal.png", Image{1, 1, {230, 153, 204, 255}}.png());
  const auto sets = independent_sets(input);
  auto capture = capture_native_source(input);
  const auto canonical = out / "canonical.glb";
  write(canonical, *capture.canonical_glb);
  NativeGraphics context;
  const std::string device = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
  std::cout << "GPU " << device << '\n';
  check(argc < 4 || std::string_view(argv[3]) != "--hardware" ||
        (device.find("Intel") != std::string::npos && device.find("Arc") != std::string::npos && device.find("A770") != std::string::npos),
        "Requested actual Intel Arc A770 identity is present");
  Fbo fbo;
  fbo.bind();
  SurfaceRenderer native;
  prepared_normal_consumer::Renderer consumer;
  cgltf_data empty{};
  consumer.render(&empty);
  const auto blank = read();
  prepared_normal_consumer::Model expected_model(canonical);
  bind_coordinate(expected_model.data, sets);
  consumer.render(expected_model.data);
  const auto expected = read();
  check(consumer.triangles == 2 && delta(blank, expected) > 20,
        "Independent source consumer renders both separated nonempty triangles");
  prepared_normal_consumer::Model wrong_model(canonical);
  bind_coordinate(wrong_model.data, sets, true);
  consumer.render(wrong_model.data);
  check(delta(expected, read()) > 12, "Wrong named coordinate is visibly distinguishable");
  prepared_normal_consumer::Model wrong_frame(canonical);
  bind_coordinate(wrong_frame.data, sets, false, true);
  consumer.render(wrong_frame.data);
  check(delta(expected, read()) > 8, "Wrong authored tangent basis is visibly distinguishable");
  ImportAuthoringOptions options;
  options.paint_resolution = 512;
  options.prepare_texture_atlas = true;
  auto prepared = prepare_authoring_import(Scene{}, input, options);
  check(prepared.objects.size() == 2, "Actual native explicit-atlas importer adopts both instances");
  Document document;
  document.replace(prepared.scene);
  check(document.wait_mesh() && document.frame().state->error.empty(), "Actual native named-UV snapshot publishes");
  const auto before = document.copy_scene().encode();
  const auto actual = draw(native, document.frame().surface);
  const auto native_error = delta(expected, actual);
  std::cout << "NAMED_UV_NATIVE_MAX_ERROR " << native_error << '\n';
  check(native_error <= 4, "Native named-UV explicit-atlas view matches independent original source");
  document.shading(false);
  check(document.wait_mesh(), "Current Core PBR command publishes named-UV material endpoint");
  const auto pbr_error = delta(expected, draw(native, document.frame().surface));
  std::cout << "NAMED_UV_PBR_MAX_ERROR " << pbr_error << '\n';
  check(pbr_error <= 4, "Current Core PBR named-UV view matches independent original source");
  document.authored_shading();
  check(document.wait_mesh() && document.copy_scene().encode() == before && !document.frame().state->can_undo,
        "View restoration preserves exact imported originals, maps and history");
  const auto project = out / "native.dsculpt";
  document.save_async(project).get();
  Document restarted;
  restarted.replace(Scene::load(project));
  check(restarted.wait_mesh() && restarted.copy_scene().encode() == before &&
        delta(expected, draw(native, restarted.frame().surface)) <= 4,
        "Native Save and independent restart preserve named bindings and pixels");
  const auto exported = out / "export.glb";
  document.export_asset(exported, AssetExportFormat::Glb).get();
  prepared_normal_consumer::Model exported_model(exported);
  fbo.bind();
  consumer.render(exported_model.data);
  const auto export_error = delta(expected, read());
  std::cout << "NAMED_UV_EXPORT_MAX_ERROR " << export_error << '\n';
  check(export_error <= 4 && document.copy_scene().encode() == before,
        "Actual GLB export independently preserves named-UV images and normal basis");
  std::cout << checks << " named-UV/native/consumer/history checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
