// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-range.hpp"
#include "dome/document.hpp"
namespace dome::detail {
struct TextureSetWorkerView {
  bool pbr=false,weights=false,selected=false,masks=false;
  uint32_t joint=0;
  std::shared_ptr<const DeformationState> weight_values;
  std::function<float(uint32_t)> mask;
};
// Appends one shared object geometry stream and every nonempty material range.
// The caller owns the unpublished snapshot; cancellation discards it as a whole.
void append_texture_set_surface(MeshSnapshot&,const ObjectSurface&,
    const std::shared_ptr<const TextureSetPackedGeometry>&,
    const TextureSetWorkerView&,const TextureSetLimits & = {});
} // namespace dome::detail
