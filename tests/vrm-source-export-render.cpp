// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/import-authoring.hpp"
#include "dome/vrm.hpp"
#include "vrm-role-consumer.hpp"
#include "vrm-source-fixtures.hpp"
#include <unistd.h>
int main(int argc, char **argv) try {
  if (argc < 4)
    return 2;
  auto home = std::filesystem::temp_directory_path() /
              ("dome-vrm-export-render-" + std::to_string(getpid()));
  std::filesystem::create_directories(home);
  setenv("HOME", home.c_str(), 1);
  setenv("XDG_STATE_HOME", home.c_str(), 1);
  setenv("XDG_CONFIG_HOME", home.c_str(), 1);
  struct Cleanup {
    std::filesystem::path p;
    ~Cleanup() {
      std::error_code ec;
      std::filesystem::remove_all(p, ec);
    }
  } cleanup{home};
  std::filesystem::create_directories(argv[3]);
  NativeGraphics graphics;
  const std::string device =
      reinterpret_cast<const char *>(glGetString(GL_RENDERER));
  std::cout << "GL_VENDOR " << glGetString(GL_VENDOR) << "\nGL_RENDERER "
            << device << std::endl;
  check(argc < 5 || std::string_view(argv[4]) != "--hardware" ||
            (device.find("Intel") != std::string::npos &&
             device.find("A770") != std::string::npos),
        "Graphics context satisfies explicit Arc A770 request when enabled");
  Fbo framebuffer;
  framebuffer.bind();
  SurfaceRenderer renderer;
  role_consumer::Renderer independent;
  for (int model = 1; model <= 2; ++model)
    for (int mode : {0, 1, 2}) {
      auto source = std::filesystem::path(argv[3]) /
                    ("input-" + std::to_string(model) + "-" +
                     std::to_string(mode) + ".vrm");
      vrm_source_fixture::source_variant(argv[model], source, mode);
      ImportAuthoringOptions options;
      options.paint_resolution = 64;
      auto prepared = prepare_authoring_import(Scene{}, source, options);
      for (float weight : {0.f, .65f}) {
        auto scene = prepared.scene;
        for (auto id : prepared.objects) {
          auto d = *scene.object(id).deformation;
          d.pose.clear();
          for (auto &bone : d.skeleton->bones)
            d.pose.push_back(bone.rest);
          if (weight > 0)
            d.pose[d.humanoid->joints.at("hips")].scale = {1.1f, .9f, 1.2f};
          if (d.morphs) {
            d.morph_weights.assign(d.morphs->targets.size(), 0);
            if (!d.morph_weights.empty())
              d.morph_weights[0] = weight;
          }
          scene.set_deformation(id, std::move(d));
        }
        Document document;
        document.replace(scene);
        check(document.wait_mesh(std::chrono::seconds(30)) &&
                  document.frame().state->error.empty(),
              "Actual original-space source avatar reaches native renderer");
        auto path =
            std::filesystem::path(argv[3]) /
            ("output-" + std::to_string(model) + "-" + std::to_string(mode) +
             "-" + std::to_string(weight) + ".vrm");
        export_vrm(scene.preview_surfaces(), path);
        role_consumer::Model file(path);
        auto native = draw(renderer, document.frame().surface);
        framebuffer.bind();
        independent.render(file.data, &file.json["extensions"]["VRM"]);
        int error = delta(native, read());
        std::cout << "VRM_EXPORT_PIXEL model=" << model << " mode=" << mode
                  << " morph=" << weight << " maxError=" << error << std::endl;
        check(error <= 2,
              "Emitted original-space skin/morph/MToon under "
              "ordinary/reflected-mesh/sheared-source chains matches actual "
              "native pixels at unchanged tolerance2");
        size_t visible = 0;
        for (size_t p = 3; p < native.size(); p += 4)
          visible += native[p] != 0;
        check(visible > 100, "Independent emitted avatar comparison has "
                             "visible exterior geometry");
      }
    }
  std::cout
      << checks
      << " native original-space VRM independent graphics checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
