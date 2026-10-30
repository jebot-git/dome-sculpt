// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/volume.hpp"
#include <optional>
namespace dome {
// Pure draft controls. The caller retains/validates its object source lease
// and publishes one guarded transaction only after an accepted return value.
std::optional<Volume::SculptSettings> sculpt_controls_dialog(Volume::SculptSettings);
}
