// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/polygon-controls.hpp"
#include <optional>
namespace dome {
bool polygon_conversion_dialog();
std::optional<PolygonControls> polygon_controls_dialog(PolygonControls);
} // namespace dome
