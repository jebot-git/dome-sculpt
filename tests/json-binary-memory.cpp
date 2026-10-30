// SPDX-License-Identifier: GPL-3.0-or-later
// A real completed factory result must remain destructible if allocation fails.
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
#include <stdexcept>
#ifdef NDEBUG
#error This allocation-unwind regression requires JSON invariants enabled.
#endif
namespace {
thread_local bool denying = false, hit = false;
thread_local size_t allocations = 0, boundary = SIZE_MAX;
void allocation_check() {
  if (denying && ++allocations >= boundary) {
    hit = true;
    throw std::bad_alloc();
  }
}
using Json = nlohmann::json;
using Bytes = Json::binary_t::container_type;
Json make(unsigned mode, Bytes &bytes) {
  switch (mode) {
  case 0:
    return Json::binary(static_cast<const Bytes &>(bytes));
  case 1:
    return Json::binary(static_cast<const Bytes &>(bytes), 37);
  case 2:
    return Json::binary(std::move(bytes));
  default:
    return Json::binary(std::move(bytes), 37);
  }
}
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void verify(const Json &result, unsigned mode, const Bytes &expected) {
  require(result.is_binary() &&
              static_cast<const Bytes &>(result.get_binary()) == expected,
          "Every factory retains exact binary bytes");
  require(result.get_binary().has_subtype() == bool(mode & 1),
          "Each overload preserves its explicit subtype policy");
  if (mode & 1)
    require(result.get_binary().subtype() == 37,
            "Binary subtype remains exact");
  else
    require(Json::to_cbor(result) == Bytes({0x45, 0, 1, 127, 128, 255}),
            "Ordinary binary CBOR bytes remain unchanged");
}
} // namespace
[[gnu::noinline]] void *operator new(size_t size) {
  allocation_check();
  if (auto value = std::malloc(size ? size : 1))
    return value;
  throw std::bad_alloc();
}
[[gnu::noinline]] void *operator new[](size_t size) {
  return ::operator new(size);
}
[[gnu::noinline]] void operator delete(void *value) noexcept {
  std::free(value);
}
[[gnu::noinline]] void operator delete[](void *value) noexcept {
  std::free(value);
}
[[gnu::noinline]] void operator delete(void *value, size_t) noexcept {
  std::free(value);
}
[[gnu::noinline]] void operator delete[](void *value, size_t) noexcept {
  std::free(value);
}
int main() try {
  const Bytes original{0, 1, 127, 128, 255};
  size_t total = 0;
  for (unsigned mode = 0; mode < 4; ++mode) {
    Bytes input = original;
    allocations = 0;
    boundary = SIZE_MAX;
    denying = true;
    {
      auto result = make(mode, input);
    }
    denying = false;
    const auto measured = allocations;
    require(measured > 0, "Every factory exposes a real allocation boundary");
    for (size_t at = 1; at <= measured; ++at) {
      input = original;
      allocations = 0;
      boundary = at;
      hit = false;
      denying = true;
      bool caught = false;
      try {
        auto result = make(mode, input);
      } catch (const std::bad_alloc &) {
        caught = true;
      }
      denying = false;
      require(hit && caught,
              "Every measured permanent denial is reached and unwinds");
      if (mode < 2)
        require(input == original,
                "Const factory refusal preserves its input exactly");
      input = original;
      verify(make(mode, input), mode, original);
    }
    input = original;
    verify(make(mode, input), mode, original);
    total += measured;
    std::cout << "PASS binary factory " << mode << ": " << measured
              << " real permanent allocation denials, exact retry and bytes\n";
  }
  std::cout
      << total
      << " assertion-enabled binary factory allocation boundaries passed\n";
  return 0;
} catch (const std::exception &error) {
  denying = false;
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
