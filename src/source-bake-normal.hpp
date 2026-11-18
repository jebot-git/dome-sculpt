// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake.hpp"
namespace dome {struct SurfaceHit;}
namespace dome::detail {
// Actual object-space roles after authentic artist-frame/morph/skin transport,
// rotated into the low target's coordinate system. Never a display normal map.
struct SourceBakeNormalOptions {
 size_t max_working_bytes=256*1024*1024;
 std::function<bool()> cancelled;
};
class PaintOverlayPacket;
// Only the owning source adapter can mint this callable and its normal/material
// pairing. A bare callback cannot authorize a retained normal-detail event.
class SourceBakeNormalResolver {
public:
 SourceBakeNormalResolver()=default;
 explicit operator bool()const noexcept{return bool(sample_);}
 std::array<Vec3,2> operator()(uint32_t triangle,std::array<float,3> bary)const{return sample_(triangle,bary);}
 bool matches_prepared_source(const TextureMaps &)const noexcept;
private:
 using Sample=std::function<std::array<Vec3,2>(uint32_t,std::array<float,3>)>;
 SourceBakeNormalResolver(Sample sample,std::shared_ptr<const PaintNormalProjection> projection,
     std::shared_ptr<const PaintOverlayPacket> packet)
  :sample_(std::move(sample)),projection_(std::move(projection)),packet_(std::move(packet)){}
 Sample sample_;
 std::shared_ptr<const PaintNormalProjection> projection_;
 std::shared_ptr<const PaintOverlayPacket> packet_;
 friend SourceBakeNormalResolver source_bake_normals(const SculptObject &,CanvasTransform,const SourceBakeNormalOptions &);
};
SourceBakeNormalResolver source_bake_normals(const SculptObject &,CanvasTransform,
 const SourceBakeNormalOptions & = {});
// Generic numerical sampler is deliberately a separate type. It cannot mint
// paired original/prepared-frame authority or pass matches_prepared_source.
class GenericBakeNormalSampler {
public:
 explicit operator bool()const noexcept{return bool(sample_);}
 std::array<Vec3,2> operator()(const dome::SurfaceHit &hit)const{return sample_(hit);}
private:
 using Sample=std::function<std::array<Vec3,2>(const dome::SurfaceHit &)>;
 explicit GenericBakeNormalSampler(Sample sample):sample_(std::move(sample)){}
 GenericBakeNormalSampler()=default;
 Sample sample_;
 friend GenericBakeNormalSampler generic_bake_normals(const BakeSource &,const SourceBakeNormalOptions &);
};
GenericBakeNormalSampler generic_bake_normals(const BakeSource &,const SourceBakeNormalOptions & = {});
// Internal dual-role route; the public legacy Bake API stays byte/layout exact.
BakeResult bake_mesh_source_normals(const Mesh &,const BakeSource &,const BakeOptions &,
 const SourceBakeNormalResolver &);
}
