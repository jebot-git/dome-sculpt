// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-authoring-capture.hpp"
#include "dome/source-uv-bake.hpp"
namespace dome::detail {
struct NativeAuthoringCompilation {
  std::vector<ObjectSurface> surfaces;
  std::vector<std::string> diagnostics;
};
// The only input geometry/material authority is the sealed original capture.
// Portable surfaces never replace the Scene retained in the container.
NativeAuthoringCompilation compile_native_authoring(
    const NativeAuthoringCapture &, const SourceUvBakeOptions &, size_t,
    const std::function<bool()> &);
} // namespace dome::detail
