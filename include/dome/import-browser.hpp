// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "import-options.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace dome {
struct ImportSettings {
  ImportOptions choices;
  uint32_t paint_resolution=512;
  void validate() const;
  bool operator==(const ImportSettings &) const = default;
};
struct ImportEntry { std::string name;bool directory=false; };
struct ImportView {
  uint64_t request=0;
  std::filesystem::path directory;
  std::vector<ImportEntry> entries;
  size_t selected=0;
  ImportSettings settings;
  bool busy=false;
  std::string error;
};
// Worker-only, one bounded directory, no recursive traversal or model mutation.
ImportView scan_import_directory(const std::filesystem::path &,
                                const std::function<bool()> &cancelled={});
const char *import_format_label(ImportSourceFormat);
const char *import_units_label(ImportUnits);
const char *import_axes_label(ImportAxes);
const char *import_normals_label(ImportNormals);
} // namespace dome
