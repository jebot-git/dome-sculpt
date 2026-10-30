// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "brush.hpp"
#include <functional>
#include <type_traits>
namespace dome {
enum class CsgOperation : uint32_t { Union, Difference };
struct CsgBox {
  Vec3 minimum{}, maximum{};
  float rounding = 0;
  CsgOperation operation = CsgOperation::Union;
};
static_assert(sizeof(CsgBox) == 32 && std::is_trivially_copyable_v<CsgBox>);
// Ordered rounded-box composition followed by ONE protection blend against
// the original field. Only Sample::distance changes, after the entire call
// succeeds. Empty input/operations create no device queue. GPU backends retain
// their explicit device selection and never substitute CPU evaluation.
void apply_csg(std::span<Sample>, std::span<const CsgBox>, float band,
               const std::function<bool()> &cancelled = {});
} // namespace dome
