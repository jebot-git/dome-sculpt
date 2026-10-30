// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>
#include <nlohmann/json.hpp>
namespace dome::archive_detail {
struct VolumeHeader {
  int version;
  float voxel, band;
};
// Header-only validation precedes any lattice/paint allocation, both for flat
// Volume files and every nested Volume in Scene aggregate storage preflight.
inline VolumeHeader volume_header(const nlohmann::json &j) {
  if (!j.is_object() || j.at("format") != "dome-sculpt" ||
      !j.at("version").is_number_integer())
    throw std::runtime_error("Unsupported project format");
  auto version = j.at("version").get<int64_t>();
  if (version < 1 || version > 4)
    throw std::runtime_error("Unsupported project format");
  for (const auto &[key, value] : j.items()) {
    (void)value;
    bool known =
        key == "format" || key == "version" || key == "voxel_size" ||
        key == "samples" || key == "sculpt_layers" ||
        key == "active_sculpt_layer" || key == "next_sculpt_layer" ||
        (version >= 3 && (key == "sculpt_mask" || key == "sculpt_controls")) ||
        (version == 4 && key == "distance_band");
    if (!known)
      throw std::runtime_error(
          "Unknown or version-incompatible volume field: " + key);
  }
  if (!j.at("voxel_size").is_number())
    throw std::runtime_error("Invalid voxel size");
  float voxel = j.at("voxel_size").get<float>();
  if (!std::isfinite(voxel) || voxel < .00001f || voxel > 1)
    throw std::runtime_error("Invalid voxel size");
  float band = voxel * 4;
  if (version == 4) {
    if (!j.at("distance_band").is_number())
      throw std::runtime_error("Invalid physical distance band");
    band = j.at("distance_band").get<float>();
    if (!std::isfinite(band) || band < .00004f || band > 4)
      throw std::runtime_error(
          "Physical distance band outside supported metre range");
  }
  if (!j.at("samples").is_array())
    throw std::runtime_error("Invalid project samples");
  if (version >= 2 && !j.contains("sculpt_layers"))
    throw std::runtime_error("Missing sculpt layer data");
  if (version >= 3) {
    if (!j.at("sculpt_mask").is_object() ||
        !j.at("sculpt_mask").at("values").is_array() ||
        !j.at("sculpt_controls").is_object())
      throw std::runtime_error("Invalid sculpt mask/controls shape");
  }
  return {int(version), voxel, band};
}
} // namespace dome::archive_detail
