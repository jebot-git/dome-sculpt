// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "deformation.hpp"
#include "import.hpp"
#include "paint.hpp"
namespace dome {
struct PaintSourceMaps;
struct HumanoidControls {
  std::string baseline = "vroid-beta-female";
  float height = 1.5978173f, anime = 0;
  // Signed proportions are bounded to [-1,1]; neutral means the authored base.
  float build = 0, shoulder_width = 0, waist_width = 0;
  float leg_length = 0, arm_length = 0, head_width = 0, head_height = 0;
  float eye_size = 0, eye_spacing = 0, nose_size = 0;
  float mouth_width = 0, jaw_width = 0;
  float hair_length = 0, hair_volume = 0, fringe = 0;
  std::array<float, 4> skin_color{1, 1, 1, 1};
  std::array<float, 4> iris_color{1, 1, 1, 1};
  std::array<float, 4> hair_color{1, 1, 1, 1};
  void validate() const;
  nlohmann::json encode() const;
  static HumanoidControls decode(const nlohmann::json &);
};
enum class HumanoidPreset { Neutral, Toon, Anime };
HumanoidControls humanoid_baseline_defaults(std::string_view baseline);
HumanoidControls
humanoid_preset(HumanoidPreset,
                std::string_view baseline = "vroid-beta-female");
struct CreatorAppearance {
  std::shared_ptr<const std::vector<uint8_t>> original_bytes;
  std::shared_ptr<const ImportedAsset> asset;
  std::shared_ptr<const std::vector<std::shared_ptr<const Image>>>
      decoded_images;
  int material = -1, source_mesh = -1, source_node = -1, source_primitive = -1,
      source_material = -1;
  // Compacted native vertex -> exact authored primitive accessor vertex.
  std::vector<uint32_t> source_vertices;
  std::string material_json, mtoon_json;
  std::shared_ptr<const std::string> vrm_json;
  // Native Skeleton joint index -> exact original glTF node, never name
  // inferred.
  std::vector<uint32_t> source_joint_nodes;
  // Native morph slot -> exact authored target index. Reorder/delete/add must
  // update or explicitly invalidate this identity; count/name are not proof.
  std::vector<uint32_t> source_morph_targets;
  // Explicit original-to-native orientation; absent legacy provenance cannot
  // authorize coordinate-sensitive VRM metadata export.
  std::optional<Quat> source_basis_quaternion;
};
struct NormalFrameSet;
struct HumanoidPart {
  std::string name;
  Mesh mesh;
  DeformationState deformation;
  std::array<float, 4> color{1, 1, 1, 1};
  bool toon = true;
  float roughness = .8f;
  std::shared_ptr<const CreatorAppearance> appearance;
  std::shared_ptr<const PaintSourceMaps> source_maps;
  bool unlit = false;
  std::shared_ptr<const NormalFrameSet> normal_frames; // Private prototype.
};
struct CreatedHumanoid {
  HumanoidControls controls;
  // Body, sclera, iris, pupil and hair have stable independent UV/topology.
  // All share the same metric rest skeleton and semantic joint indices.
  std::vector<HumanoidPart> parts;
  std::vector<std::string> notes;
  size_t primary_part = 0;
  void validate() const;
};
// Native generator using embedded, explicitly CC0 MakeHuman geometry/targets/
// rigging data. Additional eyes and groom geometry are original implementation.
CreatedHumanoid
create_humanoid(const HumanoidControls & = {},
                const std::function<bool()> &cancelled = {},
                const std::filesystem::path &asset_directory = {});
} // namespace dome
