// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/baked-emission.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
size_t checks = 0;
void check(bool condition, const char *message) {
  ++checks;
  if (!condition) throw std::runtime_error(message);
}
template<class E, class F> void refuses(F f, const char *message) {
  bool caught = false;
  try { f(); } catch (const E &) { caught = true; }
  check(caught, message);
}
void exact(float actual, float expected, const char *message) {
  check(std::bit_cast<uint32_t>(actual) == std::bit_cast<uint32_t>(expected), message);
}
void exact(const dome::BakedEmissionSample &a, const dome::BakedEmissionSample &b) {
  for (size_t k = 0; k < 3; ++k) {
    exact(a.legacy[k], b.legacy[k], "legacy coefficient bit identity");
    exact(a.absolute[k], b.absolute[k], "absolute coefficient bit identity");
  }
  exact(a.coverage, b.coverage, "coverage bit identity");
}
} // namespace
int main() try {
  using namespace dome;
  static_assert(!std::is_copy_constructible_v<BakedEmissionField>);
  static_assert(std::is_same_v<decltype(std::declval<const BakedEmissionField>().values()),
                               std::span<const BakedEmissionSample>>);
  const float largest = std::numeric_limits<float>::max();
  const float tiny = 0x1p-24f; // Far below one UNORM8 step; nonzero HDR residual.
  const BakedEmissionSample value{{.125f, .25f, 0}, {100.f, 50000.f, .0625f}, .5f};
  value.validate();
  auto resolved = value.resolve({8, 10, 12}, 0);
  exact(resolved[0], 104, "absolute at zero strength");
  exact(resolved[1], 50005, "HDR above legacy interchange cap survives");
  exact(resolved[2], 6.0625f, "background uses independent coverage");
  resolved = value.resolve({8, 10, 12}, 4);
  exact(resolved[0], 104.5f, "legacy strength applied once");
  exact(resolved[1], 50006, "nonunit strength retains absolute");
  const BakedEmissionSample seed{{}, {50000, 2, 0}, 1};
  for (float strength : {0.f, .5f, 4.f, largest}) {
    auto result = seed.resolve({largest, largest, largest}, strength);
    for (size_t k = 0; k < 3; ++k) exact(result[k], seed.absolute[k], "resolved seed independent of material factors");
  }
  BakedEmissionSample relative{{1, .5f, 0}, {}, 1};
  exact(relative.resolve({}, 0)[0], 0, "old relative zero strength");
  exact(relative.resolve({}, 4)[1], 2, "old relative factor semantics");
  BakedEmissionSample near_limit{{}, {largest, 0, 0}, 1};
  exact(near_limit.resolve({}, largest)[0], largest, "finite maximum absolute");
  near_limit.legacy[0] = 1;
  refuses<std::overflow_error>([&]{ near_limit.resolve({}, largest); }, "checked addition overflow");
  const BakedEmissionSample below{{}, {largest/2, 0, 0}, .5f};
  const BakedEmissionSample above{{1, 0, 0}, {largest, 0, 0}, 1};
  refuses<std::overflow_error>([&]{ above.resolve({}, largest); }, "finite strength checked composite overflow");
  auto over = BakedEmissionField::over(below, {{}, {largest/2, 0, 0}, .5f});
  exact(over.coverage, .75f, "over coverage");
  check(std::isfinite(over.absolute[0]) && over.absolute[0] > largest/2, "near-limit over multiplication and addition remain finite");
  const BakedEmissionSample tiny_value{{tiny, 0, 0}, {50000*tiny, 0, 0}, tiny};
  tiny_value.validate();
  check(tiny_value.resolve({}, 0)[0] > 0, "tiny FLOAT coverage retains energy");
  refuses<std::invalid_argument>([]{ BakedEmissionSample{{}, {1,0,0}, 0}.validate(); }, "zero coverage cannot hide absolute energy");
  refuses<std::invalid_argument>([]{ BakedEmissionSample{{1,0,0}, {}, 0}.validate(); }, "zero coverage cannot hide relative energy");
  refuses<std::invalid_argument>([]{ BakedEmissionSample{{}, {}, 2}.validate(); }, "coverage range");
  refuses<std::invalid_argument>([&]{ BakedEmissionSample{{}, {largest,0,0}, .5f}.validate(); }, "premultiplied absolute bound");
  for (float invalid : {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
    refuses<std::invalid_argument>([&]{ BakedEmissionSample{{}, {invalid,0,0}, 1}.validate(); }, "invalid absolute coefficient");
    refuses<std::invalid_argument>([&]{ value.resolve({}, invalid); }, "invalid strength");
    refuses<std::invalid_argument>([&]{ value.resolve({invalid,0,0}, 1); }, "invalid background");
  }
  std::vector<BakedEmissionSample> values(64);
  values[0] = value; values[1] = tiny_value; values[8] = relative; values[9] = seed;
  const auto field = BakedEmissionField::create(8, values);
  values[0] = {}; // Public input mutation must not change the immutable owner.
  exact(field->values()[0], value);
  exact(field->sample(.0625f, .0625f), value);
  exact(field->sample(-20, -20), value);
  exact(field->sample(20, 20), {});
  // Independent binary64 reference: midpoint of four PREMULTIPLIED terms.
  auto interpolated = field->sample(.125f, .125f);
  for (size_t k = 0; k < 3; ++k) {
    exact(interpolated.legacy[k], float((double(value.legacy[k])+tiny_value.legacy[k]+relative.legacy[k]+seed.legacy[k])/4), "bilinear premultiplied legacy");
    exact(interpolated.absolute[k], float((double(value.absolute[k])+tiny_value.absolute[k]+relative.absolute[k]+seed.absolute[k])/4), "bilinear premultiplied absolute");
  }
  exact(interpolated.coverage, float((double(value.coverage)+tiny+relative.coverage+seed.coverage)/4), "bilinear FLOAT coverage");
  // An empty texel contributes no unpremultiplied color/energy during filtering.
  values.assign(64, {}); values[0] = {{}, {100000,0,0}, 1};
  auto edge = BakedEmissionField::create(8, values)->sample(.125f, .0625f);
  exact(edge.absolute[0], 50000, "no unpremultiplied transparent-edge halo");
  exact(edge.coverage, .5f, "transparent-edge independent coverage");
  exact(BakedEmissionField::over(value, {}), value);
  exact(BakedEmissionField::over(value, seed), seed);
  auto mixed = BakedEmissionField::over(value, relative);
  exact(mixed, relative);
  auto encoded = field->encode();
  check(encoded.size() == 32+64*28, "exact field wire bytes");
  check(encoded[8] == 1 && encoded[9] == 0 && encoded[12] == 8, "versioned little-endian header");
  // First coefficient .125 = 0x3e000000, not host-endian struct bytes.
  check(encoded[32] == 0 && encoded[33] == 0 && encoded[34] == 0 && encoded[35] == 0x3e, "FLOAT32 LE wire coefficient");
  auto decoded = BakedEmissionField::decode(encoded);
  check(decoded->encode() == encoded, "exact wire roundtrip");
  for (size_t i = 0; i < 64; ++i) exact(decoded->values()[i], field->values()[i]);
  std::fill(encoded.begin(), encoded.end(), 0);
  exact(decoded->values()[0], value); // Input byte mutation cannot alias owner.
  const size_t required = 8*8*28;
  check(BakedEmissionField::required_bytes(8, {required,{}}) == required, "exact retained budget boundary");
  refuses<std::length_error>([&]{ BakedEmissionField::required_bytes(8, {required-1,{}}); }, "one byte below budget");
  check(BakedEmissionField::required_bytes(2048) == size_t(2048)*2048*28, "full native grid remains supported");
  check(BakedEmissionField::required_bytes(2049) == size_t(2049)*2049*28, "standalone grid has no new 2048 cap");
  refuses<std::length_error>([]{ BakedEmissionField::required_bytes(4096); }, "4096 dense field bounded before allocation");
  for (uint32_t r : {0u,7u,4097u,std::numeric_limits<uint32_t>::max()})
    refuses<std::invalid_argument>([&]{ BakedEmissionField::required_bytes(r); }, "invalid grid and overflow input");
  refuses<std::invalid_argument>([]{ BakedEmissionField::required_bytes(8, {0,{}}); }, "invalid zero budget");
  refuses<std::invalid_argument>([]{ BakedEmissionField::required_bytes(8, {128*1024*1024+1,{}}); }, "cannot raise retained budget");
  refuses<std::invalid_argument>([&]{ BakedEmissionField::create(8, std::span<const BakedEmissionSample>(values).first(63)); }, "sample count mismatch");
  auto bytes = field->encode();
  for (size_t size : {size_t(0),size_t(31),bytes.size()-1})
    refuses<std::invalid_argument>([&]{ BakedEmissionField::decode(std::span<const uint8_t>(bytes).first(size)); }, "truncated field");
  for (size_t offset : {size_t(0),size_t(8),size_t(16),size_t(24)}) {
    auto changed = bytes; ++changed[offset];
    refuses<std::invalid_argument>([&]{ BakedEmissionField::decode(changed); }, "malformed magic/version/count/bytes");
  }
  auto extra = bytes; extra.push_back(0);
  refuses<std::invalid_argument>([&]{ BakedEmissionField::decode(extra); }, "trailing wire bytes forbidden");
  auto invalid = bytes;
  // Absolute0 first texel = infinity in LE, verified before retained allocation.
  invalid[44]=0; invalid[45]=0; invalid[46]=0x80; invalid[47]=0x7f;
  refuses<std::invalid_argument>([&]{ BakedEmissionField::decode(invalid); }, "nonfinite encoded coefficient");
  const BakedEmissionLimits cancelled{128*1024*1024,[]{ return true; }};
  refuses<std::runtime_error>([&]{ BakedEmissionField::create(8, values, cancelled); }, "create cancellation");
  refuses<std::runtime_error>([&]{ BakedEmissionField::decode(bytes, cancelled); }, "decode cancellation");
  refuses<std::runtime_error>([&]{ field->encode(cancelled); }, "encode cancellation");
  std::cout << "Baked emission " << checks << " checks PASS\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << "Baked emission FAIL after " << checks << ": " << error.what() << '\n';
  return 1;
}
