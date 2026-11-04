// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "canonical-normal-owner.hpp"

namespace dome::detail {
struct BakedNormalFieldOptions {
  size_t max_working_bytes=256*1024*1024;
  size_t max_retained_bytes=128*1024*1024;
  uint32_t max_replacements=128;
  std::function<bool()> cancelled;
};

// A resolved *baked* role, separate from PaintedMesh's intrinsic normal planes.
// Its complete grid includes the old intrinsic contribution at conversion;
// normal coverage is therefore full even when material coverage is partial.
// Clearing the active role must release this field, not its durable origin.
// This leaf grants no geometry/UV/key authority and is not yet a Scene codec.
class BakedNormalField {
public:
  static std::shared_ptr<const BakedNormalField> from_canonical(
      std::shared_ptr<const CanonicalNormalConversion>,const BakedNormalFieldOptions & = {});
  static std::shared_ptr<const BakedNormalField> from_intrinsic(
      std::shared_ptr<const CanonicalNormalConversion>,const BakedNormalFieldOptions & = {});
  std::shared_ptr<const BakedNormalField> replace(
      const BakedTextures &,const BakedNormalFieldOptions & = {}) const;
  const auto &origin() const noexcept {return origin_;}
  const auto &prior() const noexcept {return prior_;}
  const auto &pbr() const noexcept {return pbr_;}
  const auto &mtoon() const noexcept {return mtoon_;}
  const auto &normal_coverage() const noexcept {return coverage_;}
  uint32_t resolution() const noexcept {return pbr_->width;}
  uint32_t replacements() const noexcept {return replacements_;}
  bool intrinsic_baseline() const noexcept {return intrinsic_baseline_;}
  size_t retained_bytes() const noexcept {return retained_bytes_;}
  void validate() const;
private:
  friend class BakedNormalArchive;
  std::shared_ptr<const BakedNormalField> replace_planes(const Image &,const Image &,
      std::span<const uint8_t>,const BakedNormalFieldOptions &) const;
  BakedNormalField()=default;
  BakedNormalField(const BakedNormalField&)=delete;
  BakedNormalField &operator=(const BakedNormalField&)=delete;
  std::weak_ptr<const BakedNormalField> self_;
  std::shared_ptr<const CanonicalNormalConversion> origin_;
  std::shared_ptr<const BakedNormalField> prior_;
  std::shared_ptr<const Image> pbr_,mtoon_;
  std::vector<uint8_t> coverage_;
  // Exact detached operation input retained for an eventual replaying codec.
  std::shared_ptr<const Image> write_pbr_,write_mtoon_;
  std::vector<uint8_t> write_coverage_;
  uint32_t replacements_=0;
  bool intrinsic_baseline_=false;
  size_t retained_bytes_=0;
};
} // namespace dome::detail
