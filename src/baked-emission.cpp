// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/baked-emission.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace dome {
namespace {
constexpr size_t retained_limit = 128 * 1024 * 1024;
constexpr std::array<uint8_t, 8> magic{'D','O','M','E','E','M','I','T'};
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(BakedEmissionSample) == 7 * sizeof(float));
void stop(const BakedEmissionLimits &limits) {
  if (limits.cancelled && limits.cancelled())
    throw std::runtime_error("Baked emission cancelled");
}
// Round the mathematical bound to the stored FLOAT domain. This permits only
// its unavoidable final half-ULP rounding, not an arbitrary epsilon/clamp.
float absolute_bound(float coverage) {
  return float(double(std::numeric_limits<float>::max()) * coverage);
}
void put(std::vector<uint8_t> &out, size_t &offset, uint64_t value, size_t bytes) {
  for (size_t i = 0; i < bytes; ++i) out[offset++] = uint8_t(value >> (8 * i));
}
uint64_t get(std::span<const uint8_t> in, size_t &offset, size_t bytes) {
  uint64_t value = 0;
  for (size_t i = 0; i < bytes; ++i) value |= uint64_t(in[offset++]) << (8 * i);
  return value;
}
template<class F> void components(const BakedEmissionSample &v, F f) {
  for (float x : v.legacy) f(x);
  for (float x : v.absolute) f(x);
  f(v.coverage);
}
BakedEmissionSample read(std::span<const uint8_t> bytes, size_t &offset) {
  BakedEmissionSample result;
  for (auto &v : result.legacy) v = std::bit_cast<float>(uint32_t(get(bytes, offset, 4)));
  for (auto &v : result.absolute) v = std::bit_cast<float>(uint32_t(get(bytes, offset, 4)));
  result.coverage = std::bit_cast<float>(uint32_t(get(bytes, offset, 4)));
  return result;
}
BakedEmissionSample weighted(std::span<const BakedEmissionSample> values,
                             std::span<const double> weights) {
  double coverage = 0;
  std::array<double, 3> legacy{}, absolute{};
  for (size_t i = 0; i < values.size(); ++i) {
    coverage += double(values[i].coverage) * weights[i];
    for (size_t k = 0; k < 3; ++k) {
      legacy[k] += double(values[i].legacy[k]) * weights[i];
      absolute[k] += double(values[i].absolute[k]) * weights[i];
    }
  }
  BakedEmissionSample result;
  result.coverage = float(coverage);
  for (size_t k = 0; k < 3; ++k) {
    result.legacy[k] = float(legacy[k]);
    result.absolute[k] = float(absolute[k]);
  }
  // At the subnormal boundary FLOAT coverage can underflow even though an HDR
  // coefficient remains representable. Refuse that unrepresentable result;
  // never quietly replace it by zero or manufacture coverage.
  result.validate();
  return result;
}
} // namespace

