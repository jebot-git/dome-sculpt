// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "sculpt-scene-chunks.hpp"
#include "dome/mesh-processing.hpp"
#include "dome/tangents.hpp"
namespace dome::detail {
struct SculptPlacedChunkLimits {
  size_t retained_bytes=8*1024*1024,working_bytes=16*1024*1024;
};
// Factory-sealed geometry only: no Scene/material/paint/mask/history back-edge.
// The base placement is authored Scene placement. Transient held/canvas
// motion remains a draw uniform / query-ray transform, not a new Mesh owner.
class SculptPlacedChunk {
public:
  using Ptr=std::shared_ptr<const SculptPlacedChunk>;
  static Ptr capture(const SculptSceneChunks::Ptr &,uint64_t object,
      const SculptSurfaceChunks::Chunk &,const Ptr &before={},bool query=false,
      SculptPlacedChunkLimits={},LocalSurfaceCache::Cancel={});
  const SculptSurfaceChunks::Chunk &source() const {return source_;}
  const std::shared_ptr<const Mesh> &geometry_owner() const {return geometry_;}
  const std::shared_ptr<const SurfaceQuery> &query() const {return query_;}
  const SculptChunkBounds &bounds() const {return bounds_;}
  const Tangent &tangent() const {return tangent_;}
  bool reflected() const {return reflected_;}
  size_t retained_bytes() const {return bytes_;}
  bool placement_matches(const SculptObjectChunks &) const;
private:
  SculptPlacedChunk()=default;
  bool matches(const SculptObjectChunks &,const SculptSurfaceChunks::Chunk &) const;
  SculptSurfaceChunks::Chunk source_;
  std::shared_ptr<const Mesh> geometry_;
  std::shared_ptr<const SurfaceQuery> query_;
  CanvasTransform world_{};
  std::optional<AffineInteractionFrame> affine_;
  SculptChunkBounds bounds_;
  Tangent tangent_{1,0,0,1};
  bool reflected_=false;
  size_t bytes_=0;
};
} // namespace dome::detail
