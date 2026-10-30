// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/volume.hpp"
namespace dome::sculpt_detail {
struct RayInterval {
  double begin, end;
};
struct RayIndexStats {
  size_t cold_builds = 0, local_updates = 0, pages_scanned = 0, node_visits = 0,
         cells = 0;
  size_t live_bytes = 0, peak_bytes = 0, allocations = 0, cached_entries = 0,
         retained_source_bytes = 0;
  size_t fallback_queries = 0, index_refusals = 0;
};
RayIndexStats ray_index_stats();
void clear_ray_index_cache();
void ray_cell_visited();
using RayCancel = std::function<bool()>;
// Interval storage is reused by this thread. Reject recursive picking from a
// cancellation callback before it can invalidate the outer query's view.
class RayQueryScope {
public:
  RayQueryScope();
  ~RayQueryScope();
  RayQueryScope(const RayQueryScope &) = delete;
  RayQueryScope &operator=(const RayQueryScope &) = delete;
};
// Returned view belongs to this thread and lasts until its next interval query.
std::span<const RayInterval>
ray_intervals(const Volume &, const double origin[3], const double direction[3],
              double maximum, double threshold, const RayCancel &);
} // namespace dome::sculpt_detail
