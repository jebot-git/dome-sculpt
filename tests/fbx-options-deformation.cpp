// SPDX-License-Identifier: GPL-3.0-or-later
#define main default_fbx_deformation_suite
#include "fbx-deformation.cpp"
#undef main

namespace {
SourceScene reference(const std::filesystem::path &path, ImportAxes axes) {
  ufbx_load_opts load{};
  load.strict = true;
  load.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
  load.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
  if (axes == ImportAxes::FromFile) load.target_axes = ufbx_axes_right_handed_y_up;
  // Keep source world distances in the file's own units. The independent oracle
  // converts the fully evaluated world positions afterwards, rather than using
  // the production adapter's scaled geometry/IBMs/local translations.
  load.target_unit_meters = 0;
  load.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
  load.evaluate_skinning = true;
  SourceScene scene(ufbx_load_file(path.c_str(), &load, nullptr), ufbx_free_scene);
  check(bool(scene), "Independent native-unit evaluator loads genuine FBX input");
  return scene;
}
double metres(const ImportOptions &options, double authored) {
  switch (options.units) {
    case ImportUnits::FromFile: return authored;
    case ImportUnits::Metres: return 1;
    case ImportUnits::Centimetres: return .01;
    case ImportUnits::Millimetres: return .001;
    case ImportUnits::Custom: return options.custom_metres_per_unit;
  }
  throw std::runtime_error("Unknown tested unit choice");
}
void compare_options(const ImportedAuthoring &imported, const ufbx_scene &source,
                     const ImportOptions &options) {
  double maximum = 0, magnitude = 0;
  const double scale = metres(options, source.settings.unit_meters);
  for (auto id : imported.objects) {
    const auto &appearance = *imported.scene.object(id).appearance;
    check(appearance.source_receipt && appearance.source_receipt->options == options,
          "Every native object retains the exact actual unit/axis choice");
    const auto &primitive = appearance.asset->primitives.at(appearance.source_primitive);
    const auto corners = nlohmann::json::parse(primitive.extras_json).at("dome_source_corners");
    const auto *node = source.nodes[size_t(appearance.source_node)];
    check(node->mesh, "Source node identity survives combined unit/axis import");
    const auto &mesh = *node->mesh;
    const auto &positions = mesh.skinned_position.exists ? mesh.skinned_position : mesh.vertex_position;
    const auto actual = native_world(imported.scene, id);
    for (size_t v = 0; v < actual.size(); ++v) {
      auto p = ufbx_get_vertex_vec3(&positions, corners.at(appearance.source_vertices.at(v)).get<uint32_t>());
      if (mesh.skinned_is_local) p = ufbx_transform_position(&node->geometry_to_world, p);
      double expected[3] = {p.x * scale, p.y * scale, p.z * scale};
      // An explicit input Z-up interpretation maps +Z to native +Y and +Y to
      // native -Z. It intentionally ignores authored axes, like the UI choice.
      if (options.axes == ImportAxes::ZUp) {
        expected[1] = p.z * scale;
        expected[2] = -p.y * scale;
      }
      const double observed[3] = {actual[v].x, actual[v].y, actual[v].z};
      for (size_t k = 0; k < 3; ++k) {
        maximum = std::max(maximum, std::abs(observed[k] - expected[k]));
        magnitude = std::max(magnitude, std::abs(expected[k]));
      }
      ++comparisons;
    }
  }
  std::cout << "OPTIONS_MAX_ERROR " << maximum << " magnitude " << magnitude << '\n';
  check(maximum <= 2e-5 + 1e-6 * magnitude,
        "Independent raw-unit evaluated skin/morph world agrees with selected native units and axes");
}
}

int main(int argc, char **argv) try {
  check(argc == 2, "Provide a private output directory");
  const auto out = std::filesystem::absolute(argv[1]);
  std::filesystem::create_directories(out);
  const std::filesystem::path inputs = DOME_FBX_FIXTURE_DIRECTORY;
  const std::array<const char *, 5> files{
      "maya_game_sausage_7500_ascii.fbx", "maya_game_sausage_7500_ascii_combined.fbx",
      "blender_293_half_skinned_7400_binary.fbx", "maya_blend_shape_cube_7700_ascii.fbx",
      "blender_279_shape_weights_7400_binary.fbx"};
  size_t rows = 0;
  for (const auto *file : files) for (auto axes : {ImportAxes::FromFile, ImportAxes::YUp, ImportAxes::ZUp}) {
    auto raw = reference(inputs / file, axes);
    for (auto units : {ImportUnits::FromFile, ImportUnits::Metres, ImportUnits::Centimetres,
                       ImportUnits::Millimetres, ImportUnits::Custom}) {
      ImportAuthoringOptions options;
      options.paint_resolution = 64;
      options.choices.axes = axes;
      options.choices.units = units;
      options.choices.custom_metres_per_unit = .007;
      std::cout << "OPTIONS_CASE " << file << " axes=" << int(axes) << " units=" << int(units) << std::endl;
      auto imported = prepare_authoring_import(Scene{}, inputs / file, options);
      compare_options(imported, *raw, options.choices);
      const auto before = imported.scene.encode();
      const auto tag = std::string(file) + "-a" + std::to_string(int(axes)) + "-u" + std::to_string(int(units));
      restart(imported.scene, out / (tag + ".dsculpt"));
      compare_export(imported.scene, imported.objects, out / (tag + ".glb"));
      check(imported.scene.encode() == before, "Options/persistence/export leave complete source endpoint exact");
      ++rows;
    }
  }
  check(rows == 75, "All five genuine inputs cross every actual unit/axis choice");
  std::cout << "PASS " << rows << " actual FBX option rows; " << checks << " checks; "
            << comparisons << " independently evaluated source corners\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
