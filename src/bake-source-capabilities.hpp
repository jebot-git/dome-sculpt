// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
namespace dome::detail {
// Cheap source-list eligibility only. Actual worker capture independently
// replays material/frame/normal/paint owners before any Bake publication.
bool bake_source_supported(const SculptObject &) noexcept;
}
