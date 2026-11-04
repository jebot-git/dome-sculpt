// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

namespace dome::detail::volume_exact {
// Exact dyadic predicates for bounded binary32 geometry and a portal parameter
// with at most60 fractional bits. Coordinates use the common2^-210 scale.
// At |coordinate|<=10000, a difference has at most225bits; orient3d <680bits,
// clip coefficients <512bits, and fraction comparisons <1024bits. Checked
// 1024-bit storage therefore needs neither a heap nor a third-party runtime.
struct Integer {
  std::array<uint32_t,32> limb{};
  uint32_t used=0;
  uint32_t shift=0;
  int sign=0;
};
struct Fraction {Integer numerator,denominator;};
struct Interval {Fraction enter,exit;};
struct Point2 {
  std::array<float,2> a{},b{};
  uint64_t parameter=0;
  std::array<double,2> free{};
  bool ordinary=false;
};
inline constexpr uint64_t parameter_scale=uint64_t(1)<<60;

template<class Budget>class Arithmetic {
  Budget &work;
  static void trim(Integer &a){
    while(a.used&&!a.limb[a.used-1])--a.used;
    if(!a.used){a.sign=0;a.shift=0;return;}
    size_t empty=0;while(!a.limb[empty])++empty;
    if(empty){for(size_t i=empty;i<a.used;++i)a.limb[i-empty]=a.limb[i];a.used-=uint32_t(empty);a.shift+=uint32_t(empty*32);}
    unsigned bits=std::countr_zero(a.limb[0]);
    if(bits){for(size_t i=0;i<a.used;++i)a.limb[i]=(a.limb[i]>>bits)|(i+1<a.used?a.limb[i+1]<<(32-bits):0);a.shift+=bits;while(a.used&&!a.limb[a.used-1])--a.used;}
    std::fill(a.limb.begin()+a.used,a.limb.end(),0);
    unsigned top=32-std::countl_zero(a.limb[a.used-1]);
    if(a.shift+(a.used-1)*32+top>1024)throw std::overflow_error("Exact volume integer capacity exceeded");
  }
  Integer aligned(const Integer&a,uint32_t base){
    if(!a.sign)return {};
    work.step(a.used);uint32_t difference=a.shift-base,word=difference/32,bits=difference%32;
    if(word+a.used>32)throw std::overflow_error("Exact volume alignment capacity exceeded");
    Integer out;out.sign=a.sign;out.shift=base;
    for(size_t i=0;i<a.used;++i){uint64_t n=uint64_t(a.limb[i])<<bits;out.limb[word+i]|=uint32_t(n);if(n>>32){if(word+i+1>=32)throw std::overflow_error("Exact volume alignment carry exceeded");out.limb[word+i+1]|=uint32_t(n>>32);}}
    out.used=std::min(uint32_t(32),word+a.used+1);while(out.used&&!out.limb[out.used-1])--out.used;return out;
  }
  int magnitude(const Integer&a,const Integer&b){work.step(std::max(a.used,b.used));if(a.used!=b.used)return a.used<b.used?-1:1;for(size_t i=a.used;i;--i)if(a.limb[i-1]!=b.limb[i-1])return a.limb[i-1]<b.limb[i-1]?-1:1;return 0;}
public:
  explicit Arithmetic(Budget&w):work(w){}
  Integer integer(uint64_t n){Integer a;if(n){a.sign=1;a.limb[0]=uint32_t(n);a.limb[1]=uint32_t(n>>32);a.used=a.limb[1]?2:1;trim(a);}return a;}
  Integer scalar(double value){
    work.step();if(!std::isfinite(value)||std::abs(value)>10000)throw std::range_error("Exact volume coordinate exceeds10000 metre workspace");
    static_assert(sizeof(double)==8&&std::numeric_limits<double>::is_iec559);
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559);
    uint64_t bits=std::bit_cast<uint64_t>(value),mantissa=bits&0xfffffffffffffULL;int encoded=int((bits>>52)&2047);
    if(encoded)mantissa|=uint64_t(1)<<52;
    if(!mantissa)return {};
    int exponent=encoded?encoded-1023-52:-1074;
    int zeros=std::countr_zero(mantissa);mantissa>>=zeros;exponent+=zeros;
    int shift=210+exponent;if(shift<0)throw std::range_error("Exact volume coordinate exceeds210 fractional bits");
    auto a=integer(mantissa);a.shift+=uint32_t(shift);a.sign=bits>>63?-1:1;trim(a);return a;
  }
  Integer scalar(float value){return scalar(double(value));}
  Integer negate(Integer a){a.sign=-a.sign;return a;}
  Integer add(const Integer&left,const Integer&right){
    if(!left.sign)return right;
    if(!right.sign)return left;
    uint32_t base=std::min(left.shift,right.shift);auto a=aligned(left,base),b=aligned(right,base);work.step(std::max(a.used,b.used));Integer out;out.shift=base;
    if(a.sign==b.sign){out.sign=a.sign;uint64_t carry=0;out.used=std::max(a.used,b.used);for(size_t i=0;i<out.used;++i){uint64_t n=uint64_t(a.limb[i])+b.limb[i]+carry;out.limb[i]=uint32_t(n);carry=n>>32;}if(carry){if(out.used==32)throw std::overflow_error("Exact volume addition capacity exceeded");out.limb[out.used++]=uint32_t(carry);}trim(out);return out;}
    int order=magnitude(a,b);if(!order)return {};const auto &large=order>0?a:b,&small=order>0?b:a;out.sign=large.sign;out.used=large.used;uint64_t borrow=0;
    for(size_t i=0;i<out.used;++i){uint64_t left=large.limb[i],right=uint64_t(small.limb[i])+borrow;out.limb[i]=uint32_t(left-right);borrow=left<right;}
    trim(out);return out;
  }
  Integer subtract(const Integer&a,const Integer&b){return add(a,negate(b));}
  Integer multiply(const Integer&a,const Integer&b){
    if(!a.sign||!b.sign)return {};
    work.step(size_t(a.used)*b.used);Integer out;out.sign=a.sign*b.sign;out.shift=a.shift+b.shift;
    for(size_t i=0;i<a.used;++i){uint64_t carry=0;for(size_t j=0;j<b.used;++j){size_t at=i+j;if(at>=32){if(a.limb[i]&&b.limb[j])throw std::overflow_error("Exact volume product capacity exceeded");continue;}uint64_t n=uint64_t(a.limb[i])*b.limb[j]+out.limb[at]+carry;out.limb[at]=uint32_t(n);carry=n>>32;}size_t at=i+b.used;while(carry){if(at>=32)throw std::overflow_error("Exact volume carry capacity exceeded");uint64_t n=uint64_t(out.limb[at])+carry;out.limb[at++]=uint32_t(n);carry=n>>32;}}
    out.used=std::min(uint32_t(32),a.used+b.used);trim(out);return out;
  }
  template<class A,class B,class C>Integer orient2(const std::array<A,2>&a,const std::array<B,2>&b,const std::array<C,2>&c){auto x=subtract(scalar(b[0]),scalar(a[0])),y=subtract(scalar(b[1]),scalar(a[1]));auto u=subtract(scalar(c[0]),scalar(a[0])),v=subtract(scalar(c[1]),scalar(a[1]));return subtract(multiply(x,v),multiply(y,u));}
  template<class A,class B,class C,class D>int orient3(const std::array<A,3>&a,const std::array<B,3>&b,const std::array<C,3>&c,const std::array<D,3>&d){
    std::array<Integer,3>x,y,z;for(int i=0;i<3;++i){auto origin=scalar(a[i]);x[i]=subtract(scalar(b[i]),origin);y[i]=subtract(scalar(c[i]),origin);z[i]=subtract(scalar(d[i]),origin);}
    auto term=[&](int i,int j,int k){return multiply(x[i],subtract(multiply(y[j],z[k]),multiply(y[k],z[j])));};
    return add(add(term(0,1,2),term(1,2,0)),term(2,0,1)).sign;
  }
  Fraction fraction(Integer n,Integer d){if(!d.sign)throw std::logic_error("Exact volume zero denominator");if(d.sign<0){n=negate(n);d=negate(d);}return {n,d};}
  Fraction zero(){return {integer(0),integer(1)};}
  Fraction one(){return {integer(1),integer(1)};}
  int compare(const Fraction&a,const Fraction&b){return subtract(multiply(a.numerator,b.denominator),multiply(b.numerator,a.denominator)).sign;}
  Integer coefficient(std::array<float,2>a,std::array<float,2>b,const Point2&p){
    if(p.ordinary)return multiply(orient2(a,b,p.free),integer(parameter_scale));
    if(p.parameter>parameter_scale)throw std::logic_error("Exact volume portal parameter exceeds its dyadic bound");
    auto x=orient2(a,b,p.a),y=orient2(a,b,p.b);
    return add(multiply(x,integer(parameter_scale-p.parameter)),multiply(y,integer(p.parameter)));
  }
  std::optional<Interval> clip(const std::array<std::array<float,2>,3>&triangle,const Point2&p,const Point2&q){
    int sign=orient2(triangle[0],triangle[1],triangle[2]).sign;if(!sign)return {};
    Interval result{zero(),one()};
    for(int edge=0;edge<3;++edge){auto a=triangle[edge],b=triangle[(edge+1)%3];auto x=coefficient(a,b,p),y=coefficient(a,b,q);if(sign<0){x=negate(x);y=negate(y);}if(x.sign>=0&&y.sign>=0)continue;if(x.sign<0&&y.sign<0)return {};auto at=fraction(x,subtract(x,y));if(x.sign<0){if(compare(at,result.enter)>0)result.enter=at;}else if(compare(at,result.exit)<0)result.exit=at;if(compare(result.enter,result.exit)>0)return {};}
    return result;
  }
};
} // namespace dome::detail::volume_exact
