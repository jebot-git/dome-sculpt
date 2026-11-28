// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/baked-emission.hpp"
#include <bit>

namespace dome::detail {
// Exact summaries of FLOAT bit patterns, including signed zero.
struct BakeRadianceSummary {
  uint32_t common[3]{UINT32_MAX,UINT32_MAX,UINT32_MAX};
  uint32_t seen[3]{};
  void include(const std::array<float,3> &radiance) noexcept {
    for(unsigned c=0;c<3;++c){
      const auto word=std::bit_cast<uint32_t>(radiance[c]);
      common[c]&=word;
      seen[c]|=word;
    }
  }
  void merge(const BakeRadianceSummary &other) noexcept {
    for(unsigned c=0;c<3;++c){
      common[c]&=other.common[c];
      seen[c]|=other.seen[c];
    }
  }
};

struct BakeEmissionAccess {
  static std::shared_ptr<const BakedEmissionField> seal(uint32_t resolution,
      std::span<const std::array<float,3>> radiance,std::span<const uint8_t> coverage,size_t covered,
      BakeRadianceSummary summary,bool prefer_compact,const BakedEmissionLimits &limits);
};
}
