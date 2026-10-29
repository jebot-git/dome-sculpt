// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "math.hpp"
#include <nlohmann/json.hpp>
#include <string_view>
namespace dome {
enum class MToonVersion { V0, V1 };
enum class MToonAlpha { Opaque, Mask, Blend };
enum class MToonCull { Off, Front, Back };
enum class MToonOutline { None, World, Screen };
enum class MToonSlot : size_t {
  Base,
  Shade,
  Normal,
  Emission,
  Rim,
  Matcap,
  Shift,
  OutlineWidth,
  UvMask,
  ReceiveShadow,
  ShadingGrade,
  Count
};
enum class MToonEncoding { Linear, Srgb, Normal };
struct MToonTexture {
  int index = -1;
  uint32_t texcoord = 0;
  int transform_texcoord = -1;
  std::array<float, 2> offset{}, scale{1, 1};
  float rotation = 0, strength = 1;
  MToonEncoding encoding = MToonEncoding::Linear;
  uint8_t channel = 0;
};
struct MToonParameters {
  std::array<float, 4> base_color{1, 1, 1, 1}; // Linear RGB, linear opacity.
  Vec3 shade_color{1, 1, 1}, emission{}, rim_color{}, matcap_color{1, 1, 1},
      outline_color{};
  float shift = 0, toony = .9f, gi_equalization = .9f, rim_power = 5,
        rim_lift = 0, rim_mix = 1;
  float normal_scale = 1, emission_strength = 1, alpha_cutoff = .5f,
        outline_width = 0, outline_mix = 1;
  std::array<float, 2> scroll{};
  float rotation_speed = 0; // V0 turns/second in bottom-left; V1 radians/sec.
  MToonAlpha alpha = MToonAlpha::Opaque;
  MToonCull cull = MToonCull::Back, outline_cull = MToonCull::Front;
  MToonOutline outline = MToonOutline::None;
  bool depth_write = true, alpha_to_coverage = false, normal_enabled = false;
  int render_queue = 2000, queue_offset = 0, source_blend = 1,
      destination_blend = 0;
  // V0 remains a separate lighting/rendering model. No conversion to V1.
  float receive_shadow = 1, shading_grade = 1, light_color_attenuation = 0,
        indirect_intensity = .1f;
  float outline_max_distance = 1, legacy_shader_version = 39;
  bool legacy_outline_fixed = false;
};
struct MToonRenderState {
  MToonCull cull = MToonCull::Back;
  bool blend = false, depth_write = true, alpha_to_coverage = false,
       maximum_alpha = false;
  int source_rgb = 1, destination_rgb = 0, source_alpha = 1,
      destination_alpha = 0;
  int queue = 2000;
};
using MToonGpuParameters = std::array<std::array<float, 4>, 12>;
static_assert(sizeof(MToonGpuParameters) == 48 * sizeof(float));
class MToonMaterial {
  MToonVersion version_ = MToonVersion::V1;
  nlohmann::json source_, initial_known_;
  nlohmann::json known() const;
  nlohmann::json draft_json() const;

public:
  MToonMaterial();
  MToonParameters parameters;
  std::array<MToonTexture, size_t(MToonSlot::Count)> textures;
  std::vector<std::string> diagnostics;
  MToonVersion version() const { return version_; }
  static MToonMaterial from_vrm0(const nlohmann::json &,
                                 size_t texture_count = SIZE_MAX);
  // Entire glTF material, including its VRMC_materials_mtoon extension.
  static MToonMaterial from_vrm1(const nlohmann::json &,
                                 size_t texture_count = SIZE_MAX);
  void validate(size_t texture_count = SIZE_MAX) const;
  nlohmann::json
  source_json() const; // Known edits merged; unknown data retained.
  nlohmann::json encode() const;
  static MToonMaterial decode(const nlohmann::json &,
                              size_t texture_count = SIZE_MAX);
  MToonGpuParameters gpu_parameters() const;
  MToonRenderState render_state(bool outline_pass = false) const;
};
struct MToonSamples {
  std::array<float, 4> base{1, 1, 1, 1};
  Vec3 shade{1, 1, 1}, emission{1, 1, 1}, rim{1, 1, 1}, matcap{};
  float shift = 0, receive_shadow = 1, shading_grade = 1;
};
struct MToonLighting {
  Vec3 normal{0, 0, 1}, view{0, 0, 1}, light_direction{0, 0, 1},
      light_color{1, 1, 1};
  Vec3 gi{}, gi_up{}, gi_down{};
  float shadow = 1;
  bool front_facing = true, outline_pass = false, normal_oriented = false;
};
struct MToonFragment {
  std::array<float, 4> color{};
  bool discard = false;
};
// Samples and lighting are linear; use sRGB texture formats or decode each
// texel before filtering. Alpha/data/normal channels are never gamma-converted
// twice.
std::array<float, 4> mtoon_default_texel(MToonSlot);
float mtoon_srgb_to_linear(float);
float mtoon_linear_to_srgb(float);
std::array<float, 4> mtoon_decode_texel(MToonEncoding, std::array<float, 4>);
MToonFragment mtoon_shade(const MToonMaterial &, const MToonSamples &,
                          const MToonLighting &);
Vec3 mtoon_facing_normal(MToonVersion, Vec3 normal, Vec3 view,
                         bool front_facing, bool outline = false);
Vec3 mtoon_normal_map(Vec3 encoded, float scale);
// Normal mapping orients geometric N before TBN construction in V1, while V0
// flips the mapped normal toward view. Pass result with normal_oriented=true.
Vec3 mtoon_material_normal(const MToonMaterial &, Vec3 geometric,
                           std::array<float, 4> tangent, Vec3 encoded,
                           Vec3 view, bool front, bool outline = false);
Vec3 mtoon_shading_normal(MToonVersion, Vec3 geometric,
                          std::array<float, 4> tangent, Vec3 encoded,
                          float scale, Vec3 view, bool front,
                          bool outline = false);
std::array<float, 2> mtoon_uv(const MToonMaterial &, MToonSlot,
                              std::array<float, 2>, double seconds,
                              float mask = 1);
// Right-handed glTF world input, top-left texture output (normal-up -> v0).
std::array<float, 2> mtoon_matcap_uv(Vec3 normal, Vec3 view,
                                     Vec3 up = {0, 1, 0});
float mtoon_outline_world_width(const MToonMaterial &, float sample = 1);
std::array<float, 2> mtoon_outline_clip_offset(
    const MToonMaterial &, std::array<float, 2> projected_normal,
    Vec3 view_normal, float clip_w, float aspect, float sample = 1);
std::string_view mtoon_glsl();
} // namespace dome
