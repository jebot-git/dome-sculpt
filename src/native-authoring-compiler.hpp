// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-authoring-capture.hpp"
#include "dome/source-uv-bake.hpp"
namespace dome::detail {
struct GltfDocument;
struct NativeAuthoringEmission {
  uint64_t object=0;
  Image image;
  std::array<float,3> factor{};
  float strength=1;
  double maximum_absolute_error=0,maximum_relative_error=0;
  size_t erased_nonzero_samples=0;
  bool unlit=false;
  size_t unlit_ignored_samples=0;
};
struct NativeAuthoringCompilation {
  std::vector<ObjectSurface> surfaces;
  std::vector<std::string> diagnostics;
  std::vector<NativeAuthoringEmission> emission;
};
// The only input geometry/material authority is the sealed original capture.
// Portable surfaces never replace the Scene retained in the container.
NativeAuthoringCompilation compile_native_authoring(
    const NativeAuthoringCapture &, const SourceUvBakeOptions &, size_t,
    const std::function<bool()> &);
void finish_native_authoring_emission(GltfDocument &,const NativeAuthoringCompilation &,
    size_t,const std::function<bool()> &);
} // namespace dome::detail
