// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/vrm.hpp"
namespace dome::detail {
VrmExportReport export_vrm1(const std::vector<ObjectSurface> &,
                            const nlohmann::json &original,
                            const std::filesystem::path &,
                            const VrmExportOptions &);
}
