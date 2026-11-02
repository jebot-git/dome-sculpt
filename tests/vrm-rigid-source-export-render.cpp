// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/import-authoring.hpp"
#include "dome/vrm.hpp"
#include "vrm-role-consumer.hpp"
#include "vrm-rigid-fixtures.hpp"
#include "dome/rig-edit.hpp"
#include "dome/source-placement.hpp"
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
      auto fitted = home / "placed.vrm";
      vrm_source_fixture::source_variant(argv[model], fitted, mode);
      auto rigid_nodes = vrm_source_fixture::with_rigid_accessories(fitted,source);
      ImportAuthoringOptions options;
      options.paint_resolution = 64;
      auto prepared = prepare_authoring_import(Scene{}, source, options);
      for (float weight : {0.f, .65f}) {
        auto scene = prepared.scene;
        if(weight>0) {
          auto active=prepared.objects.front();
          RigEdit edit;edit.joint=JointAdjustment{
            scene.object(active).deformation->humanoid->joints.at("head"),{0,0,17},{}};
          edit_rig(scene,active,edit);
        }
        uint64_t attached=0;
        for(auto id:prepared.objects) {
          const auto &o=scene.object(id);
          if(o.appearance->source_node==int(rigid_nodes.moving))attached=id;
          if(o.deformation && o.deformation->morphs) {
            auto d=*o.deformation;
            d.morph_weights.assign(d.morphs->targets.size(),0);
            d.morph_weights[0]=weight;
            scene.set_deformation(id,std::move(d));
          }
        }
        if(mode==2) {
          auto &o=scene.object(attached);
          auto previous=o.affine_edit?o.affine_edit->matrix():AffineTransform{};
          AffineTransform edit(std::array<double,16>{-1.,.12,0,0,.25,1.1,0,0,0,0,.85,0,.04,-.02,.01,1});
          o.affine_edit=std::make_shared<const NativeAffineEdit>(previous*edit);
        }
        Document document;
        document.replace(scene);
        check(document.wait_mesh(std::chrono::seconds(30)) &&
                  document.frame().state->error.empty(),
              "Actual mixed source skins and rigid accessories reach native renderer");
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
        std::cout << "VRM_RIGID_EXPORT_PIXEL model=" << model << " mode=" << mode
                  << " morph=" << weight << " maxError=" << error << std::endl;
        check(error <= 2,
              "Emitted original-space skin/morph/MToon under "
              "ordinary/reflected-mesh/sheared-source chains with rigid affine edits matches actual "
              "native pixels at unchanged tolerance2");
        auto without_rigid=scene;
        for(auto id:prepared.objects)
          if(!without_rigid.object(id).deformation || !without_rigid.object(id).deformation->skin)
            without_rigid.object(id).visible=false;
        Document body_only;body_only.replace(without_rigid);
        check(body_only.wait_mesh(std::chrono::seconds(30)) &&
                  delta(native,draw(renderer,body_only.frame().surface))>5,
              "New rigid source accessories independently contribute visible native pixels");
        size_t visible = 0;
        for (size_t p = 3; p < native.size(); p += 4)
          visible += native[p] != 0;
        check(visible > 100, "Independent emitted avatar comparison has "
                             "visible exterior geometry");
      }
    }
  std::cout
      << checks
      << " native mixed rigid/source VRM independent graphics checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
