// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "weight-surface-system.hpp"
namespace dome::detail {
// Private no-lock small-K packing. Labels are copied/authenticated once, while
// every numerical row is read afresh. This grants no geometry/source authority.
class GeometryFieldPacker {
  std::array<uint32_t,256> labels_{};
  size_t count_=0,bones_=0;
  WeightLimits limits_;
public:
  GeometryFieldPacker(std::span<const uint32_t> labels,size_t bones,WeightLimits limits,weight_surface::Budget&work):bones_(bones),limits_(limits){
    if(!bones||bones>256||labels.empty()||labels.size()>256||!limits.max_influences||limits.max_influences>8||!std::isfinite(limits.prune)||limits.prune<0||limits.prune>=1)
      throw std::invalid_argument("Invalid bounded field packing labels/limits");
    std::array<bool,256> seen{};
    for(auto joint:labels){work.step(16);if(joint>=bones||seen[joint])throw std::invalid_argument("Field packing has ambiguous joint identities");seen[joint]=true;labels_[count_++]=joint;}
  }
  VertexWeights pack(std::span<const double> row,weight_surface::Budget&work)const{
    if(row.size()!=count_)throw std::invalid_argument("Field packing row dimensions");
    // Per-label finite conversion and bounded insertion/shift work, plus the
    // unchanged at-most-eight-element constrained normalizer. No full-label
    // duplicate tree or full positive-field sort is constructed per raw row.
    work.step(256+count_*(32+16*limits_.max_influences));
    std::array<Influence,8> best{};size_t used=0;
    auto before=[](Influence a,Influence b){return a.weight==b.weight?a.joint<b.joint:a.weight>b.weight;};
    for(size_t n=0;n<count_;++n){
      if(!(row[n]>0))continue; // Same positive-double filter as the old caller.
      const float value=float(row[n]); // Selection uses the original FLOAT key.
      if(!std::isfinite(value)||value<0||value>1)throw std::invalid_argument("Invalid field influence");
      if(!(value>limits_.prune))continue;
      const Influence candidate{labels_[n],value};size_t position=0;
      while(position<used&&before(best[position],candidate))++position;
      if(position>=limits_.max_influences)continue;
      const size_t last=std::min(used,size_t(limits_.max_influences-1));
      for(size_t k=last;k>position;--k)best[k]=best[k-1];
      best[position]=candidate;if(used<limits_.max_influences)++used;
    }
    // Keep the production locked-weight arithmetic and ordering implementation.
    return normalize_weights(VertexWeights(best.begin(),best.begin()+used),bones_,{},limits_);
  }
};
} // namespace dome::detail
