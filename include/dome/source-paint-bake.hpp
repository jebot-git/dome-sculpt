// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "source-uv-bake.hpp"
#include "scene.hpp"
namespace dome {
struct SourcePaintBakeResult {
 SourceUvBakeResult sampling;
 std::shared_ptr<const TextureMaps> textures;
 std::shared_ptr<const SourceMaterialBindings> material_bindings;
};
// Explicit portable material compilation. Native independent sampler/LOD
// semantics cannot in general be flattened losslessly into a single standard
// image. This result does not grant strict export or native Scene authority;
// callers must retain the exact source and qualify the chosen portable view.
SourcePaintBakeResult bake_source_paint(const SourceUvBakeInput &,const Material &,
                                      const SourceUvBakeOptions & = {});
}
