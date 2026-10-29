// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <filesystem>
#include <span>
#include <string>
namespace dome {
enum class ImportMode { StaticScene, PreserveGltf };
using ImportedMatrix = std::array<float, 16>;
struct ImportLimits {
  size_t max_file_bytes = 128 * 1024 * 1024,
         max_resource_bytes = 256 * 1024 * 1024;
  size_t max_vertices = 2'000'000, max_triangles = 2'000'000;
  bool external_resources =
      true; // Confined to the input file's directory tree.
  std::function<bool()> cancelled;
  ImportMode mode = ImportMode::StaticScene;
  size_t max_metadata_visits = 50'000'000;
  // Explicit VRM0 compatibility: retain non-ancestor skeleton hints as
  // advisory.
  bool allow_legacy_vrm_skin_hint = false;
};
struct ImportedImage {
  std::string name, mime_type, source_uri;
  std::vector<uint8_t> bytes;
};
struct ImportedTexture {
  int image = -1;
  int wrap_s = 10497, wrap_t = 10497, min_filter = 0, mag_filter = 0;
};
struct ImportedMaterial {
  std::string name = "Material";
  std::array<float, 4> color{1, 1, 1, 1};
  std::array<float, 3> emissive{};
  float metallic = 1, roughness = 1, alpha_cutoff = .5f, normal_scale = 1,
        occlusion_strength = 1;
  bool double_sided = false, unlit = false;
  enum class AlphaMode { Opaque, Mask, Blend } alpha = AlphaMode::Opaque;
  int color_texcoord = 0, metal_rough_texcoord = 0, normal_texcoord = 0,
      occlusion_texcoord = 0, emissive_texcoord = 0;
  int color_texture = -1, metal_rough_texture = -1, normal_texture = -1,
      occlusion_texture = -1, emissive_texture = -1;
};
struct ImportedInfluenceSet {
  uint32_t set = 0;
  std::vector<std::array<uint32_t, 4>> joints;
  std::vector<std::array<float, 4>> weights;
  int joint_component_type = 0, weight_component_type = 0;
  bool weights_normalized = false;
};
struct ImportedMorphTarget {
  // Empty means that this channel was absent, hence implicitly all zero.
  std::vector<Vec3> positions, normals, tangents;
};
struct ImportedNode {
  std::string name, extras_json;
  int parent = -1, mesh = -1, skin = -1, camera = -1, light = -1;
  std::vector<uint32_t> children;
  bool has_matrix = false, has_translation = false, has_rotation = false,
       has_scale = false;
  std::array<float, 3> translation{}, scale{1, 1, 1};
  std::array<float, 4> rotation{0, 0, 0, 1};
  ImportedMatrix local{}, world{};
  // Empty distinguishes absent weights from an authored all-zero array.
  std::vector<float> weights;
};
struct ImportedSkin {
  std::string name, extras_json;
  std::vector<uint32_t> joints; // Slot -> original glTF node index.
  int skeleton = -1;
  bool inverse_bind_matrices_authored = false, skeleton_hint_advisory = false;
  // Preserve the entire authored accessor, including trailing matrices.
  // Empty with authored=false means implicit identity per joint.
  std::vector<ImportedMatrix> inverse_bind_matrices;
};
struct ImportedMesh {
  std::string name, extras_json;
  std::vector<uint32_t> primitives;
  std::vector<std::string> target_names;
  std::vector<float> weights;
};
struct ImportedScene {
  std::string name, extras_json;
  std::vector<uint32_t> roots;
};
struct ImportedAnimationDiagnostic {
  std::string name;
  size_t channels = 0, samplers = 0;
};
struct ImportedPrimitive {
  std::string name;
  Mesh mesh; // StaticScene bakes world; PreserveGltf retains mesh coordinates.
  bool mesh_local = false, normals_authored = false,
       morph_targets_authored = false;
  int source_mesh = -1, source_primitive = -1, source_node = -1;
  std::string extras_json;
  std::vector<ImportedInfluenceSet> influences;
  std::vector<ImportedMorphTarget> morph_targets;
  uint32_t material = 0;
  std::array<float, 16> source_world{1, 0, 0, 0, 0, 1, 0, 0,
                                     0, 0, 1, 0, 0, 0, 0, 1};
  std::vector<std::array<float, 4>> colors, tangents;
  std::vector<std::array<float, 2>> uv1;
};
struct ImportedAsset {
  ImportMode mode = ImportMode::StaticScene;
  int default_scene = -1;
  std::string extras_json;
  std::vector<std::string> extensions_used;
  std::vector<ImportedNode> nodes;
  std::vector<ImportedSkin> skins;
  std::vector<ImportedMesh> meshes;
  std::vector<ImportedScene> scenes;
  // Tracks are outside this importer; these diagnostics make omission explicit.
  std::vector<ImportedAnimationDiagnostic> omitted_animations;
  std::vector<ImportedPrimitive> primitives;
  std::vector<ImportedMaterial> materials;
  std::vector<ImportedImage> images;
  std::vector<ImportedTexture> textures;
  std::vector<std::string> warnings;
};
// OBJ concave polygons are ear-clipped. Default static glTF import explicitly
// rejects skins/morphs. PreserveGltf retains node/skin/morph authoring payload
// in mesh-local coordinates without converting authored inverse binds to
// Skeleton. Unsupported required extensions fail explicitly in both modes. glTF
// images are retained as encoded bytes; this function does not decode them.
ImportedAsset import_mesh(const std::filesystem::path &,
                          const ImportLimits & = {});
// Bounded native GLB/VRM bytes, never a filesystem resource resolver. The
// explicit mode argument defaults to preservation and overrides limits.mode.
// external_resources is forced false; GLB BIN and embedded data-URIs only.
ImportedAsset import_glb(std::span<const uint8_t>, ImportLimits limits = {},
                         ImportMode mode = ImportMode::PreserveGltf);
} // namespace dome
