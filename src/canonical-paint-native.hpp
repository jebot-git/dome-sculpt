// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "canonical-paint-sampling.hpp"
namespace dome::detail {
// UI eligibility only. Publication independently executes the owning factory,
// complete current model proof and aggregate resource preflight.
bool canonical_paint_retention_supported(const SculptObject &) noexcept;
bool canonical_paint_source_restorable(const SculptObject &) noexcept;
void restore_canonical_paint(Scene &,uint64_t,const std::function<bool()> & = {});
}
