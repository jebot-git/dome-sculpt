// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "weight-declared-support.hpp"
#include <map>
#include <set>
namespace dome::detail {
// Pure geometry classification, not topology repair. Coincident, co-moving
// boundary points on DIFFERENT already edge-sewn surface components identify
// a nonmanifold/touching assembly. No rows are joined by this detector.
struct LocalizedAssemblyContact {size_t point_groups=0,bytes=0;};
inline LocalizedAssemblyContact localized_assembly_contacts(
 const Mesh&m,std::span<const uint32_t>sewn,const MorphSet*morphs,
 size_t maximum_bytes,weight_surface::Budget&work){
 work.stop();
 const size_t n=m.positions.size(),corners=m.indices.size(),targets=morphs?morphs->targets.size():0;
 if(!n||n>2'000'000||corners%3||corners>6'000'000||sewn.size()!=n||targets>128)
   throw std::invalid_argument("Localized assembly geometry dimensions");
 const size_t bytes=n*(320+targets*24)+corners*96+4096;
 if(bytes>maximum_bytes)throw std::length_error("Localized assembly bytes before construction");
 work.step_items(n+corners,16);if(morphs)morphs->validate(m);
 size_t local=0;auto component=heat_intent_components(m,sewn,local,work.maximum-work.used,work.cancel);work.step(local);
 std::map<std::array<uint32_t,2>,size_t>edges;
 for(size_t c=0;c<corners;c+=3)for(size_t k=0;k<3;++k){work.step(16);auto a=m.indices[c+k],b=m.indices[c+(k+1)%3];if(a>=n||b>=n)throw std::invalid_argument("Localized assembly indices");++edges[{std::min(a,b),std::max(a,b)}];}
 std::vector<uint8_t>boundary(n);for(const auto&[e,count]:edges){work.step();if(count==1)boundary[e[0]]=boundary[e[1]]=1;}
 struct Less{weight_surface::Budget*w;bool operator()(const std::vector<uint32_t>&a,const std::vector<uint32_t>&b)const{w->step(2+a.size()+b.size());return a<b;}};
 std::map<std::vector<uint32_t>,uint32_t,Less>owner(Less{&work});std::set<std::vector<uint32_t>,Less>ambiguous(Less{&work});
 for(uint32_t v=0;v<n;++v)if(boundary[v]){
   std::vector<uint32_t>key;key.reserve(3*(targets+1));auto add=[&](Vec3 p){for(float f:{p.x,p.y,p.z}){work.step();if(!std::isfinite(f))throw std::invalid_argument("Localized assembly nonfinite geometry");key.push_back(std::bit_cast<uint32_t>(f==0?0.f:f));}};add(m.positions[v]);if(morphs)for(const auto&t:morphs->targets)add(t.delta[v]);
   auto[it,inserted]=owner.try_emplace(key,component[v]);if(!inserted&&it->second!=component[v])ambiguous.insert(std::move(key));
 }
 work.stop();return{ambiguous.size(),bytes};
}
// Current authenticated skeleton hierarchy, not display labels or an outcome
// test. Broad articulated surfaces and nonhumanoid control usage remain direct
// support. A local assembly fits one participating handle's parent/child star.
inline bool one_anatomical_articulation_star(const HeatJointMask&mask,
 const Skeleton&rig,const HumanoidMapping&humanoid,weight_surface::Budget&work){
 work.stop();
 if(rig.bones.size()>256)throw std::invalid_argument("Anatomical star control limit");
 HeatJointMask standard{};for(const auto&[role,j]:humanoid.joints){(void)role;work.step();if(j>=rig.bones.size())throw std::invalid_argument("Anatomical role outside current rig");standard[j/64]|=uint64_t(1)<<(j%64);}
 bool any=false;for(size_t k=0;k<4;++k){work.step();if(mask[k]&~standard[k])return false;any|=mask[k]!=0;}if(!any)return false;
 for(uint32_t center=0;center<rig.bones.size();++center)if(mask[center/64]&(uint64_t(1)<<(center%64))){bool star=true;for(uint32_t j=0;j<rig.bones.size();++j){work.step();if(mask[j/64]&(uint64_t(1)<<(j%64)))star&=j==center||rig.bones[j].parent==int(center)||rig.bones[center].parent==int(j);}if(star)return true;}
 return false;
}
} // namespace dome::detail
