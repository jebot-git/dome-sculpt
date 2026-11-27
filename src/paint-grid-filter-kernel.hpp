// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "softfloat/dome_f64.hpp"
#include <cstddef>
#include <cstdint>

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_GRID_FN __host__ __device__ inline
#else
#define DOME_GRID_FN inline
#endif

namespace dome::paint_grid {
enum class FloatPlane : uint8_t { Absent, UnassociatedPaint, BakedCoefficients };
inline constexpr unsigned channels = 20;
inline constexpr unsigned float_channels = 7;

struct Pixel {
  uint64_t emission[float_channels]{};
  uint8_t values[channels]{};
  uint8_t coverage[4]{};
  uint8_t mask = 255, present = 0;
};
struct Layout {
  uint32_t active = 0, srgb = 0;
  uint32_t pbr = 0;
  uint8_t group[channels]{};
  uint8_t groups = 1;
  uint8_t coverage_mask = 0xf;
  FloatPlane floats = FloatPlane::Absent;
  bool mask = false;
  uint8_t fixed_common = 0;
  uint8_t fixed_emission = 0;
  uint8_t common_constants[6]{};
  uint64_t emission_constants[float_channels-1]{};
};
struct Rectangle { uint32_t x, y, width, height; };
struct Batch {
  uint32_t from, to;
  Rectangle source, target;
};
struct Tables {
  uint64_t unit[256], srgb[256];
  uint64_t unit_threshold[255], srgb_threshold[255];
};

DOME_GRID_FN uint64_t add(uint64_t a,uint64_t b) {
  return softfloat64::f64_add({a},{b}).v;
}
DOME_GRID_FN uint64_t mul(uint64_t a,uint64_t b) {
  return softfloat64::f64_mul({a},{b}).v;
}
DOME_GRID_FN uint64_t div(uint64_t a,uint64_t b) {
  // All nonzero paint operands are positive normal binary64 values. The
  // narrowest inputs originate in F32 and even a weighted product fits
  // comfortably inside the F64 normal exponent range. Use a fixed-width
  // quotient with a nearest-even remainder test on every device.
  if(!a)return 0;
  constexpr uint64_t hidden=1ull<<52, mask=hidden-1;
  int exponent=int(a>>52)-int(b>>52)+1023;
  if(!(b&mask))return (uint64_t(exponent)<<52)|(a&mask);
  const uint64_t denominator=hidden|(b&mask);
  uint64_t numerator=hidden|(a&mask);
  if(numerator<denominator) {
    numerator<<=1;
    --exponent;
  }
  uint64_t remainder=numerator-denominator, fraction=0;
  for(unsigned bit=0;bit<52;++bit) {
    remainder<<=1;
    fraction<<=1;
    if(remainder>=denominator) {
      remainder-=denominator;
      fraction|=1;
    }
  }
  const auto complement=denominator-remainder;
  const bool round_up=remainder>complement||(remainder==complement&&(fraction&1));
  return (uint64_t(exponent)<<52)+fraction+round_up;
}
DOME_GRID_FN uint64_t integer(int32_t a) { return softfloat64::i32_to_f64(a).v; }

struct Axis {
  uint32_t first, count;
  // For enlargement the two taps have integer weights over 2*to.
  uint32_t second, fraction;
};
DOME_GRID_FN Axis axis(uint32_t from,uint32_t to,uint32_t x) {
  if (from > to) {
    const auto scale = from / to;
    return {x * scale,scale,0,0};
  }
  const int denominator = int(2 * to);
  const int numerator = int((2 * x + 1) * from) - int(to);
  const int lower = numerator >= 0 ? numerator / denominator : -1;
  const auto fraction = uint32_t(numerator - lower * denominator);
  const uint32_t a = lower < 0 ? 0 : uint32_t(lower);
  const uint32_t b = a == from - 1 || lower < 0 ? a : a + 1;
  return {a,2,b,fraction};
}
DOME_GRID_FN uint8_t quantize(uint64_t value,const uint64_t *threshold) {
  uint32_t lo = 0, hi = 255;
  while (lo < hi) {
    const auto mid = (lo + hi) / 2;
    if (value >= threshold[mid]) lo = mid + 1;
    else hi = mid;
  }
  return uint8_t(lo);
}

DOME_GRID_FN Pixel filter(size_t index,const Batch &batch,const Layout &layout,
                         const Tables &tables,const Pixel *input) {
  const auto x = axis(batch.from,batch.to,batch.target.x + uint32_t(index % batch.target.width));
  const auto y = axis(batch.from,batch.to,batch.target.y + uint32_t(index / batch.target.width));
  const bool reduce = batch.from > batch.to;
  const auto denominator = integer(reduce ? int32_t(x.count*y.count) : int32_t(2*batch.to));
  const auto area = div(integer(1),denominator);
  uint64_t weighted[channels]{}, raw[channels]{}, coverage[4]{}, mask=0;
  uint64_t emission_weighted[float_channels-1]{}, emission_raw[float_channels]{};
  Pixel out;
  for (uint32_t yi=0;yi<y.count;++yi) {
    const auto source_y = reduce ? y.first+yi : yi ? y.second : y.first;
    const auto wy = yi ? y.fraction : 2*batch.to-y.fraction;
    for (uint32_t xi=0;xi<x.count;++xi) {
      const auto source_x = reduce ? x.first+xi : xi ? x.second : x.first;
      const auto wx = xi ? x.fraction : 2*batch.to-x.fraction;
      if (!reduce && (!wx || !wy)) continue;
      const auto weight = reduce ? area : mul(mul(integer(int(wx)),area),mul(integer(int(wy)),area));
      const auto &p = input[size_t(source_y-batch.source.y)*batch.source.width+source_x-batch.source.x];
      out.present |= p.present;
      uint64_t group_weight[4]{};
      for (unsigned g=0;g<layout.groups;++g) if(layout.coverage_mask&(1u<<g)) {
        group_weight[g]=mul(weight,tables.unit[p.coverage[g]]);
        coverage[g]=add(coverage[g],group_weight[g]);
      }
      const auto pbr_weight=layout.pbr&&!reduce?
          div(mul(weight,integer(p.coverage[0])),integer(255)):0;
      for (unsigned c=0;c<channels;++c) if (layout.active&(1u<<c)) {
        const auto value=(layout.srgb&(1u<<c))?tables.srgb[p.values[c]]:tables.unit[p.values[c]];
        const auto a=!reduce&&(layout.pbr&(1u<<c))?pbr_weight:group_weight[layout.group[c]];
        weighted[c]=add(weighted[c],mul(a,value));
        raw[c]=add(raw[c],mul(weight,value));
      }
      if (layout.mask)
        mask=add(mask,reduce?mul(weight,tables.unit[p.mask]):
                 div(mul(weight,integer(p.mask)),integer(255)));
      if (layout.floats!=FloatPlane::Absent) {
        for (unsigned c=0;c<float_channels;++c)
          if (!(layout.fixed_emission&(1u<<c)))
            emission_raw[c]=add(emission_raw[c],mul(weight,p.emission[c]));
        if (layout.floats==FloatPlane::UnassociatedPaint) {
          const auto a=mul(weight,p.emission[6]);
          for (unsigned c=0;c<6;++c)
            if (!(layout.fixed_emission&(1u<<c)))
              emission_weighted[c]=add(emission_weighted[c],mul(a,p.emission[c]));
        }
      }
    }
  }
  for (unsigned g=0;g<layout.groups;++g) if(layout.coverage_mask&(1u<<g))
    out.coverage[g]=quantize(coverage[g],tables.unit_threshold);
  for (unsigned c=0;c<channels;++c) if (layout.active&(1u<<c)) {
    const auto a=coverage[layout.group[c]];
    const auto value=a?div(weighted[c],a):raw[c];
    out.values[c]=quantize(value,(layout.srgb&(1u<<c))?tables.srgb_threshold:tables.unit_threshold);
  }
  for (unsigned c=0;c<6;++c) if (layout.fixed_common&(1u<<c))
    out.values[c]=layout.common_constants[c];
  if (layout.mask) out.mask=quantize(mask,tables.unit_threshold);
  if (layout.floats!=FloatPlane::Absent) {
    const auto a=emission_raw[6];
    out.emission[6]=a;
    for (unsigned c=0;c<6;++c)
      out.emission[c]=layout.fixed_emission&(1u<<c)?layout.emission_constants[c]:
          (a&&layout.floats==FloatPlane::UnassociatedPaint)?div(emission_weighted[c],a):emission_raw[c];
  }
  return out;
}
}
#undef DOME_GRID_FN
