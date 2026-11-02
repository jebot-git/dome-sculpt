// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/rig-rest.hpp"
namespace dome::detail {
// Shared worker-only completion for a proved bone-identity-aligned new REST
// graph. It transports each part's own current pose and original bind matrices;
// no other skin's authored pose is selected as a replacement.
RigRestTransport finish_rig_rest_transport(const Skeleton &,std::span<const JointTransform>,
 Skeleton,const AffineTransform &,RigRestOptions,const std::function<bool()> &);
} // namespace dome::detail
