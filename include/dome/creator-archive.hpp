// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "humanoid-creator.hpp"
#include "source-maps.hpp"
namespace dome {
struct CreatorArchiveLimits {
  size_t max_assets = 16;
  size_t max_asset_bytes = 64 * 1024 * 1024;
  size_t max_total_bytes = 128 * 1024 * 1024;
  size_t max_json_bytes = 8 * 1024 * 1024;
  size_t max_remap_vertices = 2'000'000;
  ImportLimits import_limits{};
  std::function<bool()> cancelled;
};
class CreatorAssetArchive {
public:
  explicit CreatorAssetArchive(CreatorArchiveLimits = {});
  uint32_t add(const CreatorAppearance &, PaintImageArchive &);
  size_t size() const noexcept { return entries_.size(); }
  size_t bytes() const noexcept { return bytes_; }
  nlohmann::json encode() const;
  static CreatorAssetArchive decode(const nlohmann::json &, const PaintImageArchive &,
                                    CreatorArchiveLimits = {});
private:
  struct Entry;
  CreatorArchiveLimits limits_;
  size_t bytes_ = 0;
  std::vector<std::shared_ptr<const Entry>> entries_;
  friend nlohmann::json encode_creator_appearance(const CreatorAppearance &, CreatorAssetArchive &, PaintImageArchive &);
  friend std::shared_ptr<const CreatorAppearance> decode_creator_appearance(const nlohmann::json &, const CreatorAssetArchive &);
};
nlohmann::json encode_creator_appearance(const CreatorAppearance &, CreatorAssetArchive &, PaintImageArchive &);
std::shared_ptr<const CreatorAppearance> decode_creator_appearance(const nlohmann::json &, const CreatorAssetArchive &);
} // namespace dome
