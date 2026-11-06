// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-surface.hpp"
#include "dome/tangents.hpp"
namespace dome::detail {
struct TextureSetExportField {
  TextureSetId set=0;
  size_t first=0,count=0;
  Material material;
  std::shared_ptr<const TextureMaps> textures;
};
// One REST geometry and deformation expansion for the entire mesh instance.
// Material ranges refer into one index stream; no per-material mesh/rig copies.
struct TextureSetExport {
  ObjectSurface surface;
  std::vector<Tangent> tangents;
  std::vector<TextureSetExportField> fields;
  size_t working_bound=0;
};
size_t texture_set_export_working_bound(const ObjectSurface &,
    const TextureSetLimits & = {});
TextureSetExport compile_texture_set_export(const ObjectSurface &,
    const TextureSetLimits & = {});
} // namespace dome::detail
