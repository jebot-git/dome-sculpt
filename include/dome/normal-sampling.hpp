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
  // current MToon sampling domain, including proven fixed affine sampling ST.
  std::vector<std::array<float,2>> mtoon_coordinates;
  std::vector<uint32_t> indices;
  std::shared_ptr<const Image> resolved_pbr, resolved_mtoon;
  std::shared_ptr<const NormalDetailAuthority> detail;
  size_t storage_bytes() const;
  void validate(const std::function<bool()> &cancelled = {}) const;
};
struct NormalSamplingArchiveLimits {
  size_t max_domains = 128;
  size_t max_bytes = 128 * 1024 * 1024;
  std::function<bool()> cancelled;
};
class NormalSamplingArchive {
public:
  explicit NormalSamplingArchive(NormalSamplingArchiveLimits = {});
  uint32_t add(std::shared_ptr<const NormalSamplingDomain>);
  const std::shared_ptr<const NormalSamplingDomain> &at(uint32_t) const;
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
};
} // namespace dome
