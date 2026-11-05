// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-paint-sampling.hpp"
namespace dome::detail {
// A UV authoring result pairs the geometry-owned paint correspondence with
// the exact REST-indexed native model; neither contains evaluated positions.
struct RetainedNativePaintModel {
  RetainedNativePaint sampling;
  std::shared_ptr<const DeformationState> deformation;
};
RetainedNativePaintModel retain_native_paint_model(const SculptObject &,const Mesh &,
    std::span<const uint32_t>,const PaintSamplingLimits & = {});
} // namespace dome::detail
