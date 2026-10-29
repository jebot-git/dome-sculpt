// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <filesystem>
#include <string>
namespace dome {
struct ImportLimits {
  size_t max_file_bytes = 128 * 1024 * 1024,
         max_resource_bytes = 256 * 1024 * 1024;
  size_t max_vertices = 2'000'000, max_triangles = 2'000'000;
  bool external_resources =
      true; // Confined to the input file's directory tree.
  std::function<bool()> cancelled;
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
  bool double_sided = false;
  enum class AlphaMode { Opaque, Mask, Blend } alpha = AlphaMode::Opaque;
  int color_texcoord = 0, metal_rough_texcoord = 0, normal_texcoord = 0,
      occlusion_texcoord = 0, emissive_texcoord = 0;
  int color_texture = -1, metal_rough_texture = -1, normal_texture = -1,
      occlusion_texture = -1, emissive_texture = -1;
};
struct ImportedPrimitive {
  std::string name;
  Mesh mesh; // Node world transform baked into positions/normals.
  uint32_t material = 0;
  std::array<float, 16> source_world{1, 0, 0, 0, 0, 1, 0, 0,
                                     0, 0, 1, 0, 0, 0, 0, 1};
  std::vector<std::array<float, 4>> colors, tangents;
  std::vector<std::array<float, 2>> uv1;
};
struct ImportedAsset {
  std::vector<ImportedPrimitive> primitives;
  std::vector<ImportedMaterial> materials;
  std::vector<ImportedImage> images;
  std::vector<ImportedTexture> textures;
  std::vector<std::string> warnings;
};
// Native static triangle import. OBJ concave polygons are ear-clipped. glTF/GLB
// node skins/morph targets and unsupported required extensions fail explicitly.
// glTF images are retained as encoded bytes; this function does not decode
// them.
ImportedAsset import_mesh(const std::filesystem::path &,
                          const ImportLimits & = {});
} // namespace dome
