// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint.hpp"
#include "source-maps.hpp"
#include <functional>

namespace dome {
// Structural data only. Source authentication belongs to the owning replay
// recipe; decoding this geometry-only pool grants no editing authority.
struct NormalDetailAuthority {
  uint32_t version = 1;
  BakedNormalInterpretation interpretation = BakedNormalInterpretation::PreparedAttributes;
  std::shared_ptr<const Image> pbr, mtoon;
  std::vector<uint8_t> coverage;
  void validate(uint32_t resolution) const;
};
struct NormalSamplingDomain {
  uint32_t version = 1, resolution = 0;
  std::vector<std::array<float,2>> coordinates;
  // Empty aliases the PBR coordinates. Otherwise this is an independent
  // current MToon source-coordinate domain. The captured sampler transform is
  // applied separately, in the original fragment operation order.
  std::vector<std::array<float,2>> mtoon_coordinates;
  std::vector<uint32_t> indices;
  std::shared_ptr<const Image> resolved_pbr, resolved_mtoon;
  std::shared_ptr<const NormalDetailAuthority> detail;
  size_t storage_bytes() const;
  void validate(const std::function<bool()> &cancelled = {}) const;
  void validate(bool geometry,const std::function<bool()> &cancelled = {}) const;
};
struct NormalSamplingArchiveLimits {
  size_t max_domains = 128;
  size_t max_bytes = 128 * 1024 * 1024;
  size_t max_details = 256;
  std::function<bool()> cancelled;
};
class Scene;
// A paint leaf cannot authenticate retained sampling by itself. This owning
// envelope retains the complete bounded native context, including source and
// authoring histories; object identifies the surface within that context.
nlohmann::json encode_normal_surface(const Scene &,uint64_t object,
    const std::function<bool()> &cancelled = {});
Scene decode_normal_surface(const nlohmann::json &,
    const std::function<bool()> &cancelled = {});
class NormalSamplingArchive {
public:
  explicit NormalSamplingArchive(NormalSamplingArchiveLimits = {});
  uint32_t add(std::shared_ptr<const NormalSamplingDomain>);
  uint32_t add_detail(std::shared_ptr<const NormalDetailAuthority>);
  const std::shared_ptr<const NormalSamplingDomain> &at(uint32_t) const;
  const std::shared_ptr<const NormalDetailAuthority> &detail_at(uint32_t) const;
  size_t size() const noexcept { return domains_.size(); }
  size_t bytes() const noexcept { return bytes_; }
  void require_reachable() const;
  nlohmann::json encode(PaintImageArchive &) const;
  static NormalSamplingArchive decode(const nlohmann::json &,
      const PaintImageArchive &, NormalSamplingArchiveLimits = {});
private:
  NormalSamplingArchiveLimits limits_;
  size_t bytes_ = 0;
  std::vector<std::shared_ptr<const NormalSamplingDomain>> domains_;
  mutable std::vector<bool> referenced_;
  std::vector<std::shared_ptr<const NormalDetailAuthority>> details_;
  mutable std::vector<bool> detail_referenced_;
  uint32_t insert_detail(std::shared_ptr<const NormalDetailAuthority>);
};
} // namespace dome
