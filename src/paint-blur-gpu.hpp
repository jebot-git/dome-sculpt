// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint-blur.hpp"
#include <array>
#include <memory>
namespace dome::detail {
inline constexpr size_t blur_tile=32,blur_halo=7,blur_input=blur_tile+2*blur_halo;
using BlurInputPlane=std::array<std::array<float,4>,blur_input*blur_input>;
using BlurOutputPlane=std::array<std::array<float,4>,blur_tile*blur_tile>;
struct ColorBlurInput {
  // Native sRGB straight RGB + linear color coverage, common and PBR.
  std::array<BlurInputPlane,2> color;
  // Authored common/PBR alpha, independent layer mask, exact indexed UV chart.
  BlurInputPlane metadata;
};
struct ColorBlurOutput {
  // Native sRGB straight common/PBR RGB + linear color coverage. Plane2:
  // independent straight authored common/PBR alpha, reserved0, coverage.
  // Decode, associated filtering and final encoding all execute on GPU.
  std::array<BlurOutputPlane,3> planes;
};
class ColorBlurGpu {
 public:
  explicit ColorBlurGpu(bool allow_software_validation);
  ~ColorBlurGpu();
  ColorBlurGpu(const ColorBlurGpu&)=delete;
  ColorBlurGpu&operator=(const ColorBlurGpu&)=delete;
  void run(const ColorBlurInput&,ColorBlurOutput&,float,const std::function<bool()>&);
  const std::string&device()const noexcept;
 private:
  struct Impl;std::unique_ptr<Impl>impl_;
};
}
