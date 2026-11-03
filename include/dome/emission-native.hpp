// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
#include "emission-interchange.hpp"
#include <optional>
namespace dome {
// Explicit permission for the measured PNG/strength view. The exact native
// owner and FLOAT32 contributions remain independently recoverable in the GLB.
struct NativeEmissionExport {
  enum class Scope { ExportedObjects, WholeScene };
  std::shared_ptr<const Scene> scene;
  Scope scope = Scope::ExportedObjects;
  bool allow_measured_core_fallback = false;
};
struct NativeEmissionReadLimits {
  size_t max_container_bytes = 256*1024*1024;
  size_t max_json_bytes = 16*1024*1024;
  size_t max_native_bytes = 128*1024*1024;
  size_t max_exr_bytes = 128*1024*1024;
  size_t max_images = 256;
  EmissionInterchangeLimits image;
  std::function<bool()> cancelled;
};
// Worker-only surface extraction. Other channels are compiled without routing
// physical coefficients through the historical byte emission plane.
std::vector<ObjectSurface> emission_export_surfaces(const Scene &,
                                      const std::function<bool()> &cancelled = {});
// No marker returns nullopt. A present unknown/corrupt marker always refuses.
// All emitted JSON and BIN resources are authenticated before native parsing.
// Restore this Scene directly: do not make the container another raw source.
std::optional<Scene> extract_native_emission(std::span<const uint8_t>,
                                            const NativeEmissionReadLimits & = {});
}
