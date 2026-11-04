// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-authoring-capture.hpp"
namespace dome::detail {
struct GltfDocument;
// Bind the final emitted remaps/BIN/JSON to this genuine sealed Scene capture.
// Existing unique object-node ownership and reader rules remain unchanged.
// The caller must complete its format-specific graph before this final binding.
void attach_native_authoring(GltfDocument &,const NativeAuthoringCapture &,size_t,
    const std::function<bool()> &);
}
