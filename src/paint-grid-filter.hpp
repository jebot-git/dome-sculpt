// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "paint-grid-filter-kernel.hpp"
#include <bit>
#include <cmath>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>

namespace dome::paint_grid {
// Each batch owns a bounded input rectangle and one detached output. A
// selected GPU backend keeps its queue, tables and scratch across batches.
inline constexpr size_t max_input_pixels = 65536, max_output_pixels = 32768;
inline constexpr size_t max_mapped_pixels = 8192;
inline void checkpoint(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled()) throw std::runtime_error("Paint grid filtering cancelled");
}
inline void validate(const Batch &batch,const Layout &layout,size_t inputs,size_t outputs) {
  const auto power_of_two=[](uint32_t r){return r>=64&&r<=2048&&!(r&(r-1));};
  const auto within=[](const Rectangle &r,uint32_t extent){
    return r.x<extent&&r.y<extent&&r.width&&r.height&&
           r.width<=extent-r.x&&r.height<=extent-r.y;
  };
  if (!power_of_two(batch.from)||!power_of_two(batch.to)||
      !within(batch.source,batch.from)||!within(batch.target,batch.to)||
      inputs!=size_t(batch.source.width)*batch.source.height||inputs>max_input_pixels||
      outputs!=size_t(batch.target.width)*batch.target.height||outputs>max_output_pixels||
      !layout.groups||layout.groups>4||layout.active>=(1u<<channels)||
      (layout.fixed_common&&(batch.from<=batch.to||layout.fixed_common>=64||
          (layout.fixed_common&layout.active)))||
      (layout.fixed_emission&&(batch.from<=batch.to||
          layout.floats!=FloatPlane::UnassociatedPaint||layout.fixed_emission>=64)))
    throw std::invalid_argument("Invalid bounded paint-grid batch");
  for (unsigned c=0;c<channels;++c)
    if ((layout.active&(1u<<c))&&layout.group[c]>=layout.groups)
      throw std::invalid_argument("Invalid paint-grid coverage group");
  const auto check=[&](uint32_t first,uint32_t count,uint32_t offset,uint32_t length){
    const auto begin=axis(batch.from,batch.to,first),end=axis(batch.from,batch.to,first+count-1);
    const auto last=batch.from>batch.to?end.first+end.count-1:end.second;
    return begin.first>=offset&&last<offset+length;
  };
  if (!check(batch.target.x,batch.target.width,batch.source.x,batch.source.width)||
      !check(batch.target.y,batch.target.height,batch.source.y,batch.source.height))
    throw std::invalid_argument("Paint-grid input rectangle does not own the target footprint");
}
inline size_t mapped_capacity(size_t count) {
  if(count>max_mapped_pixels)throw std::invalid_argument("Invalid mapped paint batch capacity");
  return count;
}
inline void validate_mapped(const Layout &layout,std::span<const MappedTaps> taps,size_t inputs,
                            size_t outputs,size_t capacity,const std::function<bool()> &cancelled) {
  checkpoint(cancelled);
  if(taps.empty()||taps.size()!=outputs||outputs>capacity||inputs>capacity*4||
     !layout.groups||layout.groups>4||layout.active>=(1u<<channels)||layout.fixed_common||layout.fixed_emission)
    throw std::invalid_argument("Invalid bounded mapped paint batch");
  for(unsigned c=0;c<channels;++c)
    if((layout.active&(1u<<c))&&layout.group[c]>=layout.groups)
      throw std::invalid_argument("Invalid mapped paint coverage group");
  for(size_t i=0;i<taps.size();++i){
    if((i&1023)==0)checkpoint(cancelled);
    if(!taps[i].covered)continue;
    for(unsigned tap=0;tap<4;++tap)if(taps[i].weights[tap]){
      const auto weight=std::bit_cast<double>(taps[i].weights[tap]);
      if(taps[i].texels[tap]>=inputs||!std::isfinite(weight)||weight<=0||weight>1)
        throw std::invalid_argument("Invalid paint UV sample");
    }
  }
}
class Filter {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  explicit Filter(const Tables &,size_t mapped_count=0);
  ~Filter();
  Filter(const Filter &) = delete;
  Filter &operator=(const Filter &) = delete;
  void run(const Batch &,const Layout &,std::span<const Pixel>,std::span<Pixel>,
           const std::function<bool()> &cancelled);
  void run_mapped(const Layout &,std::span<const Pixel>,std::span<const MappedTaps>,std::span<Pixel>,
                  const std::function<bool()> &cancelled);
};
}
