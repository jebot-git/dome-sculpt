// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
namespace dome::detail {
class SourceKeyDerivation;
class FramePositionDomain;
// Exact prospective payload sizes, excluding independently budgeted working
// scratch. No future frame/deformation arrays need to exist to make this plan.
struct KeyArchiveProspect {
  std::shared_ptr<const FramePositionDomain> domain;
  size_t frame_payload_bytes=0,deformation_entries=0;
  const SourceKeyDerivation *reused_anchor=nullptr;
};
struct KeyArchiveUsage {size_t frame_bytes=0,deformation_entries=0;};
KeyArchiveUsage preflight_key_archive(const Scene &,uint64_t,
    const KeyArchiveProspect &,const std::function<bool()> &cancelled={});
}
