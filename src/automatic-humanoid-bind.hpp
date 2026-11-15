// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/avatar.hpp"
namespace dome::detail {
// Native role authority supplies only prospective source-family support. The
// generic public automatic_bind retains its original intrinsic policy.
SkinWeights automatic_bind_humanoid(const Mesh &, const Skeleton &,
                                    const HumanoidMapping &, BindOptions,
                                    const std::function<bool()> &cancelled);
}
