// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-authoring-capture.hpp"
#include "dome/source-uv-bake.hpp"
namespace dome::detail {
struct GltfDocument;
// Selectively extract native float-paint geometry/other channels. Raw original
// SourceAuthored surfaces keep their actual source-only interpretation.
std::vector<ObjectSurface> native_authoring_surfaces(
    const Scene &,const std::function<bool()> &,const SourceUvBakeOptions &);
struct NativeAuthoringEmission {
  uint64_t object=0;
  Image image;
  std::array<float,3> factor{};
  float strength=1;
  double maximum_absolute_error=0,maximum_relative_error=0;
  size_t erased_nonzero_samples=0;
  bool unlit=false;
  size_t unlit_ignored_samples=0;
  // Actual emitted emission-only sampling transform, never an authored UV edit.
  std::array<float,2> texture_offset{};
};
struct NativeAuthoringCompilation {
  std::vector<ObjectSurface> surfaces;
  std::vector<std::string> diagnostics;
  std::vector<NativeAuthoringEmission> emission;
};
// Replay only the actual sealed capture's emission-normalized native32 view.
// This is neither a public surface permission nor original Source4 authority.
bool native_paint_compiled_emission(const NativeAuthoringCapture &,const ObjectSurface &,
    const std::function<bool()> & = {});
// The only input geometry/material authority is the sealed original capture.
// Portable surfaces never replace the Scene retained in the container.
NativeAuthoringCompilation compile_native_authoring(
    const NativeAuthoringCapture &, const SourceUvBakeOptions &, size_t,
    const std::function<bool()> &);
void finish_native_authoring_emission(GltfDocument &,const NativeAuthoringCompilation &,
    size_t,const std::function<bool()> &);
} // namespace dome::detail
