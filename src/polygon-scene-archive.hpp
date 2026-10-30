// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/creator-archive.hpp"
#include "dome/material-bindings.hpp"
#include "dome/polygon-scene.hpp"
#include "polygon-color-archive.hpp"
#include <set>
namespace dome::detail {
class SourcePlacementArchive;
// Bounded immutable graph archive. Source IDs are data, never object lookups.
class PolygonSceneArchive {
public:
  uint32_t add(std::shared_ptr<const PolygonAuthority>, bool deep = true);
  uint32_t add_origin(std::shared_ptr<const PolygonOrigin>, bool deep = true);
  const std::shared_ptr<const PolygonAuthority> &at(uint32_t) const;
  const auto &authorities() const { return authorities_; }
  const auto &origins() const { return origins_; }
  const auto &volumes() const { return volumes_; }
  const auto &paints() const { return paints_; }
  nlohmann::json encode(PaintImageArchive &, CreatorAssetArchive &,
                        SourceMaterialArchive &, SourcePlacementArchive * = nullptr) const;
  static void preflight(const nlohmann::json &, const nlohmann::json &objects);
  static PolygonSceneArchive decode(const nlohmann::json &,
                                    const PaintImageArchive &,
                                    const CreatorAssetArchive &,
                                    const SourceMaterialArchive &,
                                    const SourcePlacementArchive * = nullptr);

private:
  size_t entries_ = 0;
  PolygonColorArchive colors_;
  std::set<const PolygonAuthority *> deeply_validated_;
  std::set<const PolygonOrigin *> deeply_validated_origins_;
  bool direct_origins_ = false;
  void collect(std::shared_ptr<const PolygonAuthority>,
               std::shared_ptr<const PolygonOrigin>, bool deep);
  std::vector<std::shared_ptr<const PolygonAuthority>> authorities_;
  std::vector<std::shared_ptr<const PolygonOrigin>> origins_;
  std::vector<std::shared_ptr<const Volume>> volumes_;
  std::vector<std::shared_ptr<const PaintedMesh>> paints_;
};
uint32_t polygon_archive_index(const nlohmann::json &, size_t count);
} // namespace dome::detail
