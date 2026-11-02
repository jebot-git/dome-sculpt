// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-sampling.hpp"
#include "dome/source-uv-bake.hpp"
namespace dome::detail {
// Replay only the normal/coverage metadata of the existing SourceUV transfer.
// This is not the resolved normal field: its interpretation stays explicit.
std::shared_ptr<const NormalDetailAuthority> transfer_normal_detail(
    const PaintedMesh &,const Mesh &,const SourceUvBakeOptions &);
std::shared_ptr<const SourceNormalSampling> retain_normal_sampling(
    const SourceUvBakeInput &,const SourceUvBakeResult &,const SourceUvBakeOptions &);
bool same_normal_detail(const NormalDetailAuthority *,const BakedTextures *,uint32_t,bool content = true);
} // namespace dome::detail
