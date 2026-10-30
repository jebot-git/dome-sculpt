// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "import-skin-fixtures.hpp"
#include "source-color-consumer.hpp"
int main(int argc, char **argv) try {
  if (argc < 2)
    return 2;
  const std::filesystem::path directory = argv[1];
  std::filesystem::create_directories(directory);
  NativeGraphics context;
  std::cout << "GPU " << glGetString(GL_RENDERER) << std::endl;
  Fbo fbo;
  fbo.bind();
  SurfaceRenderer native;
  color_consumer::Renderer independent;
  for (bool authored : {false, true})
    for (const auto *alpha : {"OPAQUE", "MASK", "BLEND"})
      for (float weight : {0.f, .65f}) {
        auto fixture = import_fixture::skinned(authored);
        auto &material = fixture.j["materials"][0];
        material.erase("extensions");
        material["alphaMode"] = alpha;
        material["alphaCutoff"] = .3f;
        fixture.j["nodes"][1]["weights"] = {weight};
        fixture.j["nodes"][2]["weights"] = {weight * .5f};
        fixture.j["nodes"][3]["rotation"] = {0, 0, float(std::sin(.15)),
                                             float(std::cos(.15))};
        Image bump{1, 1, {230, 153, 204, 255}};
        auto png = bump.png();
        while (fixture.bin.size() % 4)
          fixture.bin.push_back(0);
        int view = int(fixture.j["bufferViews"].size());
        fixture.j["bufferViews"].push_back({{"buffer", 0},
                                            {"byteOffset", fixture.bin.size()},
                                            {"byteLength", png.size()}});
        fixture.bin.insert(fixture.bin.end(), png.begin(), png.end());
        fixture.j["images"].push_back(
            {{"mimeType", "image/png"}, {"bufferView", view}});
        fixture.j["textures"].push_back({{"source", 1}, {"sampler", 0}});
        material["normalTexture"] = {{"index", 1}, {"scale", .8f}};
        auto path =
            directory / (std::string(authored ? "authored-" : "implicit-") +
                         alpha + "-" + std::to_string(weight) + ".glb");
        fixture.save(path);
        ImportAuthoringOptions options;
        options.paint_resolution = 64;
        auto prepared = prepare_authoring_import(Scene{}, path, options);
        Document document;
        document.replace(prepared.scene);
        check(document.wait_mesh() && document.frame().state->error.empty(),
              "Actual imported source skin reaches current-pose native "
              "renderer using authored/implicit binding at empty native pose");
        auto actual = draw(native, document.frame().surface);
        color_consumer::Model model(path);
        fbo.bind();
        independent.render(model.data);
        auto expected = read();
        int error = delta(actual, expected);
        std::cout << "SKIN_PIXEL authored=" << authored << " alpha=" << alpha
                  << " morph=" << weight << " maxError=" << error << std::endl;
        check(error <= 2, "Independent original-file consumer matches initial "
                          "rotated joint pose, nonconventional/identity bind, "
                          "N/T morph, alpha and ignored mesh-node transforms");
      }
  std::cout << checks << " imported skin pixel checks passed" << std::endl;
} catch (const std::exception &e) {
  std::cerr << e.what() << std::endl;
  return 1;
}
