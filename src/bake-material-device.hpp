// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
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
using Uv=std::array<float,2>;
inline constexpr uint8_t no_coordinate=UINT8_MAX;
struct CoordinateLayout {
  uint8_t roles[RoleCount]{no_coordinate,no_coordinate,no_coordinate,no_coordinate,no_coordinate,no_coordinate};
  uint8_t paint=no_coordinate,high=no_coordinate,count=0;
};
struct ResultLayout {
  unsigned stride;
  bool emission,pbr,normal,baked_normal;
  unsigned channel_count() const noexcept {
    return 8+3*unsigned(emission)+4*(unsigned(pbr)+unsigned(normal)+unsigned(baked_normal));
  }
};
struct TransferLayout {
  CoordinateLayout coordinates;
  ResultLayout results;
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
  virtual void read(std::span<const Uv>,std::span<Values>,const std::function<bool()> &cancelled)=0;
};

std::unique_ptr<Sampler> upload(const ImageInput (&)[RoleCount],Parameters,TransferLayout,const PaintOverlayPacket *overlay=nullptr,
    const std::function<bool()> &cancelled={},const paint_compute::MaterialAtlas *atlas=nullptr,
    const std::function<void(size_t)> &reserve={});
} // namespace detail::bake_material
} // namespace dome
