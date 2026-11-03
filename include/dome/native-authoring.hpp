// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gltf-export.hpp"
#include <optional>
#include <span>
namespace dome {
// Worker API: capture Scene::snapshot() before publishing the const owner.
// Source surfaces are derived internally, before any portable compiler runs.
struct NativeAuthoringExport {
  enum class Scope { WholeScene, ExportedObjects };
  std::shared_ptr<const Scene> scene;
  Scope scope = Scope::WholeScene;
  // WholeScene requires an empty list. ExportedObjects requires exact unique
  // visible, nonempty object IDs. Historical/source dependencies remain owned.
  std::vector<uint64_t> objects;
  // Intent only: permits a portable standard view alongside exact native data.
  // This never authenticates changed surfaces or grants compiler capabilities.
  bool allow_portable_fallback = false;
};
struct NativeAuthoringReadLimits {
  size_t max_container_bytes = 256*1024*1024;
  size_t max_json_bytes = 16*1024*1024;
  size_t max_native_bytes = 128*1024*1024;
  size_t max_decoded_native_bytes = 256*1024*1024;
  // Emitted object remaps; decoded Scene quotas independently cover hidden data.
  size_t max_objects = 256;
  std::function<bool()> cancelled;
};
// Additive Scene-owner route. Existing writer-option layouts are unchanged.
// options.emission and competing native markers refuse rather than override.
GltfExportReport export_glb_authoring(const NativeAuthoringExport &,
                                     const std::filesystem::path &,
                                     const GltfExportOptions & = {});
// Absent marker returns nullopt; malformed/present competing authority refuses.
// Restore the returned Scene directly, never recapture the container as source.
std::optional<Scene> extract_native_authoring(std::span<const uint8_t>,
                                             const NativeAuthoringReadLimits & = {});
}
