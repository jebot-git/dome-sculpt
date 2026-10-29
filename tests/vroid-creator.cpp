// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/humanoid-sha256.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/projection.hpp"
#include "dome/source-maps.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <unistd.h>
using namespace dome;
void require(bool v, const char *s) {
  if (!v)
    throw std::runtime_error(s);
  std::cout << "PASS " << s << '\n';
}
void rejects(auto f, const char *s) {
  bool failed = false;
  try {
    f();
  } catch (const std::exception &e) {
    failed = true;
    std::cout << "EXPECTED " << e.what() << '\n';
  }
  require(failed, s);
}
int main() try {
  std::cout << std::unitbuf;
  std::vector<uint8_t> empty, abc{'a', 'b', 'c'};
  require(
      creator_detail::sha256(empty) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e"
                                       "4649b934ca495991b7852b855" &&
          creator_detail::sha256(abc) == "ba7816bf8f01cfea414140de5dae2223b0036"
                                         "1a396177a9cb410ff61f20015ad",
      "Native licensed-asset fingerprint passes NIST-compatible known digests");
  auto controls = humanoid_baseline_defaults("vroid-beta-female");
  auto avatar = create_humanoid(controls);
  avatar.validate();
  require(
      controls.anime == 0 && controls.eye_size == 0 &&
          controls.skin_color == std::array<float, 4>{1, 1, 1, 1},
      "New VRoid defaults use neutral relative shape and white tint controls");
  require(avatar.parts.size() == 19 &&
              avatar.primary_part < avatar.parts.size(),
          "Actual CC0 VRoid primitive/material parts and primary body identity "
          "are preserved");
  const auto &asset = *avatar.parts.front().appearance->asset;
  require(
      avatar.parts[0].appearance->original_bytes &&
          creator_detail::sha256(*avatar.parts[0].appearance->original_bytes) ==
              "1995f174f6c8f2211f3c56c9e5fe4f9daf27ee5e648f129479582a259238149"
              "b",
      "Exact verified original VRM bytes remain owned for source-independent "
      "native archives");
  require(asset.materials.size() == 16 && asset.images.size() == 25 &&
              asset.skins[0].skeleton == 12 &&
              asset.skins[0].skeleton_hint_advisory,
          "Source retains actual15materials/25images and explicitly diagnosed "
          "legacy Face skeleton hint");
  require(avatar.parts.front().deformation.skeleton->bones.size() == 83,
          "Female original83-joint normalized hierarchy is retained");
  float floor = 1000, ceiling = -1000;
  for (const auto &p : asset.primitives)
    for (auto i : p.mesh.indices) {
      floor = std::min(floor, p.mesh.positions[i].y);
      ceiling = std::max(ceiling, p.mesh.positions[i].y);
    }
  float scale = controls.height / (ceiling - floor);
  size_t vertices = 0, morphs = 0;
  bool alpha = false, normal = false, authored_normal_morph = false;
  for (const auto &part : avatar.parts) {
    auto source = part.appearance;
    require(source && source->asset.get() == &asset &&
                source->original_bytes ==
                    avatar.parts[0].appearance->original_bytes &&
                source->decoded_images ==
                    avatar.parts.front().appearance->decoded_images,
            "Parts share original encoded asset and one immutable decoded "
            "image cache");
    require(source->source_basis_quaternion &&
                source->source_basis_quaternion->w == 0 &&
                source->source_basis_quaternion->x == 0 &&
                source->source_basis_quaternion->y == 1 &&
                source->source_basis_quaternion->z == 0,
            "Explicit authored basis matches proper Y180 source conversion");
    require(source->source_joint_nodes.size() ==
                    part.deformation.skeleton->bones.size() &&
                std::set<uint32_t>(source->source_joint_nodes.begin(),
                                   source->source_joint_nodes.end())
                        .size() == source->source_joint_nodes.size(),
            "Normative joint-to-original-node identity is complete and unique");
    size_t native_targets =
        part.deformation.morphs ? part.deformation.morphs->targets.size() : 0;
    bool mapped = source->source_morph_targets.size() == native_targets;
    for (size_t i = 0; i < native_targets; ++i)
      mapped &= source->source_morph_targets[i] == i;
    require(mapped, "Normative morph identity preserves original source target "
                    "indices without parsing names");
    const auto &material = asset.materials[source->material];
    require(part.color == material.color &&
                part.roughness == material.roughness,
            "Neutral creation preserves original material factors without MH "
            "colorization");
    const auto &primitive = *std::find_if(
        asset.primitives.begin(), asset.primitives.end(), [&](const auto &p) {
          return p.source_mesh == source->source_mesh &&
                 p.source_primitive == source->source_primitive &&
                 p.material == uint32_t(source->material) &&
                 p.mesh.indices.size() == part.mesh.indices.size();
        });
    require(source->source_vertices.size() == part.mesh.positions.size(),
            "Every compact native vertex retains its authored accessor index");
    bool exact_geometry = true;
    for (size_t i = 0; i < part.mesh.positions.size(); ++i) {
      auto original = source->source_vertices[i];
      auto p = primitive.mesh.positions[original],
           n = primitive.mesh.normals[original];
      exact_geometry &= length(part.mesh.positions[i] -
                               Vec3{-p.x * scale, (p.y - floor) * scale,
                                    -p.z * scale}) < 2e-6f;
      exact_geometry &=
          part.mesh.uv[i] == primitive.mesh.uv[original] &&
          length(part.mesh.normals[i] - Vec3{-n.x, n.y, -n.z}) < 1e-6f;
    }
    require(exact_geometry,
            "Neutral compact vertices preserve authored position/normal/UV "
            "under documented basis/ground transform");
    auto raw = nlohmann::json::parse(source->material_json);
    require(raw.at("name") == material.name &&
                nlohmann::json::parse(source->mtoon_json).at("shader") ==
                    "VRM/MToon",
            "Full original material and classic MToon metadata are retained "
            "separately");
    if (material.alpha != ImportedMaterial::AlphaMode::Opaque)
      alpha = true;
    if (material.normal_texture >= 0) {
      normal = true;
      auto &t = asset.textures[material.normal_texture];
      require(part.source_maps->normal.image ==
                  source->decoded_images->at(t.image),
              "Normal source view preserves original image identity");
    }
    if (material.color_texture >= 0) {
      auto &t = asset.textures[material.color_texture];
      require(part.source_maps->color.image ==
                      source->decoded_images->at(t.image) &&
                  part.source_maps->color.wrap_s == t.wrap_s,
              "Color source view preserves full original dimensions/sampler "
              "without flattening");
    }
    if (part.deformation.morphs) {
      morphs += part.deformation.morphs->targets.size();
      for (const auto &t : primitive.morph_targets)
        authored_normal_morph |= !t.normals.empty();
    }
    vertices += part.mesh.positions.size();
  }
  require(vertices < 30000 && morphs > 56 && alpha && normal &&
              authored_normal_morph,
          "Actual detailed source attributes, alpha, normal images and "
          "normal-morph payload survive conversion");
  auto again = create_humanoid(controls);
  require(again.parts[0].appearance->asset == avatar.parts[0].appearance->asset,
          "Repeated edits reuse verified immutable baseline and shared texture "
          "cache");
  auto male = create_humanoid(humanoid_baseline_defaults("vroid-beta-male"));
  require(male.parts.size() == 19 &&
              male.parts[0].deformation.skeleton->bones.size() == 67,
          "Actual CC0 male baseline retains its distinct67-joint source");
  auto old = humanoid_baseline_defaults("makehuman-hm08").encode();
  old["version"] = 1;
  old.erase("baseline");
  auto migrated = HumanoidControls::decode(old);
  require(migrated.baseline == "makehuman-hm08" && migrated.anime == .65f &&
              migrated.skin_color[0] == .78f,
          "Historical schema1 creator records preserve exact MH parameters and "
          "baseline");
  require(HumanoidControls::decode(controls.encode()).baseline ==
              "vroid-beta-female",
          "Schema2 explicitly retains selected licensed baseline identity");
  rejects([&] { create_humanoid(controls, [] { return true; }); },
          "Verified baseline creation supports ordered-worker cancellation");
  const auto temporary = std::filesystem::temp_directory_path() /
                         ("dome-vroid-hash-" + std::to_string(getpid()));
  std::filesystem::create_directories(temporary / "vroid");
  {
    std::ofstream bad(temporary / "vroid/base-female.vrm", std::ios::binary);
    std::vector<char> bogus(64, 'x');
    bad.write(bogus.data(), bogus.size());
  }
  rejects([&] { create_humanoid(controls, {}, temporary); },
          "Wrong baseline bytes fail identity verification without falling "
          "back to MH or another directory");
  std::filesystem::remove_all(temporary);
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
