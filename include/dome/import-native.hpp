// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "import-source.hpp"
namespace dome {
// Native ufbx-backed conversion. The exact FBX/OBJ/MTL/resources remain in
// original; canonical_glb is an explicitly labelled authoring adapter.
CapturedImportSource capture_native_source(const std::filesystem::path &,
                                          ImportOptions = {}, ImportLimits = {});
} // namespace dome
