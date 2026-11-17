// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake.hpp"
namespace dome::detail {
// Actual object-space roles after authentic artist-frame/morph/skin transport,
// rotated into the low target's coordinate system. Never a display normal map.
struct SourceBakeNormalOptions {
 size_t max_working_bytes=256*1024*1024;
 std::function<bool()> cancelled;
};
class PaintOverlayPacket;
class CanonicalNormalConversion;
class BakedNormalField;
// Only the owning source adapter can mint this callable and its normal/material
// pairing. A bare callback cannot authorize a retained normal-detail event.
class SourceBakeNormalResolver {
public:
 SourceBakeNormalResolver()=default;
 explicit operator bool()const noexcept{return bool(sample_);}
 std::array<Vec3,2> operator()(uint32_t triangle,std::array<float,3> bary)const{return sample_(triangle,bary);}
 bool matches_prepared_source(const TextureMaps &)const noexcept;
 bool requires_source_pairing()const noexcept;
private:
 using Sample=std::function<std::array<Vec3,2>(uint32_t,std::array<float,3>)>;
 SourceBakeNormalResolver(Sample sample,std::shared_ptr<const PaintNormalProjection> projection,
     std::shared_ptr<const PaintOverlayPacket> packet,
     std::shared_ptr<const CanonicalNormalConversion> canonical={},
     std::shared_ptr<const BakedNormalField> field={})
  :sample_(std::move(sample)),projection_(std::move(projection)),packet_(std::move(packet)),
   canonical_(std::move(canonical)),field_(std::move(field)){}
 Sample sample_;
 std::shared_ptr<const PaintNormalProjection> projection_;
 std::shared_ptr<const PaintOverlayPacket> packet_;
 std::shared_ptr<const CanonicalNormalConversion> canonical_;
 std::shared_ptr<const BakedNormalField> field_;
 friend SourceBakeNormalResolver source_bake_normals(const SculptObject &,CanvasTransform,const SourceBakeNormalOptions &);
};
SourceBakeNormalResolver source_bake_normals(const SculptObject &,CanvasTransform,
 const SourceBakeNormalOptions & = {});
// Internal dual-role route; the public legacy Bake API stays byte/layout exact.
BakeResult bake_mesh_source_normals(const Mesh &,const BakeSource &,const BakeOptions &,
 const SourceBakeNormalResolver &);
}
