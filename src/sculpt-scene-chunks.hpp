// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "sculpt-surface-chunks.hpp"
#include "affine-interaction.hpp"

namespace dome::detail {
// Scalar ownership is intentionally separate from geometry. Captures only the
// actual immutable mask map/background/voxel meaning, never Volume history or
// a paint/material collection. A mask edit must not become a geometry key.
class SculptMaskSnapshot {
public:
  explicit SculptMaskSnapshot(const Volume &);
  float sample(Vec3) const;
  bool same(const SculptMaskSnapshot &) const;
  size_t retained_bytes() const;
private:
  Volume::GeometryOwner values_;
  float background_,voxel_;
};
struct SculptObjectChunks {
  uint64_t object=0;
  std::string name;
  Material material;
  CanvasTransform local{},world{};
  std::optional<AffineInteractionFrame> affine;
  SculptSurfaceChunks::Ptr geometry;
  std::shared_ptr<const SculptMaskSnapshot> mask;
  bool source_dormant=false;
  Vec3 placed_position(Vec3) const;
  Vec3 placed_normal(Vec3) const;
  bool reflected() const;
};
struct SculptSceneChunkLimits {
  SculptChunkLimits geometry;
  size_t retained_bytes=512*1024*1024,working_bytes=1024*1024*1024;
};
struct SculptSceneChunkContext {
  uint64_t generation=0,revision=0,view_epoch=0;
  bool mask_view=false;
};
class SculptSceneChunks;
struct SculptScenePreview {
  // These remain genuine packed/painted/legacy surfaces. A chunk object is
  // never represented by an empty fake ObjectSurface or invented range.
  std::vector<ObjectSurface> legacy;
  std::shared_ptr<const SculptSceneChunks> chunks;
};
class SculptSceneChunks {
public:
  using Ptr=std::shared_ptr<const SculptSceneChunks>;
  static SculptScenePreview capture(const Scene &,SculptSceneChunkContext,
      const Ptr &before={},SculptSceneChunkLimits={},LocalSurfaceCache::Cancel={});
  const std::vector<SculptObjectChunks> &objects() const {return objects_;}
  const SculptSceneChunkContext &context() const {return context_;}
  size_t retained_bytes() const {return bytes_;}
  const SculptObjectChunks *find(uint64_t) const;
  // Selected-object Cut/export boundary. Retains original local indices until
  // the existing exact world placement/reflection policy is applied once.
  Mesh materialize(uint64_t,LocalSurfaceCache::Cancel={}) const;
private:
  struct Cache {uint64_t object;SculptSurfaceChunks::Ptr geometry;};
  SculptSceneChunks()=default;
  SculptSceneChunkContext context_;
  std::vector<SculptObjectChunks> objects_;
  std::vector<Cache> caches_;
  size_t bytes_=0;
  SculptSceneChunkLimits limits_;
};
// Sole private bridge into Scene's common visibility/empty-polygon policy.
// No duplicate hand-written Scene traversal decides what is displayed.
struct SculptScenePreviewAccess {
  static std::vector<ObjectSurface> surfaces(const Scene &,
      const LocalSurfaceCache::Cancel &,const Scene::SculptMesher &,
      const std::function<bool(const SculptObject &)> &);
};
} // namespace dome::detail
