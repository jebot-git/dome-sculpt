// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace dome {
// Premultiplied, linear FLOAT coefficients. Legacy is relative to the current
// typed material strength; absolute is already resolved radiance. Neither may
// be reconstructed from the ordinary display RGB8 emission image. In
// particular, absolute remains visible when legacy_strength is zero.
struct BakedEmissionSample {
  std::array<float, 3> legacy{}, absolute{};
  float coverage = 0;
  void validate() const;
  std::array<float, 3> resolve(std::array<float, 3> background,
                             float legacy_strength) const;
};

struct BakedEmissionLimits {
  // The existing paint retention budget, not an extra allowance. The owning
  // BakedTextures/PaintedMesh must also charge their other retained channels.
  size_t max_bytes = 128 * 1024 * 1024;
  std::function<bool()> cancelled;
};

// Immutable version-1 square field. All factories copy their input; callers
// cannot retain a mutable alias into a worker/history/render snapshot. The
// standalone Bake grid remains 8..4096, independently of PaintedMesh's native
// 64..2048 grid. Dense fields exceeding the byte budget refuse BEFORE allocation.
class BakedEmissionField final {
public:
  static constexpr uint32_t version = 1;
  static constexpr size_t encoded_header_bytes = 32;
  static size_t required_bytes(uint32_t resolution,
                               const BakedEmissionLimits & = {});
  static std::shared_ptr<const BakedEmissionField>
  create(uint32_t resolution, std::span<const BakedEmissionSample>,
         const BakedEmissionLimits & = {});
  static std::shared_ptr<const BakedEmissionField>
  decode(std::span<const uint8_t>, const BakedEmissionLimits & = {});
  // Complete wire/coefficient validation without allocating retained storage.
  static uint32_t encoded_resolution(std::span<const uint8_t>,
                                     const BakedEmissionLimits & = {});
  std::vector<uint8_t> encode(const BakedEmissionLimits & = {}) const;

  uint32_t resolution() const noexcept { return resolution_; }
  size_t storage_bytes() const noexcept { return values_.size() * 7 * sizeof(float); }
  std::span<const BakedEmissionSample> values() const noexcept { return values_; }
  // Clamp-to-edge texel centers, matching the existing native paint UV domain.
  // Interpolate the coefficients and coverage, never unpremultiplied radiance.
  BakedEmissionSample sample(float u, float v) const;
  static BakedEmissionSample over(const BakedEmissionSample &below,
                                  const BakedEmissionSample &above);

  BakedEmissionField(const BakedEmissionField &) = delete;
  BakedEmissionField &operator=(const BakedEmissionField &) = delete;
private:
  BakedEmissionField(uint32_t r, std::span<const BakedEmissionSample> values)
      : resolution_(r), values_(values.begin(), values.end()) {}
  BakedEmissionField(uint32_t r, std::vector<BakedEmissionSample> &&values)
      : resolution_(r), values_(std::move(values)) {}
  uint32_t resolution_;
  std::vector<BakedEmissionSample> values_;
};
} // namespace dome
