// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
namespace dome {
// The stable material slot is object-local. Raw import table indices and
// transient draw-range positions never replace either part of this identity.
struct SurfaceMaterialIdentity {
  uint64_t object=0;
  uint32_t texture_set=0;
  bool operator==(const SurfaceMaterialIdentity&)const=default;
};
} // namespace dome
