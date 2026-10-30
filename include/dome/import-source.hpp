// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "import.hpp"
#include "import-options.hpp"
#include <nlohmann/json_fwd.hpp>
namespace dome {
struct ImportSourceFile {
  // Portable, normalized, UTF-8 path relative to the source root directory.
  std::string name;
  std::shared_ptr<const std::vector<uint8_t>> bytes;
};
struct ImportSourceReceipt {
  ImportSourceFormat format = ImportSourceFormat::Automatic;
  ImportOptions options;
  ImportSourceFile root;
  std::vector<ImportSourceFile> resources;
  // Exact conversion implementation/settings identity, not an original format.
  std::string adapter;
  size_t stored_bytes() const;
  void validate(const ImportLimits & = {}) const;
  nlohmann::json encode() const;
  // Complete bounded shape/count/byte preflight, before copying binary files.
  static size_t encoded_bytes(const nlohmann::json &, const ImportLimits & = {});
  static std::shared_ptr<const ImportSourceReceipt>
  decode(const nlohmann::json &, const ImportLimits & = {});
};
struct CapturedImportSource {
  std::shared_ptr<const ImportSourceReceipt> original;
  std::shared_ptr<const std::vector<uint8_t>> canonical_glb;
  std::vector<std::string> diagnostics;
};
// Single-capture root/resource resolver. A resource is opened only on its first
// normalized name; later calls return the same immutable bytes. No parser or
// archive ever rereads an already captured source from the filesystem.
class ImportSourceCapture {
public:
  ImportSourceCapture(const std::filesystem::path &, ImportOptions = {},
                      ImportLimits = {});
  ~ImportSourceCapture();
  ImportSourceCapture(const ImportSourceCapture &) = delete;
  ImportSourceCapture &operator=(const ImportSourceCapture &) = delete;
  const ImportSourceReceipt &source() const;
  std::shared_ptr<const std::vector<uint8_t>> resource(std::string_view name);
  CapturedImportSource finish(
      std::shared_ptr<const std::vector<uint8_t>> canonical_glb,
      std::string adapter, std::vector<std::string> diagnostics = {});
private:
  struct State;
  std::unique_ptr<State> state_;
};
// Capture and embed a glTF/GLB resource bundle without changing accessor,
// primitive, material, node or animation identities. External resources become
// buffer views in the canonical GLB; original JSON/GLB/resources remain exact.
CapturedImportSource capture_gltf_source(const std::filesystem::path &,
                                        ImportOptions = {}, ImportLimits = {});
} // namespace dome
