// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
#include "dome/humanoid-creator.hpp"
#include <algorithm>
namespace dome::detail {
// Advisory UI eligibility only. The export worker independently validates the
// immutable original metadata, source graph and all material authorities.
inline bool editable_avatar_source(const SculptObject &object) {
  return object.appearance && object.appearance->asset &&
      object.appearance->asset->avatar_version==ImportedAvatarVersion::Vrm1;
}
inline bool editable_avatar_view(const EditorSnapshot &frame) {
  if(!frame.state||!frame.surface||!frame.state->can_export_avatar||
      frame.surface->generation!=frame.state->generation||
      frame.surface->view_epoch!=frame.state->view_epoch)return false;
  return std::any_of(frame.surface->ranges.begin(),frame.surface->ranges.end(),
      [&](const SurfaceRange &range){return range.object==frame.state->active_object&&
        range.appearance&&range.appearance->asset&&
        range.appearance->asset->avatar_version==ImportedAvatarVersion::Vrm1;});
}
}
