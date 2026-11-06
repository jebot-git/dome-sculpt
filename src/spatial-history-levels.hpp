// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
namespace dome::detail {
// Losing tracked geometry is not an observed history-button release. Keep the
// last accepted levels until a focused provider frame reports a real release.
inline std::array<SpatialHand,2> history_only_hands(const std::array<SpatialHand,2> &previous) noexcept {
  std::array<SpatialHand,2> result{};
  for(size_t i=0;i<result.size();++i){result[i].undo=previous[i].undo;result[i].redo=previous[i].redo;}
  return result;
}
}
