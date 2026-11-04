// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "heat-source.hpp"
#include "volume-exact.hpp"
namespace dome::detail {
// Geometry-only closest finite source, with exact ties. Coordinates are genuine
// captured binary32 REST coordinates in the same metric. This gives component
// intent only: no visibility, source-skin or final deformation authority.
class NearestWeightSources {
  struct Source {
    std::array<float, 3> head, tail;
    uint32_t joint;
  };
  std::vector<Source> sources_;
  uint32_t source_shift_ = UINT32_MAX;
  using Integer = volume_exact::Integer;
  using Fraction = volume_exact::Fraction;
  template <class Arithmetic>
  static Integer reduced(float value, uint32_t shift, Arithmetic &exact) {
    auto v = exact.scalar(value);
    if (v.sign) {
      if (v.shift < shift)
        throw std::logic_error("Nearest source common dyadic scale changed");
      v.shift -= shift;
    }
    return v;
  }
  template <class Arithmetic>
  static Fraction distance(std::array<float, 3> point, const Source &s,
                           uint32_t shift, Arithmetic &exact) {
    Integer rr, dd, rd;
    for (size_t k = 0; k < 3; ++k) {
      auto a = reduced(s.head[k], shift, exact);
      auto r = exact.subtract(reduced(point[k], shift, exact), a);
      auto d = exact.subtract(reduced(s.tail[k], shift, exact), a);
      rr = exact.add(rr, exact.multiply(r, r));
      dd = exact.add(dd, exact.multiply(d, d));
      rd = exact.add(rd, exact.multiply(r, d));
    }
    if (!dd.sign || rd.sign <= 0)
      return exact.fraction(rr, exact.integer(1));
    if (exact.subtract(rd, dd).sign >= 0) {
      Integer tail;
      for (size_t k = 0; k < 3; ++k) {
        auto r = exact.subtract(reduced(point[k], shift, exact),
                                reduced(s.tail[k], shift, exact));
        tail = exact.add(tail, exact.multiply(r, r));
      }
      return exact.fraction(tail, exact.integer(1));
    }
    auto numerator =
        exact.subtract(exact.multiply(rr, dd), exact.multiply(rd, rd));
    if (numerator.sign < 0)
      throw std::logic_error("Exact segment distance became negative");
    return exact.fraction(numerator, dd);
  }

public:
  template <class Budget>
  NearestWeightSources(std::span<const HeatSourceSegment> input, Budget &work) {
    if (input.empty() || input.size() > 512)
      throw std::length_error("Nearest source construction bound");
    work.step(input.size() * 64);
    sources_.reserve(input.size());
    volume_exact::Arithmetic exact(work);
    for (const auto &s : input) {
      if (s.bone >= 256)
        throw std::invalid_argument(
            "Nearest source joint outside declared mask");
      Source owned;
      owned.joint = s.bone;
      for (size_t k = 0; k < 3; ++k) {
        owned.head[k] = float(s.head[k]);
        owned.tail[k] = float(s.tail[k]);
        if (double(owned.head[k]) != s.head[k] ||
            double(owned.tail[k]) != s.tail[k])
          throw std::invalid_argument(
              "Nearest source requires exact captured binary32 endpoints");
        for (float value : {owned.head[k], owned.tail[k]}) {
          auto n = exact.scalar(value);
          if (n.sign)
            source_shift_ = std::min(source_shift_, n.shift);
        }
      }
      sources_.push_back(owned);
    }
  }
  template <class Budget>
  std::array<uint64_t, 4> nearest(std::array<float, 3> point,
                                  Budget &work) const {
    work.stop();
    volume_exact::Arithmetic exact(work);
    auto shift = source_shift_;
    for (auto value : point) {
      auto n = exact.scalar(value);
      if (n.sign)
        shift = std::min(shift, n.shift);
    }
    if (shift == UINT32_MAX)
      shift = 0;
    // A common per-query dyadic scale cancels before distance products.
    // Bounded binary32 coordinates have <=166 significant coordinate bits;
    // degree-six distance comparisons plus dot-product carries fit1024 bits.
    std::optional<Fraction> best;
    std::array<uint64_t, 4> mask{};
    for (const auto &source : sources_) {
      auto value = distance(point, source, shift, exact);
      const int order = best ? exact.compare(value, *best) : -1;
      if (order < 0) {
        best = value;
        mask = {};
      }
      if (order <= 0)
        mask[source.joint / 64] |= uint64_t(1) << (source.joint % 64);
    }
    return mask;
  }
};
} // namespace dome::detail
