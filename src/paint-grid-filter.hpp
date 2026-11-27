// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "paint-grid-filter-kernel.hpp"
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>

namespace dome::paint_grid {
// Each batch owns a bounded input rectangle and one detached output. A
// selected GPU backend keeps its queue, tables and scratch across batches.
inline constexpr size_t max_input_pixels = 65536, max_output_pixels = 32768;
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
      !layout.groups||layout.groups>4||layout.active>=(1u<<channels))
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
class Filter {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  explicit Filter(const Tables &);
  ~Filter();
  Filter(const Filter &) = delete;
  Filter &operator=(const Filter &) = delete;
  void run(const Batch &,const Layout &,std::span<const Pixel>,std::span<Pixel>,
           const std::function<bool()> &cancelled);
};
}
