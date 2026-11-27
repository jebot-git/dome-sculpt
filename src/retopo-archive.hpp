// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <nlohmann/json.hpp>

namespace dome::detail {
inline constexpr int retopo_split_archive_version = 2;
inline constexpr int retopo_subdivision_archive_version = 3;

inline bool retopo_archive_shape(const nlohmann::json &value) {
  if (!value.is_object() || value.size() != 5 || !value.contains("version")) return false;
  const auto &version = value.at("version");
  return version.is_number_integer() && (version == 1 || version == retopo_split_archive_version ||
                                        version == retopo_subdivision_archive_version);
}
} // namespace dome::detail
