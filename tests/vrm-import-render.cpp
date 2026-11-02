// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_material_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/import-authoring.hpp"
#include "vrm-role-consumer.hpp"
#include <fstream>
#include <unistd.h>
namespace {
uint32_t word(std::span<const uint8_t> b, size_t at) {
  return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 |
         uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}
void source_variant(const std::filesystem::path &input,
                    const std::filesystem::path &output) {
  std::ifstream file(input, std::ios::binary);
  std::vector<uint8_t> raw{std::istreambuf_iterator<char>(file), {}};
  auto j =
      nlohmann::json::parse(raw.begin() + 20, raw.begin() + 20 + word(raw, 12));
  auto begin = 20 + word(raw, 12);
  std::vector<uint8_t> tail(raw.begin() + begin, raw.end());
  // A new explicit source fixture, not a runtime normalization. Preserve
  // original geometry/images/skins/morphs while choosing the independently
  // implemented shader subset and a full-model orthographic placement.
  for (auto &property : j["extensions"]["VRM"]["materialProperties"]) {
    auto m = MToonMaterial::from_vrm0(property);
    for (size_t r = 2; r < m.textures.size(); ++r)
      m.textures[r].index = -1;
    m.parameters.base_color = {.75f, .6f, .4f, 1};
    m.parameters.shade_color = {.18f, .12f, .08f};
    m.parameters.toony = .5f;
    m.parameters.shift = -.2f;
    m.parameters.normal_enabled = false;
    m.parameters.normal_scale = 0;
    m.parameters.indirect_intensity = 0;
    m.parameters.light_color_attenuation = 0;
    m.parameters.emission = {};
    m.parameters.rim_color = {};
    m.parameters.outline = MToonOutline::None;
    m.parameters.scroll = {};
    m.parameters.rotation_speed = 0;
    m.parameters.cull = MToonCull::Back;
    m.parameters.alpha = MToonAlpha::Opaque;
    m.parameters.depth_write = true;
    m.parameters.source_blend = 1;
    m.parameters.destination_blend = 0;
    m.parameters.render_queue = 2000;
    property = m.source_json();
  }
  for (auto &m : j["materials"]) {
    m["alphaMode"] = "OPAQUE";
    m["doubleSided"] = false;
  }
  auto root = j["nodes"].size();
  j["nodes"].push_back({{"name", "Independent original model placement"},
                        {"children", j["scenes"][0]["nodes"]},
                        {"scale", {.8f, .8f, .8f}},
                        {"translation", {0, -.7f, 0}}});
  j["scenes"][0]["nodes"] = {root};
  auto text = j.dump();
  while (text.size() % 4)
    text += ' ';
  std::vector<uint8_t> bytes;
  auto put = [&](uint32_t x) {
    for (int k = 0; k < 4; ++k)
      bytes.push_back(uint8_t(x >> (8 * k)));
  };
  put(0x46546c67);
  put(2);
  put(uint32_t(20 + text.size() + tail.size()));
  put(uint32_t(text.size()));
  put(0x4e4f534a);
  bytes.insert(bytes.end(), text.begin(), text.end());
  bytes.insert(bytes.end(), tail.begin(), tail.end());
  std::ofstream result(output, std::ios::binary);
  result.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}
} // namespace
int main(int argc, char **argv) try {
  if (argc < 3)
    return 2;
  auto home = std::filesystem::temp_directory_path() /
              ("dome-vrm-render-" + std::to_string(getpid()));
  std::filesystem::create_directories(home);
  setenv("HOME", home.c_str(), 1);
  setenv("XDG_STATE_HOME", home.c_str(), 1);
  setenv("XDG_CONFIG_HOME", home.c_str(), 1);
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code ec;
      std::filesystem::remove_all(path, ec);
    }
  } cleanup{home};
  NativeGraphics graphics;
  std::string device = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
  std::cout << "GL_VENDOR " << glGetString(GL_VENDOR) << "\nGL_RENDERER "
            << device << std::endl;
  check(argc < 4 || std::string_view(argv[3]) != "--hardware" ||
            device.find("Intel") != std::string::npos &&
                device.find("A770") != std::string::npos,
        "Graphics context meets the explicit hardware request when enabled");
  Fbo framebuffer;
  framebuffer.bind();
  SurfaceRenderer native;
  role_consumer::Renderer consumer;
  for (int model = 1; model <= 2; ++model) {
    auto path = home / (std::to_string(model) + ".vrm");
    source_variant(argv[model], path);
    ImportAuthoringOptions options;
    options.paint_resolution = 64;
    auto prepared = prepare_authoring_import(Scene{}, path, options);
    for (float weight : {0.f, .35f, .8f}) {
      auto scene = prepared.scene;
      role_consumer::Model independent(path);
      for (auto id : prepared.objects) {
        auto &o = scene.object(id);
        auto d = std::make_shared<DeformationState>(*o.deformation);
        d->pose.clear();
        for (const auto &b : d->skeleton->bones)
          d->pose.push_back(b.rest);
        auto joint = d->humanoid->joints.at("hips");
        if (weight > 0) {
          d->pose[joint].scale = {1.1f, .9f, 1.2f};
          auto source = o.appearance->source_joint_nodes[joint];
          independent.data->nodes[source].has_scale = 1;
          auto &scale = independent.data->nodes[source].scale;
          scale[0] = 1.1f;
          scale[1] = .9f;
          scale[2] = 1.2f;
        }
        if (d->morphs) {
          d->morph_weights.assign(d->morphs->targets.size(), 0);
          if (!d->morph_weights.empty())
            d->morph_weights[0] = weight;
          auto &node =
              independent.data->nodes[size_t(o.appearance->source_node)];
          // Both original CC0 files omit node AND mesh default-weight arrays.
          // Explicit native authoring supplies a current pose, so give the
          // independent consumer its own owned current node weights too.
          if (!node.weights_count) {
            node.weights = static_cast<cgltf_float *>(
                std::calloc(d->morphs->targets.size(), sizeof(cgltf_float)));
            if (!node.weights)
              throw std::bad_alloc();
            node.weights_count = d->morphs->targets.size();
          }

          for (size_t t = 0; t < node.mesh->weights_count; ++t)
            node.mesh->weights[t] = t == 0 ? weight : 0;
          for (size_t t = 0; t < node.weights_count; ++t)
            node.weights[t] = t == 0 ? weight : 0;
        }
        scene.set_deformation(id, *d);
      }
      Document doc;
      doc.replace(scene);
      check(doc.wait_mesh(std::chrono::seconds(30)) &&
                doc.frame().state->error.empty(),
            "Actual imported VRM reaches immutable native renderer with "
            "complete source skin/current morph");
      auto snapshot = doc.frame().surface;
      auto actual = draw(native, snapshot);
      framebuffer.bind();
      consumer.render(independent.data, &independent.json["extensions"]["VRM"]);
      auto expected = read();
      float position_error = 0;
      size_t compared_corners = 0;
      for (const auto &range : snapshot->ranges) {
        const auto &object = scene.object(range.object);
        const auto &a = *object.appearance;
        const auto &mesh = independent.data->meshes[size_t(a.source_mesh)];
        size_t offset = 0;
        for (int primitive = 0; primitive < a.source_primitive; ++primitive)
          offset +=
              role_consumer::attr(mesh.primitives[primitive].attributes,
                                  mesh.primitives[primitive].attributes_count,
                                  cgltf_attribute_type_position)
                  ->count;
        const auto &primitive = mesh.primitives[size_t(a.source_primitive)];
        for (size_t corner = 0; corner < range.count; ++corner) {
          auto source = cgltf_accessor_read_index(primitive.indices, corner);
          auto actual_vertex = snapshot->mesh.indices[range.first + corner];
          position_error =
              std::max(position_error,
                       length(snapshot->mesh.positions[actual_vertex] -
                              consumer.world_positions.at(size_t(a.source_mesh))
                                  .at(offset + source)));
          ++compared_corners;
        }
      }
      std::cout << "VRM_WORLD model=" << model << " morph=" << weight
                << " maxError=" << position_error
                << " corners=" << compared_corners << std::endl;
      check(compared_corners > 10000 && position_error < 2e-6f,
            "Every actual imported corner matches independently parsed "
            "original skin/IBM/morph world position");

      int error = delta(actual, expected);
      std::cout << "VRM_IMPORT_PIXEL model=" << model << " morph=" << weight
                << " maxError=" << error << std::endl;
      check(error <= 2,
            "External VRM original-source skin/morph/typed main-shade pixels "
            "match independent cgltf+GLSL at unchanged tolerance2");
      size_t visible = 0;
      for (size_t p = 3; p < actual.size(); p += 4)
        visible += actual[p] != 0;
      check(
          visible > 100,
          "Model consumer comparison includes visible exterior avatar pixels");
      framebuffer.bind();
      consumer.render(independent.data);
      check(delta(actual, read()) > 16,
            "Independent PBR substitution changes visible source pixels and "
            "cannot satisfy typed VRM oracle");
    }
  }
  std::cout << checks << " actual generic VRM native graphics checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
