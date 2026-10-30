// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
namespace dome::detail {
struct NormalPreparationCacheStats {uint64_t hits=0,builds=0;};
NormalPreparationCacheStats normal_preparation_cache_stats();
} // namespace dome::detail
