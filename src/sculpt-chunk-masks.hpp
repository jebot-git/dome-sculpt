// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "sculpt-scene-chunks.hpp"
namespace dome::detail {
struct SculptChunkMask {
  SculptSurfaceChunks::Chunk geometry;
  std::shared_ptr<const SculptMaskSnapshot> mask;
  std::vector<float> values;
  size_t retained_bytes() const {return sizeof(SculptChunkMask)+128+values.capacity()*sizeof(float);}
};
// Immutable scalar-only directory. A protection edit cannot become a geometry
// or placed/BVH key; a changed geometry packet samples only that packet when
// the scalar owner is unchanged.
class SculptChunkMasks {
public:
  using Ptr=std::shared_ptr<const SculptChunkMasks>;
  using Values=std::shared_ptr<const SculptChunkMask>;
  static Ptr capture(SculptSurfaceChunks::Ptr,std::shared_ptr<const SculptMaskSnapshot>,
      const Ptr &before,size_t retained_bytes,size_t working_bytes,
      LocalSurfaceCache::Cancel={});
  Values find(const SculptSurfaceChunks::Chunk &) const;
  size_t retained_bytes() const;
private:
  struct State;
  explicit SculptChunkMasks(std::shared_ptr<const State>);
  std::shared_ptr<const State> state_;
};
} // namespace dome::detail
