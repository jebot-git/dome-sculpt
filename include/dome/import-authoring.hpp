// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "import.hpp"
#include "scene.hpp"
namespace dome {
struct ImportAuthoringOptions {
  uint32_t paint_resolution = 512;
  ImportLimits limits;
};
struct ImportedAuthoring {
  Scene scene;
  std::vector<std::string> diagnostics;
  std::vector<uint64_t> objects;
};
// Detached preparation: never mutates the caller's Scene, source file, or raw
// imported attributes. File, image, tangent and paint work belongs on a worker.
ImportedAuthoring prepare_authoring_import(const Scene &,
                                           const std::filesystem::path &,
                                           ImportAuthoringOptions = {});
} // namespace dome
