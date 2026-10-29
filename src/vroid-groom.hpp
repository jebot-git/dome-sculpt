// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/humanoid-creator.hpp"
namespace dome::creator_detail {
void append_vroid_groom(CreatedHumanoid &,
                        const std::function<bool()> &cancelled);
}
