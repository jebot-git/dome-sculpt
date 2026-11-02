// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/source-uv-bake.hpp"
#include "polygon-conversion.hpp"
namespace dome::detail {
// Detached atomic raw-current-corner operation. This preserves the immutable
// initial corner basis and complete old authoring state under its own Origin.
// Prepared target colours use a separate owning UV receipt, not this raw path.
void reparameterize_raw_polygon_colors(Scene &,uint64_t,const Mesh &,
    std::span<const uint32_t>,const SourceUvBakeOptions & = {});
}
