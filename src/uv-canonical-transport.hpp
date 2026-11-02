// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh.hpp"
#include <algorithm>
#include <functional>
#include <span>
namespace dome::detail {
// A conservative, exact translation certificate for the dynamically generated
// canonical Mikk basis. Every triangle retains its floating-point UV differences
// and the global UV equality partition is unchanged. Thus any posed position /
// normal equality can create precisely the same Mikk welds, not just rest welds.
// It does not certify scaling, rotation, seams introduced/merged, or resampling.
inline bool preserves_canonical_uv_derivatives(const Mesh &before,const Mesh &after,
 std::span<const uint32_t> remap,size_t max_bytes,
 const std::function<bool()> &cancelled={}) {
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Canonical UV transport cancelled");};stop();
 if(remap.size()!=after.positions.size()||before.indices.size()!=after.indices.size()||before.uv.size()!=before.positions.size()||after.uv.size()!=after.positions.size())return false;
 for(size_t c=0;c<after.indices.size();c+=3){if((c&1023)==0)stop();
  for(size_t k=0;k<3;++k)if(remap[after.indices[c+k]]!=before.indices[c+k])return false;
  for(size_t k=1;k<3;++k)for(size_t d=0;d<2;++d)
   if(before.uv[before.indices[c+k]][d]-before.uv[before.indices[c]][d]!=after.uv[after.indices[c+k]][d]-after.uv[after.indices[c]][d])return false;
 }
 using Row=std::pair<std::array<float,2>,std::array<float,2>>;
 if(remap.size()>max_bytes/sizeof(Row))throw std::length_error("Canonical UV certificate exceeds its working budget");
 std::vector<Row> rows;rows.reserve(remap.size());for(size_t v=0;v<remap.size();++v){if((v&1023)==0)stop();if(remap[v]>=before.uv.size())return false;rows.emplace_back(before.uv[remap[v]],after.uv[v]);}
 size_t comparisons=0;auto poll=[&]{if((++comparisons&4095)==0)stop();};
 std::sort(rows.begin(),rows.end(),[&](const Row&a,const Row&b){poll();return a.first<b.first;});
 for(size_t v=1;v<rows.size();++v){if((v&1023)==0)stop();if(rows[v].first==rows[v-1].first&&rows[v].second!=rows[v-1].second)return false;}
 std::sort(rows.begin(),rows.end(),[&](const Row&a,const Row&b){poll();return a.second<b.second;});
 for(size_t v=1;v<rows.size();++v){if((v&1023)==0)stop();if(rows[v].second==rows[v-1].second&&rows[v].first!=rows[v-1].first)return false;}
 stop();return true;
}
}
