// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace dome {
struct Image;
struct PaintSourceTexture;
namespace paint_compute {class MaterialAtlas;}
namespace detail {class PaintOverlayPacket;}
namespace detail::bake_material {
enum Role { Color, Orm, Emission, Occlusion, PbrColor, TangentNormal, RoleCount };
inline constexpr size_t batch_capacity=8192;

struct ImageInput {
  const Image *image=nullptr;
  const PaintSourceTexture *original=nullptr;
  bool repeat=false;
};
struct Parameters {
  float color[4],emission[3];
  float roughness,metallic,occlusion_strength;
  bool composited,original,separate_pbr,include_overlay_emission,include_baked_normal;
};
struct Coordinates {
  float uv[RoleCount][2];
  float paint_uv[2],high_uv[2];
};
struct Values {
  float color[4],emission[3],roughness,metallic,occlusion;
  float pbr[4],normal[4];
  float baked_normal[4];
  uint32_t invalid;
};

class Sampler {
public:
  virtual ~Sampler()=default;
  virtual void read(std::span<const Coordinates>,std::span<Values>,const std::function<bool()> &cancelled)=0;
};

std::unique_ptr<Sampler> upload(const ImageInput (&)[RoleCount],Parameters,const PaintOverlayPacket *overlay=nullptr,
    const std::function<bool()> &cancelled={},const paint_compute::MaterialAtlas *atlas=nullptr,
    const std::function<void(size_t)> &reserve={});
} // namespace detail::bake_material
} // namespace dome
