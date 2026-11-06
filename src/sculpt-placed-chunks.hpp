// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "sculpt-placed-chunk.hpp"
namespace dome::detail {
struct SculptPlacedChunksLimits {
  size_t retained_bytes=512*1024*1024,working_bytes=1024*1024*1024;
  size_t nodes=65'536,metadata_steps=16'000'000;
  SculptPlacedChunkLimits chunk;
};
struct SculptPlacedChunksReport {
  size_t built=0,reused=0,removed=0,retained_bytes=0,working_bytes=0;
  bool complete_reconcile=false;
};
// One authored object, one persistent world-geometry/query directory. Material
// and live held/canvas state remain outside this geometry-only owner.
class SculptPlacedChunks {
public:
  using Ptr=std::shared_ptr<const SculptPlacedChunks>;
  static Ptr capture(const SculptSceneChunks::Ptr &,uint64_t object,
      const Ptr &before={},bool queries=false,SculptPlacedChunksLimits={},
      LocalSurfaceCache::Cancel={});
  const SculptSurfaceChunks::Ptr &geometry() const;
  const SculptPlacedChunksReport &report() const;
  // Allocation owned by this placement layer, excluding the genuine geometry
  // owner reported separately by geometry(). Standalone limits remain total.
  size_t placement_bytes() const;
  const SculptChunkBounds &bounds() const;
  bool has_queries() const;
  SculptPlacedChunk::Ptr find(const SculptSurfaceChunks::Chunk &) const;
  void for_each(const std::function<void(const SculptPlacedChunk::Ptr &)> &,
                LocalSurfaceCache::Cancel={}) const;
  // Traverse actual aggregate world bounds without rebuilding/flattening any
  // geometry. A query supplies its conservative static or full-affine bound
  // predicate; candidate owners retain their real local triangle namespace.
  void visit_bounds(const std::function<bool(const SculptChunkBounds &)> &,
      const std::function<void(const SculptPlacedChunk::Ptr &)> &,
      LocalSurfaceCache::Cancel={}) const;
private:
  struct State;
  explicit SculptPlacedChunks(std::shared_ptr<const State>);
  std::shared_ptr<const State> state_;
};
} // namespace dome::detail