void BakedEmissionSample::validate() const {
  if (!std::isfinite(coverage) || coverage < 0 || coverage > 1)
    throw std::invalid_argument("Invalid baked emission coverage");
  const float bound = absolute_bound(coverage);
  for (size_t k = 0; k < 3; ++k) {
    if (!std::isfinite(legacy[k]) || legacy[k] < 0 || legacy[k] > coverage ||
        !std::isfinite(absolute[k]) || absolute[k] < 0 || absolute[k] > bound)
      throw std::invalid_argument("Invalid premultiplied baked emission coefficient");
  }
}
std::array<float, 3> BakedEmissionSample::resolve(std::array<float, 3> background,
                                                float strength) const {
  validate();
  if (!std::isfinite(strength) || strength < 0)
    throw std::invalid_argument("Invalid baked emission legacy strength");
  std::array<float, 3> result;
  for (size_t k = 0; k < 3; ++k) {
    if (!std::isfinite(background[k]) || background[k] < 0)
      throw std::invalid_argument("Invalid baked emission background");
    const double value = double(background[k]) * (1 - double(coverage)) +
                         double(legacy[k]) * strength + absolute[k];
    if (value > std::numeric_limits<float>::max())
      throw std::overflow_error("Baked emission radiance exceeds FLOAT range");
    result[k] = float(value);
  }
  return result;
}
size_t BakedEmissionField::required_bytes(uint32_t resolution,
                                         const BakedEmissionLimits &limits) {
  if (!limits.max_bytes || limits.max_bytes > retained_limit)
    throw std::invalid_argument("Invalid baked emission byte budget");
  stop(limits);
  if (resolution < 8 || resolution > 4096)
    throw std::invalid_argument("Invalid baked emission grid");
  const uint64_t count = uint64_t(resolution) * resolution;
  const uint64_t bytes = count * 7 * sizeof(float);
  if (bytes > limits.max_bytes || bytes > std::numeric_limits<size_t>::max())
    throw std::length_error("Baked emission exceeds retained byte budget");
  return size_t(bytes);
}
std::shared_ptr<const BakedEmissionField>
BakedEmissionField::create(uint32_t r, std::span<const BakedEmissionSample> values,
                            const BakedEmissionLimits &limits) {
  const size_t bytes = required_bytes(r, limits);
  if (values.size() != bytes / (7 * sizeof(float)))
    throw std::invalid_argument("Baked emission grid/sample count mismatch");
  for (size_t i = 0; i < values.size(); ++i) {
    if ((i & 4095) == 0) stop(limits);
    values[i].validate();
  }
  stop(limits);
  auto result = std::shared_ptr<const BakedEmissionField>(new BakedEmissionField(r, values));
  stop(limits);
  return result;
}
BakedEmissionSample BakedEmissionField::sample(float u, float v) const {
  if (!std::isfinite(u) || !std::isfinite(v))
    throw std::invalid_argument("Invalid baked emission UV");
  const double x = std::clamp(double(u) * resolution_ - .5, 0., double(resolution_ - 1));
  const double y = std::clamp(double(v) * resolution_ - .5, 0., double(resolution_ - 1));
  const uint32_t x0 = uint32_t(x), y0 = uint32_t(y);
  const uint32_t x1 = std::min(x0 + 1, resolution_ - 1), y1 = std::min(y0 + 1, resolution_ - 1);
  const double a = x - x0, b = y - y0;
  const std::array<BakedEmissionSample, 4> values{
      values_[size_t(y0)*resolution_+x0], values_[size_t(y0)*resolution_+x1],
      values_[size_t(y1)*resolution_+x0], values_[size_t(y1)*resolution_+x1]};
  const std::array<double, 4> weights{(1-a)*(1-b), a*(1-b), (1-a)*b, a*b};
  return weighted(values, weights);
}
BakedEmissionSample BakedEmissionField::over(const BakedEmissionSample &below,
                                             const BakedEmissionSample &above) {
  below.validate(); above.validate();
  const std::array<BakedEmissionSample, 2> values{above, below};
  const std::array<double, 2> weights{1., 1. - double(above.coverage)};
  return weighted(values, weights);
}
std::vector<uint8_t> BakedEmissionField::encode(const BakedEmissionLimits &limits) const {
  const size_t bytes = required_bytes(resolution_, limits);
  std::vector<uint8_t> out(encoded_header_bytes + bytes);
  std::copy(magic.begin(), magic.end(), out.begin());
  size_t offset = magic.size();
  put(out, offset, version, 4); put(out, offset, resolution_, 4);
  put(out, offset, values_.size(), 8); put(out, offset, bytes, 8);
  for (size_t i = 0; i < values_.size(); ++i) {
    if ((i & 4095) == 0) stop(limits);
    components(values_[i], [&](float v) { put(out, offset, std::bit_cast<uint32_t>(v), 4); });
  }
  stop(limits);
  return out;
}
uint32_t BakedEmissionField::encoded_resolution(std::span<const uint8_t> bytes,
                                                const BakedEmissionLimits &limits) {
  stop(limits);
  if (bytes.size() < encoded_header_bytes || !std::equal(magic.begin(), magic.end(), bytes.begin()))
    throw std::invalid_argument("Invalid baked emission field header");
  size_t offset = magic.size();
  if (get(bytes, offset, 4) != version)
    throw std::invalid_argument("Unsupported baked emission field version");
  const uint32_t r = uint32_t(get(bytes, offset, 4));
  const size_t retained = required_bytes(r, limits);
  if (get(bytes, offset, 8) != uint64_t(r)*r || get(bytes, offset, 8) != retained ||
      bytes.size() != encoded_header_bytes + retained)
    throw std::invalid_argument("Baked emission encoded size mismatch");
  // Reject malformed coefficients before allocating retained storage.
  for (size_t i = 0; i < size_t(r)*r; ++i) {
    if ((i & 4095) == 0) stop(limits);
    read(bytes, offset).validate();
  }
  stop(limits);
  return r;
}
std::shared_ptr<const BakedEmissionField>
BakedEmissionField::decode(std::span<const uint8_t> bytes, const BakedEmissionLimits &limits) {
  const uint32_t r = encoded_resolution(bytes, limits);
  std::vector<BakedEmissionSample> values(size_t(r)*r);
  size_t offset = encoded_header_bytes;
  for (size_t i = 0; i < values.size(); ++i) {
    if ((i & 4095) == 0) stop(limits);
    values[i] = read(bytes, offset);
  }
  stop(limits);
  // This vector was created here, never exposed to a caller. Moving it avoids
  // a second retained-size allocation without admitting any mutable alias.
  auto result = std::shared_ptr<const BakedEmissionField>(new BakedEmissionField(r, std::move(values)));
  stop(limits);
  return result;
}
} // namespace dome
