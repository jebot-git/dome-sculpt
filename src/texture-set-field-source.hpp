// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/texture-sets.hpp"
namespace dome::detail {
class PolygonSurfaceOwner;
class PolygonSurfacePreview;
struct TextureSetFieldSample {
  uint32_t triangle=0;
  uint64_t face=0;
  std::array<uint64_t,3> corners{};
  std::array<double,3> barycentric{};
  std::array<float,2> paint_uv{};
  std::optional<std::array<float,2>> normal_uv;
};
// Detached field preparation, not permission to publish an unfinished Scene40
// codec/render/export adapter. The source is a genuine immutable native field;
// a sealed membership is independently bound to its actual shared geometry.
class TextureSetFieldSource final {
public:
  static size_t working_bytes(const TextureSetView &,const TextureSetLimits & = {});
  static std::shared_ptr<const TextureSetFieldSource> capture(
      const TextureSetView &,std::shared_ptr<const TextureSetFaceDomain>,
      const TextureSetLimits & = {});
  void authenticate_owner(const std::shared_ptr<const TextureSetFieldSource>&)const;
  void validate_field(const TextureSetView &,const TextureSetLimits & = {})const;
  const TextureSetView&field()const noexcept{return field_;}
  const std::shared_ptr<const PaintedMesh>&original_paint()const noexcept{return original_;}
  const std::shared_ptr<const PolygonSurfaceOwner>&polygon_owner()const noexcept{return polygon_;}
  // The private source view uses the original proved raster/attachment and
  // current member COW values. Editable member Paint retains its subset raster.
  std::shared_ptr<const PaintedMesh> read_view(const TextureSetView &,
      const TextureSetLimits & = {})const;
  TextureSetFieldSample sample(uint32_t,std::array<double,3>,
      const TextureSetLimits & = {})const;
  Vec3 normal(uint32_t,std::array<double,3>,const AffineTransform &,bool pbr,
      const TextureSetLimits & = {})const;
private:
  friend struct TextureSetAccess;
  static std::shared_ptr<const TextureSetFieldSource> capture_prepared(
      const TextureSetView &,std::shared_ptr<const TextureSetFaceDomain>,
      std::shared_ptr<const PolygonSurfacePreview>,const TextureSetLimits &);
  TextureSetFieldSource()=default;
  TextureSetFieldSource(TextureSetFieldSource&&)=default;
  mutable std::weak_ptr<const TextureSetFieldSource>self_;
  TextureSetView field_;
  std::shared_ptr<const PaintedMesh>original_;
  std::shared_ptr<const PolygonSurfaceOwner>polygon_;
  std::shared_ptr<const PolygonSurfacePreview>preview_;
};
// Uses the real source topology/face/corner IDs and one shared Mesh owner.
// Every triangle receives exactly one requested set; polygon faces cannot split
// between sets. This does not mutate/publish the caller's Scene or collection.
std::vector<std::shared_ptr<const TextureSetFieldSource>> prepare_texture_set_field_sources(
    const SculptObject &,std::span<const TextureSetId>,const TextureSetLimits & = {});
} // namespace dome::detail
