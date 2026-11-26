// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "source-bake-normal.hpp"
#include "baked-emission-domain.hpp"
namespace dome::detail {
// Worker-only route for a source domain already captured from the same owning
// Scene/source by bake_object. It never grants the generic Bake API permission
// to omit a FLOAT field. Authenticate the exact target owner, ordered source/
// destination corners and actual outward ray columns before sharing that field.
BakeResult bake_mesh_retained_emission(const Mesh &,const BakeSource &,
    const BakeOptions &,const SourceBakeNormalResolver &,
    const std::shared_ptr<const BakedEmissionDomain> &,
    const std::shared_ptr<const Mesh> &target);
}
