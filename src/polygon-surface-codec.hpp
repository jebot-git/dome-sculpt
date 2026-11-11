// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon-surface-owner.hpp"
namespace dome::detail {
// Private node record. References are supplied by the actual enclosing native
// polygon-origin pool; a record by itself grants no Scene/material authority.
// The real before-root is replayed before current geometry/attachment is minted.
using PolygonSurfaceOriginWriter=std::function<uint32_t(std::shared_ptr<const PolygonOrigin>)>;
using PolygonSurfaceOriginReader=std::function<std::shared_ptr<const PolygonOrigin>(uint32_t)>;
using PolygonSurfaceGeometryReader=std::function<std::shared_ptr<const Mesh>(uint32_t)>;
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

class PolygonSurfaceNodeWriter final {
public:
 explicit PolygonSurfaceNodeWriter(const PolygonSurfaceTransferLimits & = {});
 uint32_t add(std::shared_ptr<const PolygonSurfaceOwner>);
 nlohmann::json encode(const PolygonSurfaceOriginWriter &) const;
 size_t size()const noexcept{return owners_.size();}
 size_t reserved_working_bytes()const noexcept{return reserved_;}
private:
 PolygonSurfaceTransferLimits limits_;
 size_t reserved_=0,entries_=0;
 std::vector<std::shared_ptr<const PolygonSurfaceOwner>>owners_;
};

// Private per-owning-call reader. No persistent/global cache and no Scene
// permission is minted by constructing it. The eventual Scene preflight must
// additionally charge all encoded pools and enforce earlier-material roots.
class PolygonSurfaceNodeReader final {
public:
 PolygonSurfaceNodeReader(const nlohmann::json &,size_t origin_count,
     PolygonSurfaceOriginReader,const PolygonSurfaceTransferLimits & = {},
     PolygonSurfaceGeometryReader = {});
 const PolygonSurfaceStage &at(uint32_t);
 void require_complete();
 size_t size()const noexcept{return stages_.size();}
 size_t reserved_working_bytes()const noexcept{return reserved_;}
private:
 const nlohmann::json &input_;
 size_t origin_count_=0,reserved_=0;
 PolygonSurfaceOriginReader origin_;
 PolygonSurfaceGeometryReader geometry_;
 PolygonSurfaceTransferLimits limits_;
 std::vector<std::optional<PolygonSurfaceStage>>stages_;
 std::vector<uint8_t>busy_;
};
} // namespace dome::detail
