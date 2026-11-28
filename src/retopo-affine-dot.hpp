// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <bit>
#include <cstdint>

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_AFFINE_DOT __host__ __device__ inline
#else
#define DOME_AFFINE_DOT inline
#endif

namespace dome::detail::retopo_surface {
// The authored affine direction transforms each product and sum at the
// x86 long-double precision, then rounds the row once to binary64.
struct Extended {
  static constexpr uint64_t leading=UINT64_C(0x8000000000000000);
  static constexpr uint64_t fraction=UINT64_C(0x000fffffffffffff);
  static constexpr uint64_t infinity=UINT64_C(0x7ff0000000000000);
  uint64_t significand=0;
  int exponent=0;
  bool negative=false;

  DOME_AFFINE_DOT static int zeros(uint64_t value){
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
    return __clzll(value);
#else
    return std::countl_zero(value);
#endif
  }
  DOME_AFFINE_DOT static Extended from_double(uint64_t bits){
    const int raw=int((bits&infinity)>>52);
    const auto mantissa=bits&fraction;
    if(raw)return {(mantissa<<11)|leading,raw-1023,bool(bits&leading)};
    if(!mantissa)return {};
    const int shift=zeros(mantissa);
    return {mantissa<<shift,-1011-shift,bool(bits&leading)};
  }
  DOME_AFFINE_DOT static Extended rounded(uint64_t upper,uint64_t lower,int exponent,bool negative){
    if(lower>leading||(lower==leading&&(upper&1))){
      if(++upper==0){upper=leading;++exponent;}
    }
    return {upper,exponent,negative};
  }
  DOME_AFFINE_DOT Extended multiply(Extended b) const {
    if(!significand||!b.significand)return {};
    const uint64_t a0=uint32_t(significand),a1=significand>>32,
                   b0=uint32_t(b.significand),b1=b.significand>>32;
    const uint64_t t=a0*b0,x=a1*b0+(t>>32),y=a0*b1+uint32_t(x);
    const uint64_t lo=(y<<32)|uint32_t(t),hi=a1*b1+(x>>32)+(y>>32);
    if(hi&leading)return rounded(hi,lo,exponent+b.exponent+1,negative!=b.negative);
    return rounded((hi<<1)|(lo>>63),lo<<1,exponent+b.exponent,negative!=b.negative);
  }
  DOME_AFFINE_DOT Extended add(Extended b) const {
    if(!significand)return b;
    if(!b.significand)return *this;
    auto a=*this;
    if(a.exponent<b.exponent||(a.exponent==b.exponent&&a.significand<b.significand)){
      const auto swap=a;a=b;b=swap;
    }
    uint64_t high=b.significand,low=0;
    const int shift=a.exponent-b.exponent;
    if(shift>=128){high=0;low=1;}
    else if(shift>=64){
      const int tail=shift-64;
      high=0;
      low=b.significand>>tail;
      if(tail&&(b.significand<<(64-tail)))low|=1;
    }else if(shift){
      high=b.significand>>shift;
      low=b.significand<<(64-shift);
    }
    if(a.negative==b.negative){
      const auto sum=a.significand+high;
      if(sum>=a.significand)return rounded(sum,low,a.exponent,a.negative);
      const uint64_t tail=(sum<<63)|(low>>1)|(low&1);
      return rounded(leading|(sum>>1),tail,a.exponent+1,a.negative);
    }
    high=a.significand-high-(low!=0);
    low=uint64_t(0)-low;
    if(!high){
      if(!low)return {};
      const int left=zeros(low);
      return {low<<left,a.exponent-64-left,a.negative};
    }
    const int left=zeros(high);
    if(left){high=(high<<left)|(low>>(64-left));low<<=left;}
    return rounded(high,low,a.exponent-left,a.negative);
  }
  DOME_AFFINE_DOT uint64_t to_double() const {
    if(!significand)return 0;
    const uint64_t sign=negative?leading:0;
    if(exponent>1023)return sign|infinity;
    const int shift=exponent>=-1022?11:-1011-exponent;
    if(shift>64)return sign;
    if(shift==64)return sign|uint64_t(significand>leading);
    uint64_t rounded=significand>>shift;
    const uint64_t remainder=significand<<(64-shift);
    rounded+=remainder>leading||(remainder==leading&&(rounded&1));
    const uint64_t prefix=exponent>=-1022?uint64_t(exponent+1022)<<52:0;
    return sign|(prefix+rounded);
  }
};

DOME_AFFINE_DOT uint64_t affine_dot(const uint64_t *row,const uint64_t *direction,bool &valid){
  Extended sum;
  for(int k=0;k<3;++k)
    sum=sum.add(Extended::from_double(row[k]).multiply(Extended::from_double(direction[k])));
  const auto bits=sum.to_double(),magnitude=bits&~Extended::leading;
  valid&=magnitude<Extended::infinity&&(magnitude||!sum.significand);
  return bits;
}
}
#undef DOME_AFFINE_DOT
