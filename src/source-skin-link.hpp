// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/source-placement.hpp"

namespace dome::detail {
std::shared_ptr<const SourceNodePlacement> source_skin_link(
    std::span<const SceneGroup>, const DeformationState &);
bool same_source_skin_instance(const SourceNodePlacement &, const SourceNodePlacement &) noexcept;
const SculptObject &source_skin_reference(const Scene &, const SourceNodePlacement &);

// Adopt the ordinary skin worker's result into the original avatar instance.
bool link_native_source_skin(Scene &, uint64_t target, uint64_t reference,
                             const std::function<bool()> &);
void update_native_source_skins(const Scene &before, Scene &after, const SourceNodePlacement &,
                                const std::function<bool()> &);
void authenticate_native_source_skin(const ObjectSurface &, const ObjectSurface &reference,
                                     const std::function<bool()> &);
} // namespace dome::detail
