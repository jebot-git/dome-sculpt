// SPDX-License-Identifier: GPL-3.0-or-later
// Native GUI preparation/inspection only. No production UI or file hooks.
#include "cgltf.h"
#include "dome/import-authoring.hpp"
#include "dome/humanoid-creator.hpp"
#include "vrm1-metadata.hpp"
#include <cstdlib>
#include <iostream>
int main(int argc, char **argv) try {
  if (argc != 3)
    return 2;
  const std::string_view mode = argv[1];
  if (mode == "--ui-fixture") {
    auto source = std::getenv("DOME_VRM_UI_SOURCE");
    if (!source)
      return 2;
    dome::ImportAuthoringOptions options;
    options.paint_resolution = 64;
    auto prepared =
        dome::prepare_authoring_import(dome::Scene{}, source, options);
    for (auto id : prepared.objects)
      if (prepared.scene.object(id).appearance->asset->avatar_version !=
          dome::ImportedAvatarVersion::Vrm1)
        throw std::runtime_error("GUI source must be validated actual VRM1");
    prepared.scene.save(argv[2]);
    return 0;
  }
  if (mode == "--inspect") {
    auto scene = dome::Scene::load(argv[2]);
    bool actual = false;
    for (auto &o : scene.objects())
      if (o.appearance) {
        if (o.appearance->asset->avatar_version !=
            dome::ImportedAvatarVersion::Vrm1)
          throw std::runtime_error(
              "Native GUI replay lost derived VRM1 identity");
        actual = true;
      }
    if (!actual)
      throw std::runtime_error("Native GUI source has no actual VRM1 owner");
    std::cout << scene.encode().dump() << '\n';
    return 0;
  }
  if (mode == "--validate-glb") {
    cgltf_options options{};
    cgltf_data *data = nullptr;
    if (cgltf_parse_file(&options, argv[2], &data) != cgltf_result_success)
      return 1;
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> owned(data, cgltf_free);
    if (cgltf_load_buffers(&options, data, argv[2]) != cgltf_result_success ||
        cgltf_validate(data) != cgltf_result_success || data->meshes_count != 3)
      return 1;
    auto json = nlohmann::json::parse(data->json, data->json + data->json_size);
    dome::detail::validate_vrm1_metadata(json, {}, true);
    if (json.at("extensions").contains("VRM"))
      return 1;
    std::cout << "Valid actual VRM1 source-node 3-part output\n";
    return 0;
  }
  return 2;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
