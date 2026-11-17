// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake.hpp"
namespace dome::detail {
// Actual object-space roles after authentic artist-frame/morph/skin transport,
// rotated into the low target's coordinate system. Never a display normal map.
using SourceBakeNormalResolver=std::function<std::array<Vec3,2>(uint32_t,std::array<float,3>)>;
struct SourceBakeNormalOptions {
 size_t max_working_bytes=256*1024*1024;
 std::function<bool()> cancelled;
};
SourceBakeNormalResolver source_bake_normals(const SculptObject &,CanvasTransform,
 const SourceBakeNormalOptions & = {});
// Internal dual-role route; the public legacy Bake API stays byte/layout exact.
BakeResult bake_mesh_source_normals(const Mesh &,const BakeSource &,const BakeOptions &,
 const SourceBakeNormalResolver &);
}
