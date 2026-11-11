// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/volume.hpp"
#include <bit>
#include <stdexcept>
#include <vector>

namespace dome::detail {
// Native files do not serialize COW control blocks. After full Volume decoding,
// share actual immutable evaluated storage only after every key/value bit has
// been checked. Layers, masks, settings and future edits remain independent.
// This never attaches an old geometry packet to a different source identity:
// the decoded Volume itself acquires the verified COW data before publication.
class NativeSceneGeometryIntern {
  using Cells=CowMap<Int3,float,IntHash>;
  struct Key {uint64_t size,context,sum,xor_sum;bool operator==(const Key&)const=default;};
  struct Entry {Key key;Cells cells;};
  std::vector<Entry> entries_;
  inline static thread_local NativeSceneGeometryIntern *current_=nullptr;
  NativeSceneGeometryIntern *previous_;
  // Scene preflight admits at most four million samples. Hashing plus positive
  // equality checks fit eight million visits; adversarial candidate collisions
  // cannot turn this optional optimization into quadratic unbounded work.
  size_t remaining_=8'000'000;
  static uint64_t mix(uint64_t v)noexcept {
    v=(v^(v>>30))*0xbf58476d1ce4e5b9ull;
    v=(v^(v>>27))*0x94d049bb133111ebull;return v^(v>>31);
  }
  void intern(Cells &cells,float voxel,float band) {
    const auto count=cells.size();
    if(count>remaining_)return;
    remaining_-=count;
    Key key{count,(uint64_t(std::bit_cast<uint32_t>(voxel))<<32)|std::bit_cast<uint32_t>(band),0,0};
    for(const auto &[p,value]:cells.read()) {
      const auto h=mix(uint32_t(p.x))^std::rotl(mix(uint32_t(p.y)),21)^std::rotl(mix(uint32_t(p.z)),42)^
          std::rotl(mix(std::bit_cast<uint32_t>(value)),7);
      key.sum+=mix(h);key.xor_sum^=mix(h^0x9e3779b97f4a7c15ull);
    }
    for(const auto &entry:entries_)if(entry.key==key) {
      if(count>remaining_)return;
      remaining_-=count;
      bool same=true;const auto candidate=entry.cells.read();
      for(const auto &[p,value]:cells.read()) {
        const auto at=candidate.find(p);
        if(at==candidate.end()||std::bit_cast<uint32_t>(at->second)!=std::bit_cast<uint32_t>(value)){same=false;break;}
      }
      if(same){cells=entry.cells;return;}
    }
    // Existing Scene object quota bounds metadata too. Exhaustion leaves the
    // fully validated native data untouched; it never fabricates an alias.
    if(entries_.size()<256)entries_.push_back({key,cells});
  }
public:
  // Scope only the current native Scene read, including nested reads and
  // exceptions. Other threads and standalone Volume reads keep their own data.
  NativeSceneGeometryIntern():previous_(current_){entries_.reserve(256);current_=this;}
  ~NativeSceneGeometryIntern(){current_=previous_;}
  NativeSceneGeometryIntern(const NativeSceneGeometryIntern&)=delete;
  NativeSceneGeometryIntern&operator=(const NativeSceneGeometryIntern&)=delete;
  static void intern_decoded(Cells &cells,float voxel,float band) {
    if(current_)current_->intern(cells,voxel,band);
  }
};
}
