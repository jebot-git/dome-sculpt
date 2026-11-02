// SPDX-License-Identifier: GPL-3.0-or-later
#include "softfloat/dome_f64.hpp"
#include <algorithm>
#include <array>
#include <string_view>
#include <bit>
#include <cfenv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#ifdef DOME_TEST_SYCL
#include <sycl/sycl.hpp>
#endif
using namespace dome::softfloat64;
namespace {
uint64_t random_word(uint64_t &state) {
  state ^= state << 13;
  state ^= state >> 7;
  state ^= state << 17;
  return state;
}
bool equal(uint64_t actual, uint64_t expected) {
  return actual == expected || (std::isnan(std::bit_cast<double>(actual)) &&
                               std::isnan(std::bit_cast<double>(expected)));
}
std::array<uint64_t, 4> reference(uint64_t a, uint64_t b) {
  const double x = std::bit_cast<double>(a), y = std::bit_cast<double>(b);
  volatile double sum = x + y, difference = x - y, product = x * y, quotient = x / y;
  return {std::bit_cast<uint64_t>(double(sum)), std::bit_cast<uint64_t>(double(difference)),
          std::bit_cast<uint64_t>(double(product)), std::bit_cast<uint64_t>(double(quotient))};
}
std::array<uint64_t, 4> encoded(uint64_t a, uint64_t b) {
  return {f64_add({a}, {b}).v, f64_sub({a}, {b}).v,
          f64_mul({a}, {b}).v, f64_div({a}, {b}).v};
}
void compare(const auto &actual, const auto &expected, size_t row) {
  for (size_t k = 0; k < expected.size(); ++k) if (!equal(actual[k], expected[k])) {
    std::cerr << "Binary64 mismatch row=" << row << " operation=" << k
              << std::hex << " expected=" << expected[k] << " actual=" << actual[k] << '\n';
    throw std::runtime_error("Opaque integer arithmetic differs from independent native binary64");
  }
}
size_t host() {
  const std::array<uint64_t, 16> edges{0, UINT64_C(0x8000000000000000), 1,
      UINT64_C(0x8000000000000001), UINT64_C(0x000FFFFFFFFFFFFF),
      UINT64_C(0x0010000000000000), UINT64_C(0x7FEFFFFFFFFFFFFF),
      UINT64_C(0xFFEFFFFFFFFFFFFF), UINT64_C(0x3FEFFFFFFFFFFFFF),
      UINT64_C(0x3FF0000000000000), UINT64_C(0x3FF0000000000001),
      UINT64_C(0xBFF0000000000000), UINT64_C(0x4000000000000000),
      UINT64_C(0x3CA0000000000000), UINT64_C(0x3FD5555555555555),
      UINT64_C(0x3FD5555555555556)};
  size_t checks = 0;
  for (auto a : edges) for (auto b : edges) {
    compare(encoded(a, b), reference(a, b), checks / 4);
    checks += 4;
  }
  uint64_t state = UINT64_C(0xDADBDEF123456789);
  for (size_t i = 0; i < 500000; ++i) {
    const auto a = random_word(state), b = random_word(state);
    if (!std::isfinite(std::bit_cast<double>(a)) || !std::isfinite(std::bit_cast<double>(b))) continue;
    compare(encoded(a, b), reference(a, b), checks / 4);
    checks += 4;
    const auto integer = std::bit_cast<int32_t>(uint32_t(a));
    if (i32_to_f64(integer).v != std::bit_cast<uint64_t>(double(integer)))
      throw std::runtime_error("Signed integer to binary64 conversion differs");
  }
  for (int32_t integer : {INT32_MIN, INT32_MIN + 1, -1, 0, 1, INT32_MAX - 1, INT32_MAX})
    if (i32_to_f64(integer).v != std::bit_cast<uint64_t>(double(integer)))
      throw std::runtime_error("Boundary signed integer conversion differs");
  std::cout << "PASS " << checks << " exact binary64 arithmetic results, random/boundary integer conversions\n";
  return checks;
}
#ifdef DOME_TEST_SYCL
void device(bool arc) {
  constexpr size_t count = 250000;
  uint64_t state = UINT64_C(0xDADBDEF123456789);
  std::vector<uint64_t> input(count * 2), expected(count * 5), output(count * 5);
  for (auto &value : input) do { value = random_word(state); }
    while (!std::isfinite(std::bit_cast<double>(value)));
  const auto cpu_begin = std::chrono::steady_clock::now();
  for (size_t i = 0; i < count; ++i) {
    const auto row = reference(input[i * 2], input[i * 2 + 1]);
    std::copy(row.begin(), row.end(), expected.begin() + i * 5);
    expected[i * 5 + 4] = std::bit_cast<uint64_t>(double(std::bit_cast<int32_t>(uint32_t(input[i * 2]))));
  }
  std::cout << "NATIVE_CPU_ARITHMETIC_MS " << std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - cpu_begin).count() << '\n';
  sycl::queue queue{sycl::gpu_selector_v, sycl::property_list{sycl::property::queue::in_order{}}};
  const auto gpu = queue.get_device();
  const auto name = gpu.get_info<sycl::info::device::name>();
  if (arc && (name.find("Intel") == std::string::npos || name.find("Arc") == std::string::npos ||
              name.find("A770") == std::string::npos)) throw std::runtime_error("Strict Arc A770 device is absent");
  std::cout << "DEVICE " << name << " native_fp64=" << gpu.has(sycl::aspect::fp64) << std::endl;
  struct Release {
    sycl::queue &queue;
    uint64_t *input = nullptr, *output = nullptr;
    ~Release() {
      try { queue.wait_and_throw(); } catch (...) {}
      if (input) try { sycl::free(input, queue); } catch (...) {}
      if (output) try { sycl::free(output, queue); } catch (...) {}
    }
  } memory{queue};
  memory.input = sycl::malloc_device<uint64_t>(input.size(), queue);
  memory.output = sycl::malloc_device<uint64_t>(output.size(), queue);
  if (!memory.input || !memory.output) throw std::bad_alloc();
  auto *in = memory.input, *out = memory.output;
  for (int iteration = 0; iteration < 4; ++iteration) {
    const auto begin = std::chrono::steady_clock::now();
    queue.memcpy(in, input.data(), input.size() * sizeof(uint64_t));
    queue.parallel_for(sycl::range<1>{count}, [=](sycl::id<1> id) {
      const auto i = size_t(id);
      const float64_t a{in[i * 2]}, b{in[i * 2 + 1]};
      out[i * 5] = f64_add(a, b).v;
      out[i * 5 + 1] = f64_sub(a, b).v;
      out[i * 5 + 2] = f64_mul(a, b).v;
      out[i * 5 + 3] = f64_div(a, b).v;
      out[i * 5 + 4] = i32_to_f64(int32_t(uint32_t(in[i * 2]))).v;
    });
    queue.memcpy(output.data(), out, output.size() * sizeof(uint64_t));
    queue.wait_and_throw();
    std::cout << "GPU_COPY_KERNEL_READBACK_MS iteration=" << iteration << " value="
              << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count() << '\n';
    compare(output, expected, size_t(iteration));
  }
  std::cout << "PASS 1250000 exact encoded arithmetic/integer results per iteration across four GPU transactions\n";
}
#endif
}
int main(int argc, char **argv) try {
  if (std::fegetround() != FE_TONEAREST) throw std::runtime_error("Nearest-even rounding is required");
  if (argc == 1) { host(); return 0; }
#ifdef DOME_TEST_SYCL
  if (std::string_view(argv[1]) == "--device") { device(argc > 2 && std::string_view(argv[2]) == "--arc"); return 0; }
#endif
  return 2;
} catch (const std::exception &error) {
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
