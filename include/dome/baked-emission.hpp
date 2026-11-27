// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
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

// Immutable square field. Version 1 is the original dense layout. Version 2
// stores exact shared coefficients and, when possible, binary FLOAT coverage.
// Generators retain the dense layout whenever it fits the requested budget.
class BakedEmissionField final {
public:
  static constexpr uint32_t version = 1;
  static constexpr size_t encoded_header_bytes = 32;
  struct EncodedSize { uint32_t resolution; size_t retained_bytes; };
  struct StorageView {
    uint32_t varying_mask=0;
    std::array<float,7> constants{};
    std::span<const BakedEmissionSample> dense;
    std::span<const float> varying;
    std::span<const uint32_t> binary_coverage;
  };
  static size_t required_bytes(uint32_t resolution,
                               const BakedEmissionLimits & = {});
  static std::shared_ptr<const BakedEmissionField>
  create(uint32_t resolution, std::span<const BakedEmissionSample>,
         const BakedEmissionLimits & = {});
  // The indexed generator is repeatable and can be called twice for a packed
  // field. This lets Bake and merge avoid staging a full dense sample array.
  static std::shared_ptr<const BakedEmissionField>
  generate(uint32_t resolution,const std::function<BakedEmissionSample(size_t)> &,
           const BakedEmissionLimits & = {});
  // Material members prefer the smaller exact layout before reserving their
  // shared Scene archive. A genuinely dense field can still use version 1.
  static std::shared_ptr<const BakedEmissionField>
  generate_compact(uint32_t resolution,const std::function<BakedEmissionSample(size_t)> &,
                   const BakedEmissionLimits & = {});
  static std::shared_ptr<const BakedEmissionField>
  decode(std::span<const uint8_t>, const BakedEmissionLimits & = {});
  // Complete wire/coefficient validation without allocating retained storage.
  static uint32_t encoded_resolution(std::span<const uint8_t>,
                                     const BakedEmissionLimits & = {});
  static EncodedSize inspect_encoded(std::span<const uint8_t>,
                                     const BakedEmissionLimits & = {});
  std::vector<uint8_t> encode(const BakedEmissionLimits & = {}) const;

  uint32_t resolution() const noexcept { return resolution_; }
  size_t storage_bytes() const noexcept;
  size_t sample_count() const noexcept { return size_t(resolution_)*resolution_; }
  BakedEmissionSample texel(size_t) const;
  StorageView storage() const noexcept;
  class Samples {
  public:
    struct Iterator {
      using iterator_category=std::forward_iterator_tag;
      using value_type=BakedEmissionSample;
      using difference_type=std::ptrdiff_t;
      using reference=value_type;
      using pointer=void;
      const BakedEmissionField *field=nullptr;
      size_t at=0;
      value_type operator*() const { return field->texel(at); }
      Iterator &operator++(){ ++at;return *this; }
      Iterator operator++(int){ auto prior=*this;++*this;return prior; }
      bool operator==(const Iterator &) const=default;
    };
    explicit Samples(const BakedEmissionField &field):field_(field){}
    size_t size() const noexcept { return field_.sample_count(); }
    BakedEmissionSample operator[](size_t i) const { return field_.texel(i); }
    Iterator begin() const noexcept {return {&field_,0};}
    Iterator end() const noexcept {return {&field_,size()};}
  private:
    const BakedEmissionField &field_;
  };
  Samples values() const noexcept { return Samples(*this); }
  // Clamp-to-edge texel centers, matching the existing native paint UV domain.
  // Interpolate the coefficients and coverage, never unpremultiplied radiance.
  BakedEmissionSample sample(float u, float v) const;
  static BakedEmissionSample over(const BakedEmissionSample &below,
                                  const BakedEmissionSample &above);

  BakedEmissionField(const BakedEmissionField &) = delete;
  BakedEmissionField &operator=(const BakedEmissionField &) = delete;
private:
  static std::shared_ptr<const BakedEmissionField>
  generate_storage(uint32_t,const std::function<BakedEmissionSample(size_t)> &,
                   const BakedEmissionLimits &,bool prefer_compact);
  explicit BakedEmissionField(uint32_t r):resolution_(r){}
  BakedEmissionField(uint32_t r, std::span<const BakedEmissionSample> values)
      : resolution_(r), values_(values.begin(), values.end()) {}
  BakedEmissionField(uint32_t r, std::vector<BakedEmissionSample> &&values)
      : resolution_(r), values_(std::move(values)) {}
  uint32_t resolution_;
  std::vector<BakedEmissionSample> values_;
  uint32_t varying_mask_=0;
  std::array<float,7> constants_{};
  std::vector<float> varying_;
  std::vector<uint32_t> coverage_words_;
};
} // namespace dome
