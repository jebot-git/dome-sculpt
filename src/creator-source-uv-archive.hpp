// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/creator-archive.hpp"
namespace dome::detail {
// Inspect canonical metadata before allocating any new coordinate arrays.
// Bare source archives use their existing local parser/storage limits; Scene
// additionally owns the feature-version admission of all model records.
void preflight_creator_source_coordinates(const nlohmann::json &models,unsigned scene_version,
                                          const CreatorArchiveLimits &limits={});
}
