// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint-grid-filter-kernel.hpp"
#include <bit>

namespace dome::detail::surface_ray {
// Use the existing integer binary64 implementation for GPUs without native
// FP64. All inputs originate in binary32 and remain finite in the ray metric.
class SoftScalar {
  static constexpr uint64_t sign=uint64_t(1)<<63;
  uint64_t bits_;
public:
  constexpr SoftScalar() : bits_(0) {}
  explicit SoftScalar(float value):bits_(paint_grid::widen_float_bits(std::bit_cast<uint32_t>(value))){}
  static constexpr SoftScalar from_bits(uint64_t bits) {
    SoftScalar value;
    value.bits_=bits;
    return value;
  }
  SoftScalar operator-() const {return from_bits(bits_^sign);}
  [[clang::noinline]] SoftScalar operator+(SoftScalar value) const {
    return from_bits(softfloat64::f64_add({bits_},{value.bits_}).v);
  }
  [[clang::noinline]] SoftScalar operator-(SoftScalar value) const {
    return from_bits(softfloat64::f64_sub({bits_},{value.bits_}).v);
  }
  [[clang::noinline]] SoftScalar operator*(SoftScalar value) const {
    return from_bits(softfloat64::f64_mul({bits_},{value.bits_}).v);
  }
  [[clang::noinline]] SoftScalar operator/(SoftScalar value) const {
    return from_bits(softfloat64::f64_div({bits_},{value.bits_}).v);
  }
  bool operator==(SoftScalar value) const {
    return bits_==value.bits_||!((bits_|value.bits_)&~sign);
  }
  bool operator<(SoftScalar value) const {
    if(*this==value)return false;
    const bool negative=bits_&sign;
    if((bits_^value.bits_)&sign)return negative;
    return negative?bits_>value.bits_:bits_<value.bits_;
  }
  bool operator>(SoftScalar value) const {return value<*this;}
  bool operator<=(SoftScalar value) const {return !(value<*this);}
  bool operator>=(SoftScalar value) const {return !(*this<value);}
};
struct SoftPrecision {
  using Scalar=SoftScalar;
  static constexpr Scalar finite_max=Scalar::from_bits(UINT64_C(0x7fefffffffffffff));
};
} // namespace dome::detail::surface_ray
