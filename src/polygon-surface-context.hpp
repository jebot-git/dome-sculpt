// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <nlohmann/json_fwd.hpp>
namespace dome::detail {
// Structural access only while a genuine Scene39/material21/pool witness is
// being preflighted. This is not a material/frame/paint publication lease.
const nlohmann::json&polygon_surface_payload_shape(const nlohmann::json&);
bool polygon_surface_physical_payload(const nlohmann::json&);
}
