// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/import-authoring.hpp"
#include "source-color-consumer.hpp"
#include "source-color-fixtures.hpp"
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
  auto original = source_color_fixture::bytes();
  auto length = uint32_t(original[12]) | uint32_t(original[13]) << 8 |
                uint32_t(original[14]) << 16 | uint32_t(original[15]) << 24;
  auto source = nlohmann::json::parse(original.begin() + 20,
                                      original.begin() + 20 + length);
  std::vector<uint8_t> binary(original.begin() + 28 + length, original.end());
  auto word = [](std::vector<uint8_t> &b, uint32_t x) {
    for (int k = 0; k < 4; ++k)
      b.push_back(uint8_t(x >> (8 * k)));
  };
  for (const auto *alpha : {"OPAQUE", "MASK", "BLEND"})
    for (float weight : {0.f, .65f}) {
      auto json = source;
      json["materials"][0]["alphaMode"] = alpha;
      json["materials"][0]["alphaCutoff"] = .35f;
      json["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"][3] = .7f;
      json["nodes"][0]["weights"] = {weight};
      auto text = json.dump();
      while (text.size() % 4)
        text += ' ';
      std::vector<uint8_t> bytes;
      for (auto x :
           {0x46546c67u, 2u, uint32_t(28 + text.size() + binary.size()),
            uint32_t(text.size()), 0x4e4f534au})
        word(bytes, x);
      bytes.insert(bytes.end(), text.begin(), text.end());
      word(bytes, uint32_t(binary.size()));
      word(bytes, 0x004e4942);
      bytes.insert(bytes.end(), binary.begin(), binary.end());
      auto path = directory /
                  (std::string(alpha) + "-" + std::to_string(weight) + ".glb");
      {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
      }
      ImportAuthoringOptions options;
      options.paint_resolution = 64;
      auto prepared = prepare_authoring_import(Scene{}, path, options);
      Document document;
      document.replace(prepared.scene);
      check(document.wait_mesh() && document.frame().state->error.empty(),
            "Actual imported source reaches immutable native renderer without "
            "a manual adoption adapter");
      auto actual = draw(native, document.frame().surface);
      color_consumer::Model model(path);
      fbo.bind();
      independent.render(model.data);
      auto expected = read();
      int error = delta(actual, expected);
      std::cout << "PIXEL " << alpha << " morph=" << weight
                << " maxError=" << error << std::endl;
      check(error <= 2, "Imported original PBR/normal/linearRGBA/alpha with "
                        "POSITION-NORMAL-TANGENT morphs matches independently "
                        "parsed original GLB pixels");
    }
  std::cout << checks << " imported source pixel checks passed" << std::endl;
} catch (const std::exception &e) {
  std::cerr << e.what() << std::endl;
  return 1;
}
