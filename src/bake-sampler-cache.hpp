// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <stdexcept>

namespace dome::detail {
// Scoped to one immutable Bake source. Keep large fields usable with the
// original one-sampler working set, and reuse smaller fields across batches.
template<class Sampler> class BakeSamplerCache {
  struct Entry {
    uint32_t material;
    size_t bytes;
    std::unique_ptr<Sampler> sampler;
  };
  std::list<Entry> recent_;
  size_t bytes_=0;
  const size_t limit_;

  void evict() {
    bytes_-=recent_.back().bytes;
    recent_.pop_back();
  }
public:
  explicit BakeSamplerCache(size_t limit):limit_(limit){}

  void clear() {
    recent_.clear();
    bytes_=0;
  }

  bool trim_idle() {
    const bool trimmed=recent_.size()>1;
    while(recent_.size()>1)evict();
    return trimmed;
  }

  Sampler &get(uint32_t material,auto create,auto trim_other) {
    for(auto entry=recent_.begin();entry!=recent_.end();++entry)if(entry->material==material){
      recent_.splice(recent_.begin(),recent_,entry);
      return *recent_.front().sampler;
    }
    size_t incoming=0;
    const auto reserve=[&](size_t bytes){
      if(bytes>std::numeric_limits<size_t>::max()-incoming)
        throw std::length_error("Bake sampler allocation size overflow");
      incoming+=bytes;
      while(!recent_.empty()&&(incoming>limit_||bytes_>limit_-incoming))evict();
    };
    std::unique_ptr<Sampler> sampler;
    try{
      sampler=create(reserve);
    }catch(const std::bad_alloc &){
      const bool had_entries=!recent_.empty();
      clear();
      if(!trim_other()&&!had_entries)throw;
      incoming=0;
      sampler=create(reserve);
    }
    recent_.push_front({material,incoming,std::move(sampler)});
    bytes_+=incoming;
    return *recent_.front().sampler;
  }
};
} // namespace dome::detail
