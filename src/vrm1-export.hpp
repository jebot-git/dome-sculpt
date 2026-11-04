// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/vrm.hpp"
namespace dome::detail {
struct EmissionExportPrepared;
class Vrm1AuthoringCompilation;
VrmExportReport export_vrm1_authoring(const Vrm1AuthoringCompilation &,
    const std::filesystem::path &,const VrmExportOptions &);
VrmExportReport export_vrm1(const std::vector<ObjectSurface> &,
                            const nlohmann::json &original,
                            const std::filesystem::path &,
                            const VrmExportOptions &,EmissionExportPrepared * = nullptr);
}
