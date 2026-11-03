// SPDX-License-Identifier: GPL-3.0-or-later
// Independent actual imported primitive/source-node/native-joint support audit.
#include "dome/creation.hpp"
#include "heat-source.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
using namespace dome::detail;
int main(int argc, char **argv) try {
  const bool male = argc > 1 && std::string(argv[1]) == "male";
  Scene scene;
  append_humanoid(scene,
                  male ? humanoid_baseline_defaults("vroid-beta-male")
                       : HumanoidControls{},
                  "Primitive palette authority", {}, 64);
  const auto original = scene.encode();
  size_t parts = 0, missing = 0, wrong = 0, current_mismatch = 0;
  for (const auto &o : scene.objects())
    if (o.painted && o.appearance && o.deformation && o.deformation->skin &&
        o.deformation->skeleton) {
      const auto &a = *o.appearance;
      const auto &rig = *o.deformation->skeleton;
      const auto &asset = *a.asset;
      HeatBindingOptions options;
      auto profile =
          imported_heat_profile(rig, a, options, {}, &*o.deformation->humanoid,
                                ImportedHeatPolicy::OriginalPrimitive);
      if (!parts) {
        auto exact = options;
        exact.maximum_work = profile.work;
        auto bounded =
            imported_heat_profile(rig, a, exact, {}, &*o.deformation->humanoid,
                                  ImportedHeatPolicy::OriginalPrimitive);
        if (bounded.eligible != profile.eligible ||
            bounded.work != profile.work)
          throw std::runtime_error("Exact indexed work budget changed output");
        --exact.maximum_work;
        bool work_refused = false, cancelled = false, invalid_refused = false;
        try {
          imported_heat_profile(rig, a, exact, {}, &*o.deformation->humanoid,
                                ImportedHeatPolicy::OriginalPrimitive);
        } catch (const std::length_error &) {
          work_refused = true;
        }
        try {
          imported_heat_profile(
              rig, a, options, [] { return true; }, &*o.deformation->humanoid,
              ImportedHeatPolicy::OriginalPrimitive);
        } catch (const std::runtime_error &) {
          cancelled = true;
        }
        auto altered = a;
        auto detached = std::make_shared<ImportedAsset>(*a.asset);
        auto global_index = detached->meshes.at(size_t(a.source_mesh))
                                .primitives.at(size_t(a.source_primitive));
        auto &p = detached->primitives.at(global_index);
        p.mesh.indices.at(0) = uint32_t(p.mesh.positions.size());
        altered.asset = detached;
        try {
          imported_heat_profile(rig, altered, options, {},
                                &*o.deformation->humanoid,
                                ImportedHeatPolicy::OriginalPrimitive);
        } catch (const std::invalid_argument &) {
          invalid_refused = true;
        }
        if (!work_refused || !cancelled || !invalid_refused)
          throw std::runtime_error("Indexed budget/cancel/index guard failed");
        std::cout << "CONTROL exact-work/one-short/cancel/invalid-draw PASS\n";
      }
      const ImportedPrimitive *primitive = nullptr;
      uint32_t global = UINT32_MAX;
      for (uint32_t i = 0; i < asset.primitives.size(); ++i)
        if (asset.primitives[i].source_mesh == a.source_mesh &&
            asset.primitives[i].source_primitive == a.source_primitive) {
          if (primitive)
            throw std::runtime_error(
                "Independent primitive identity ambiguous");
          primitive = &asset.primitives[i];
          global = i;
        }
      if (!primitive ||
          asset.meshes.at(size_t(a.source_mesh))
                  .primitives.at(size_t(a.source_primitive)) != global ||
          asset.nodes.at(size_t(a.source_node)).mesh != a.source_mesh)
        throw std::runtime_error("Canonical primitive identity differs from "
                                 "production palette lookup");
      const auto &skin =
          asset.skins.at(size_t(asset.nodes.at(size_t(a.source_node)).skin));
      std::map<uint32_t, uint32_t> native;
      for (uint32_t i = 0; i < a.source_joint_nodes.size(); ++i)
        if (!native.emplace(a.source_joint_nodes[i], i).second)
          throw std::runtime_error(
              "Independent source-node/native-joint map ambiguous");
      std::vector<uint8_t> expected(rig.bones.size()),
          current(rig.bones.size());
      for (const auto &set : primitive->influences)
        for (auto v : primitive->mesh.indices)
          for (int k = 0; k < 4; ++k)
            if (set.weights.at(v)[k] > 0) {
              auto node = skin.joints.at(set.joints.at(v)[k]);
              auto j = native.at(node);
              if (rig.bones[j].deform)
                expected[j] = 1;
            }
      for (const auto &row : o.deformation->skin->vertices)
        for (auto value : row)
          if (value.weight > 0 && rig.bones[value.joint].deform)
            current[value.joint] = 1;
      for (size_t i = 0; i < current.size(); ++i) {
        wrong += expected[i] != profile.eligible[i];
        current_mismatch += expected[i] != current[i];
        if (current[i] && !profile.eligible[i]) {
          ++missing;
          std::cout << "MISSING current_joint=" << i
                    << " source_node=" << a.source_joint_nodes[i] << '\n';
        }
      }
      std::cout << "PART " << o.id << " mesh=" << a.source_mesh
                << " local_primitive=" << a.source_primitive
                << " global_primitive=" << global
                << " drawn_corners=" << primitive->mesh.indices.size()
                << " accessor_rows=" << primitive->mesh.positions.size()
                << " palette=";
      for (size_t i = 0; i < expected.size(); ++i)
        if (expected[i])
          std::cout << i << ',';
      std::cout << " name=" << o.name << '\n';
      ++parts;
    }
  bool exact = scene.encode() == original;
  std::cout << "SUMMARY parts=" << parts << " direct_palette_mismatch=" << wrong
            << " current_support_mismatch=" << current_mismatch
            << " excluded_current_artist_joints=" << missing
            << " source_scene_exact=" << exact << '\n';
  if (wrong || current_mismatch || missing || !exact)
    return 1;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
