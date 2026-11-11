// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh-processing.hpp"
#include <memory>
namespace dome::detail {
// Private material-query composition, never a replacement draw/native Mesh.
// Small ordinary inputs retain the public SurfaceQuery behavior. Larger packed
// scenes retain ALL original attributes for material callbacks and use bounded
// compact geometry only inside independent BVHs. All callback/result triangle
// IDs remain in the original global packed namespace.
class PackedMaterialQuery {
 struct Impl;
 std::unique_ptr<Impl> impl_;
public:
 // Trusted private material policy: false proves that a whole original
 // triangle interval contains no eligible object/material. It never replaces
 // arbitrary per-triangle/barycentric filters or their shared work budget.
 using Eligibility=std::function<bool(uint32_t first,uint32_t count)>;
 struct Limits {
  size_t retained_bytes=512ull*1024*1024;
  size_t working_bytes=768ull*1024*1024;
  size_t partitions=128,part_triangles=262144,nodes=4194304;
 };
 struct Usage {
  size_t retained_bound=0,working_bound=0,partitions=0,nodes=0,compact_vertices=0;
  bool partitioned=false;
 };
 explicit PackedMaterialQuery(const Mesh&,const std::function<bool()>&cancelled={});
 PackedMaterialQuery(const Mesh&,const std::function<bool()>&cancelled,Limits,
                     size_t reserved_material_metadata=0);
 ~PackedMaterialQuery();
 PackedMaterialQuery(const PackedMaterialQuery&)=delete;
 PackedMaterialQuery&operator=(const PackedMaterialQuery&)=delete;
 const Mesh&mesh()const;
 Usage usage()const noexcept;
 SurfaceHit closest(Vec3)const; // Existing default2M total triangle-test budget.
 SurfaceHit closest(Vec3,const SurfaceQueryOptions&,const Eligibility & = {})const;
 std::optional<SurfaceHit> nearest(Vec3,const SurfaceQueryOptions&,const Eligibility & = {})const;
 std::optional<SurfaceQueryResult> nearest_result(Vec3,const SurfaceQueryOptions&,const Eligibility & = {})const;
 std::optional<SurfaceHit> raycast(Vec3,Vec3,float,const SurfaceQueryOptions&,const Eligibility & = {})const;
 std::optional<SurfaceQueryResult> raycast_result(Vec3,Vec3,float,const SurfaceQueryOptions&,const Eligibility & = {})const;
};
}
