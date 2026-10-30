// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "humanoid-creator.hpp"
#include "source-maps.hpp"
namespace dome {
// Immutable model-wide authority, independent of any particular primitive,
// material or native vertex remap. Source hierarchy nodes can retain it without
// inventing a primitive appearance or copying a large source_vertices array.
struct CreatorSourceModel {
  std::shared_ptr<const std::vector<uint8_t>> original_bytes;
  std::shared_ptr<const ImportSourceReceipt> source_receipt;
  std::shared_ptr<const ImportedAsset> asset;
  std::shared_ptr<const std::vector<std::shared_ptr<const Image>>> decoded_images;
  std::shared_ptr<const std::string> vrm_json;
  CreatorSourceModel()=default;
  explicit CreatorSourceModel(const CreatorAppearance &a)
      :original_bytes(a.original_bytes),source_receipt(a.source_receipt),asset(a.asset),
       decoded_images(a.decoded_images),vrm_json(a.vrm_json){}
};
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
  uint32_t add_model(const CreatorSourceModel &, PaintImageArchive &);
  std::shared_ptr<const CreatorSourceModel> model(uint32_t) const;
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
  uint32_t add_source(const CreatorSourceModel &,const CreatorAppearance *,PaintImageArchive &);
  friend nlohmann::json encode_creator_appearance(const CreatorAppearance &, CreatorAssetArchive &, PaintImageArchive &);
  friend std::shared_ptr<const CreatorAppearance> decode_creator_appearance(const nlohmann::json &, const CreatorAssetArchive &);
};
nlohmann::json encode_creator_appearance(const CreatorAppearance &, CreatorAssetArchive &, PaintImageArchive &);
std::shared_ptr<const CreatorAppearance> decode_creator_appearance(const nlohmann::json &, const CreatorAssetArchive &);
} // namespace dome
