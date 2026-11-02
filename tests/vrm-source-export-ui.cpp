// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only preparation/inspection for the real FLTK export chooser fixture.
#include "dome/import-authoring.hpp"
#include "cgltf.h"
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>

int main(int argc, char **argv) try {
  if (argc != 3)
    return 2;
  const std::string_view mode = argv[1];
  if (mode == "--ui-fixture") {
    const char *source = std::getenv("DOME_VRM_UI_SOURCE");
    if (!source)
      return 2;
    dome::ImportAuthoringOptions options;
    options.paint_resolution = 64;
    auto prepared = dome::prepare_authoring_import(dome::Scene{}, source, options);
    prepared.scene.save(argv[2]);
    return 0;
  }
  if (mode == "--inspect") {
    std::cout << dome::Scene::load(argv[2]).encode().dump() << '\n';
    return 0;
  }
  if (mode == "--validate-glb") {
    cgltf_options options{};
    cgltf_data *data = nullptr;
    if (cgltf_parse_file(&options, argv[2], &data) != cgltf_result_success)
      return 1;
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> owned(data, cgltf_free);
    if (cgltf_load_buffers(&options, data, argv[2]) != cgltf_result_success ||
        cgltf_validate(data) != cgltf_result_success || data->meshes_count != 19)
      return 1;
    auto json = nlohmann::json::parse(data->json, data->json + data->json_size);
    if (!json.at("extensions").contains("VRM"))
      return 1;
    std::cout << "Valid source-preserving VRM0 nineteen-part output\n";
    return 0;
  }
  return 2;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
