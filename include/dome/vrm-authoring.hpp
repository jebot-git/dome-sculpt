// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-authoring.hpp"
#include "vrm.hpp"
namespace dome {
// Explicit editable-native package with a standards-compatible portable VRM1
// view. Capture/authentication is derived from the immutable Scene owner, not
// caller-provided changed surfaces. Existing VRM option layouts stay unchanged.
VrmExportReport export_vrm_authoring(const NativeAuthoringExport &,
    const std::filesystem::path &,const VrmExportOptions & = {});
VrmExportReport export_vrm_authoring(const NativeAuthoringExport &,
    const std::filesystem::path &,const VrmExportOptions &,const SourceUvBakeOptions &);
}
