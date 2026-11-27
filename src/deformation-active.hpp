// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/deformation.hpp"
#include <algorithm>

namespace dome::detail {
inline bool evaluates_skin(const DeformationState &state) noexcept {
  return state.skeleton && state.skin && (!state.pose.empty() || state.skeleton->inverse_bind_override);
}
inline bool evaluates_geometry(const DeformationState *state) noexcept {
  return state && (evaluates_skin(*state) || (state->morphs &&
      std::ranges::any_of(state->morph_weights, [](float weight) { return weight != 0; })));
}
} // namespace dome::detail
