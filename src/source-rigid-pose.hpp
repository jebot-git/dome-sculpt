// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/source-placement.hpp"
namespace dome::detail {
// Immutable imported identity, never reconstructed from names or JSON text.
std::shared_ptr<const SourceNodePlacement>
source_rig_instance(const Scene &, const SculptObject &);
// Worker-time source attachment frame before/after an atomic linked rig edit.
// The caller persists its exact world delta on the real rigid object; no frame
// depends on another live skin at render/pick time or after that skin is deleted.
std::vector<SceneGroup> source_rigid_groups(
    const Scene &, const SculptObject &, std::vector<SceneGroup>,
    const std::function<bool()> &cancelled = {});
} // namespace dome::detail
