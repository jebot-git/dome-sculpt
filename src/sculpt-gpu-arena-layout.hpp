// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>
namespace dome::render_detail {
// Units are vertices or uint32 indices, never bytes. Copies are private plans:
// reservations cannot reuse occupied accepted ranges before publication.
class SculptArenaSlots {
public:
  struct Span {size_t first=0,count=0;bool operator==(const Span &) const=default;};
  explicit SculptArenaSlots(size_t capacity=0):capacity_(capacity) {
    if(capacity)free_.push_back({0,capacity});
  }
  size_t capacity() const{return capacity_;}
  std::optional<Span> reserve(size_t count) {
    if(!count)return Span{};
    for(auto i=free_.begin();i!=free_.end();++i)if(i->count>=count){
      Span result{i->first,count};i->first+=count;i->count-=count;
      if(!i->count)free_.erase(i);
      return result;
    }
    return {};
  }
  void release(Span span) {
    if(!span.count)return;
    if(span.first>capacity_||span.count>capacity_-span.first)
      throw std::logic_error("Sculpt arena release outside capacity");
    auto after=std::lower_bound(free_.begin(),free_.end(),span.first,
      [](const Span&a,size_t b){return a.first<b;});
    if((after!=free_.end()&&span.first+span.count>after->first)||
       (after!=free_.begin()&&(after-1)->first+(after-1)->count>span.first))
      throw std::logic_error("Sculpt arena release overlaps free range");
    auto at=free_.insert(after,span); // May throw; no accepted allocator is edited.
    if(at!=free_.begin()&&(at-1)->first+(at-1)->count==at->first){
      (at-1)->count+=at->count;at=free_.erase(at)-1;
    }
    if(at+1!=free_.end()&&at->first+at->count==(at+1)->first){
      at->count+=(at+1)->count;free_.erase(at+1);
    }
  }
private:
  size_t capacity_=0;
  std::vector<Span> free_;
};
inline size_t sculpt_arena_add(size_t a,size_t b) {
  if(b>std::numeric_limits<size_t>::max()-a)
    throw std::length_error("Sculpt arena size overflow");
  return a+b;
}
inline size_t sculpt_arena_multiply(size_t a,size_t b) {
  if(a&&b>std::numeric_limits<size_t>::max()/a)
    throw std::length_error("Sculpt arena size overflow");
  return a*b;
}
inline size_t sculpt_arena_capacity(size_t required) {
  // Explicit 25% reserve, charged even when unused. No silent coarsening.
  return required?std::max(size_t(256),sculpt_arena_add(required,required/4)):0;
}
inline size_t sculpt_arena_storage(size_t vertices,size_t indices) {
  // Position + normal + ALWAYS enabled mask6 (including -1 defaults).
  return sculpt_arena_add(sculpt_arena_multiply(vertices,28),sculpt_arena_multiply(indices,4));
}
inline void sculpt_arena_peak(size_t retained,size_t staged,size_t limit) {
  if(retained>limit||staged>limit-retained)
    throw std::length_error("Sculpt GPU arena old plus staged capacity exceeds budget");
}
} // namespace dome::render_detail
