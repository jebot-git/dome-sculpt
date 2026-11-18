// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <openxr/openxr.h>
namespace dome::test {
// OpenXR time is signed nanoseconds. Widen BEFORE both arithmetic operations;
// unsigned32 arithmetic wraps at physical frame184, before assignment to XrTime.
constexpr XrTime xr_fixture_time(uint64_t physical_frame) noexcept {
  return XrTime(1'000'000'000) + XrTime(physical_frame) * XrTime(18'000'000);
}
static_assert(xr_fixture_time(183)==4'294'000'000LL);
static_assert(xr_fixture_time(184)==4'312'000'000LL);
static_assert(xr_fixture_time(318)==6'724'000'000LL);
} // namespace dome::test
