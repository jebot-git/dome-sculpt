// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon-surface-owner.hpp"
namespace dome::detail {
// Private node record. References are supplied by the actual enclosing native
// polygon-origin pool; a record by itself grants no Scene/material authority.
// The real before-root is replayed before current geometry/attachment is minted.
using PolygonSurfaceOriginWriter=std::function<uint32_t(std::shared_ptr<const PolygonOrigin>)>;
using PolygonSurfaceOriginReader=std::function<std::shared_ptr<const PolygonOrigin>(uint32_t)>;
nlohmann::json encode_polygon_surface_node(
 const std::shared_ptr<const PolygonSurfaceOwner> &,const PolygonSurfaceOriginWriter &,
 const PolygonSurfaceTransferLimits & = {});
// Strict allocation-free shape/reference/scalar preflight. Existing JSON memory
// is borrowed; all edit IDs are charged before any replay/candidate copy.
size_t preflight_polygon_surface_node(const nlohmann::json &,size_t origin_count,
 const PolygonSurfaceTransferLimits & = {});
PolygonSurfaceStage decode_polygon_surface_node(
 const nlohmann::json &,size_t origin_count,const PolygonSurfaceOriginReader &,
 const PolygonSurfaceTransferLimits & = {});
} // namespace dome::detail
