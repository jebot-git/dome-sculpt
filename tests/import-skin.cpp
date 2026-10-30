// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/humanoid-creator.hpp"
#include "import-skin-fixtures.hpp"
#include <iostream>
#include <unistd.h>
using namespace dome;
using namespace import_fixture;
namespace {
unsigned checks = 0;
void check(bool b, const char *s) {
  if (!b)
    throw std::runtime_error(s);
  ++checks;
  std::cout << "PASS " << s << std::endl;
}
} // namespace

int main(int argc, char **argv) try {
  if (argc == 3 && std::string(argv[1]) == "--ui-fixture") {
    skinned(true).save(argv[2]);
    return 0;
  }
  if (argc == 3 && std::string(argv[1]) == "--empty-project") {
    Scene{}.save(argv[2]);
    return 0;
  }
  if (argc == 3 && std::string(argv[1]) == "--inspect") {
    std::cout << Scene::load(argv[2]).encode().dump() << std::endl;
    return 0;
  }

  auto directory = std::filesystem::temp_directory_path() /
                   ("dome-import-skin-" + std::to_string(getpid()));
  std::filesystem::create_directory(directory);
  struct Cleanup {
    std::filesystem::path p;
    ~Cleanup() {
      std::error_code e;
      std::filesystem::remove_all(p, e);
    }
  } cleanup{directory};
  ImportAuthoringOptions options;
  options.paint_resolution = 64;
  for (bool authored : {false, true}) {
    auto f = skinned(authored);
    auto path = directory / (authored ? "authored.glb" : "implicit.glb");
    f.save(path);
    auto prepared = prepare_authoring_import(Scene{}, path, options);
    for (auto id : prepared.objects) {
      auto &o = prepared.scene.object(id);
      const auto &asset = *o.appearance->asset;
      const auto &p = asset.primitives[0];
      const auto &skin = asset.skins[0];
      auto local = o.deformation->preview(o.painted->mesh());
      auto world = prepared.scene.world_transform(id);
      for (size_t v = 0; v < local.positions.size(); ++v) {
        auto source = o.appearance->source_vertices[v];
        auto P =
            p.mesh.positions[source] + p.morph_targets[0].positions[source] *
                                           o.deformation->morph_weights[0];
        Vec3 expected{};
        for (const auto &set : p.influences)
          for (size_t k = 0; k < 4; ++k)
            if (set.weights[source][k] != 0) {
              auto slot = set.joints[source][k];
              auto q = authored
                           ? transformed(skin.inverse_bind_matrices[slot], P)
                           : P;
              expected += transformed(asset.nodes[skin.joints[slot]].world, q) *
                          set.weights[source][k];
            }
        auto actual =
            ob::transform(world, {local.positions[v].x, local.positions[v].y,
                                  local.positions[v].z});
        if (length(expected - Vec3{actual.x, actual.y, actual.z}) > 2e-6f)
          throw std::runtime_error(
              "Imported skin uses wrong mesh-node transform, joint identity, "
              "IBM or morph order");
      }
      check(o.appearance->source_joint_nodes ==
                    std::vector<uint32_t>({0, 3, 4}) &&
                o.deformation->skeleton->inverse_bind_override->size() == 3,
            "Imported joint slots map to exact ancestor-inclusive native "
            "source-node IDs without invented joints");
      check(o.deformation->pose.empty(),
            "Original current joint transforms and bind matrices evaluate "
            "correctly without an invented nonempty pose");
    }
    check(true, authored ? "Actual native import matches independent "
                           "nonconventional authored-IBM world positions while "
                           "ignoring skinned mesh-node transforms"
                         : "Actual native import uses glTF implicit identity "
                           "inverse binds rather than inverse-rest guesses");
    auto saved =
        directory / (authored ? "authored.dsculpt" : "implicit.dsculpt");
    prepared.scene.save(saved);
    auto restarted = Scene::load(saved);
    check(restarted.encode() == prepared.scene.encode() &&
              restarted.encode()["version"] == 11,
          "Actual imported source/skin/frame data persists under Scene11 "
          "without changing exact raw bytes");
    if (authored)
      check(restarted.active()
                        .appearance->asset->skins[0]
                        .inverse_bind_matrices.size() == 3 &&
                restarted.active()
                        .appearance->asset->skins[0]
                        .inverse_bind_matrices[2][12] == 99,
            "Unused trailing authored inverse-bind accessor matrices remain in "
            "immutable original provenance");
    auto output = directory /
                  (authored ? "emitted-authored.glb" : "emitted-implicit.glb");
    export_glb(prepared.scene.surfaces(), output);
    ImportLimits limits;
    limits.mode = ImportMode::PreserveGltf;
    auto emitted = import_mesh(output, limits);
    check(emitted.skins.size() == 2 && emitted.primitives.size() == 2,
          "Generic GLB export retains both imported skin instances and their "
          "independent current morph defaults");
  }
  std::cout << checks << " actual imported skin checks passed" << std::endl;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
