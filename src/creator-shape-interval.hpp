// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
namespace dome::creator_detail {
// Float endpoints enclose the creator's float arithmetic at every operation.
// Transcendentals reserve four ulps on either side, including subnormals. The
// operation graph mirrors Shaper::raw; subdivision covers whole input cells.
struct ShapeInterval {
  float lo=0,hi=0;
  ShapeInterval()=default;
  ShapeInterval(float x):lo(x),hi(x){}
  ShapeInterval(float a,float b):lo(a),hi(b){}
  static float down(float x){return std::nextafter(x,-INFINITY);}
  static float up(float x){return std::nextafter(x,INFINITY);}
  ShapeInterval &operator+=(ShapeInterval b);
};
inline ShapeInterval operator+(ShapeInterval a,ShapeInterval b){return {ShapeInterval::down(a.lo+b.lo),ShapeInterval::up(a.hi+b.hi)};}
inline ShapeInterval operator-(ShapeInterval a){return {-a.hi,-a.lo};}
inline ShapeInterval operator-(ShapeInterval a,ShapeInterval b){return a+-b;}
inline ShapeInterval operator*(ShapeInterval a,ShapeInterval b){float v[]={a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};return {ShapeInterval::down(*std::min_element(v,v+4)),ShapeInterval::up(*std::max_element(v,v+4))};}
inline ShapeInterval operator/(ShapeInterval a,float b){return b>0?ShapeInterval{ShapeInterval::down(a.lo/b),ShapeInterval::up(a.hi/b)}:ShapeInterval{ShapeInterval::down(a.hi/b),ShapeInterval::up(a.lo/b)};}
inline ShapeInterval &ShapeInterval::operator+=(ShapeInterval b){*this=*this+b;return *this;}
inline ShapeInterval &operator*=(ShapeInterval &a,ShapeInterval b){a=a*b;return a;}
inline ShapeInterval &operator-=(ShapeInterval &a,ShapeInterval b){a=a-b;return a;}
inline ShapeInterval abs(ShapeInterval a){return {a.lo<=0&&a.hi>=0?0.f:std::min(std::abs(a.lo),std::abs(a.hi)),std::max(std::abs(a.lo),std::abs(a.hi))};}
inline ShapeInterval square(ShapeInterval a){auto x=abs(a);return {std::max(0.f,ShapeInterval::down(x.lo*x.lo)),ShapeInterval::up(x.hi*x.hi)};}
inline ShapeInterval exp(ShapeInterval a){auto lo=std::exp(a.lo),hi=std::exp(a.hi);for(int k=0;k<4;++k){lo=ShapeInterval::down(lo);hi=ShapeInterval::up(hi);}return {std::max(0.f,lo),hi};}
inline ShapeInterval smooth(float a,float b,ShapeInterval v){auto f=[&](float x){float t=std::clamp((x-a)/(b-a),0.f,1.f);return t*t*(3-2*t);};auto lo=f(v.lo),hi=f(v.hi);for(int k=0;k<4;++k){lo=ShapeInterval::down(lo);hi=ShapeInterval::up(hi);}return {std::max(0.f,lo),std::min(1.f,hi)};}
inline ShapeInterval interval_leg(ShapeInterval p,float hips,float c){auto f=[&](float y){return y<hips?hips+(y-hips)*(1+c*.12f):y;};return {ShapeInterval::down(f(p.lo)),ShapeInterval::up(f(p.hi))};}
inline ShapeInterval interval_arm(ShapeInterval p,float shoulder,float c){auto f=[&](float x){if(std::abs(x)>shoulder)x+=(x<0?-1.f:1.f)*(std::abs(x)-shoulder)*c*.12f;return x;};return {ShapeInterval::down(f(p.lo)),ShapeInterval::up(f(p.hi))};}
struct ShapeBox{ShapeInterval x,y,z;};
inline ShapeInterval interval_abs(ShapeInterval v){return abs(v);}
inline ShapeInterval interval_pow2(ShapeInterval v,float){auto r=square(v);for(int k=0;k<4;++k){r.lo=ShapeInterval::down(r.lo);r.hi=ShapeInterval::up(r.hi);}r.lo=std::max(0.f,r.lo);return r;}
inline ShapeInterval interval_exp(ShapeInterval v){return exp(v);}
inline ShapeInterval interval_smooth(float a,float b,ShapeInterval v){return smooth(a,b,v);}
}
