// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint-grid-filter-kernel.hpp"
#include <bit>

namespace dome::detail::surface_ray {
// Use the existing integer binary64 implementation for GPUs without native
// FP64. All inputs originate in binary32 and remain finite in the ray metric.
class SoftScalar {
  static constexpr uint64_t sign=uint64_t(1)<<63;
  static constexpr uint64_t hidden=uint64_t(1)<<52;
  uint64_t bits_;

  // The paint-grid quotient handles normal inputs and outputs. Preserve
  // the same nearest-even division if an extreme ray intermediate enters
  // the subnormal or overflow range.
  static uint64_t divide_extreme(uint64_t a,uint64_t b,int ea,int eb){
    uint64_t numerator=a&(hidden-1),denominator=b&(hidden-1);
    if(!ea){
      const auto normalized=softfloat64::softfloat_normSubnormalF64Sig(numerator);
      ea=normalized.exp;numerator=normalized.sig;
    }
    if(!eb){
      const auto normalized=softfloat64::softfloat_normSubnormalF64Sig(denominator);
      eb=normalized.exp;denominator=normalized.sig;
    }
    numerator|=hidden;
    denominator|=hidden;
    int exponent=ea-eb+1023;
    if(numerator<denominator){numerator<<=1;--exponent;}
    if(exponent>=2047)return UINT64_C(0x7ff0000000000000);
    if(exponent<-52)return 0;
    if(exponent==-52)return numerator>denominator?1:0;
    uint64_t quotient=1,remainder=numerator-denominator;
    const int fractional_bits=exponent>0?52:51+exponent;
    for(int i=0;i<fractional_bits;++i){
      remainder<<=1;
      quotient<<=1;
      if(remainder>=denominator){remainder-=denominator;++quotient;}
    }
    const auto complement=denominator-remainder;
    quotient+=remainder>complement||(remainder==complement&&(quotient&1));
    return exponent>0?(uint64_t(exponent-1)<<52)+quotient:quotient;
  }
public:
  constexpr SoftScalar() : bits_(0) {}
  explicit SoftScalar(float value):bits_(paint_grid::widen_float_bits(std::bit_cast<uint32_t>(value))){}
  static constexpr SoftScalar from_bits(uint64_t bits) {
    SoftScalar value;
    value.bits_=bits;
    return value;
  }
  uint64_t bits() const {return bits_;}
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
    // Use the fixed-width quotient for the finite ray domain. The Arc
    // compiler can lose low bits in SoftFloat's reciprocal-estimate path.
    const uint64_t a=bits_&~sign,b=value.bits_&~sign,result_sign=(bits_^value.bits_)&sign;
    const int ea=int(a>>52),eb=int(b>>52),exponent=ea-eb+1023;
    if(!b||ea==2047||eb==2047)return from_bits(softfloat64::f64_div({bits_},{value.bits_}).v);
    if(!a)return from_bits(result_sign);
    if(ea>0&&eb>0&&exponent>1&&exponent<2047)
      return from_bits(paint_grid::div(a,b)|result_sign);
    return from_bits(divide_extreme(a,b,ea,eb)|result_sign);
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
  static Scalar from_bits(uint64_t bits){return Scalar::from_bits(bits);}
  static float narrow(Scalar value){return std::bit_cast<float>(paint_grid::narrow_float_bits(value.bits()));}
};
} // namespace dome::detail::surface_ray
