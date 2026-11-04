// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/creator-archive.hpp"
#include "dome/source-placement.hpp"
#include "dome/scene.hpp"
#include "weight-surface-system.hpp"
#include <set>
namespace dome::detail {
struct NativeAnatomicalDomain {uint64_t group=0;std::vector<uint64_t> objects;};
// Owning-worker scope, derived from genuine current hierarchy and exact source
// owners/primitive identities. The deepest complete enclosing group selects ONE
// current instance even when a user later groups several instances together.
// No original affine/node provenance is invented for legacy current groups.
inline NativeAnatomicalDomain native_anatomical_domain(const Scene&scene,
 std::span<const uint64_t>selected,weight_surface::Budget&work){
 if(selected.empty()||selected.size()>64||scene.objects().size()>128||scene.groups().size()>128)
   throw std::invalid_argument("Native anatomical bounded current source scope");
 const auto&first=scene.object(selected.front());
 if(!first.appearance||!first.appearance->asset||!first.appearance->original_bytes||!first.group)
   throw std::invalid_argument("Native anatomical targets need genuine grouped current source geometry");
 const auto&asset=first.appearance->asset;const auto&bytes=first.appearance->original_bytes;
 if(asset->primitives.empty()||asset->primitives.size()>64)
   throw std::length_error("Native anatomical complete primitive count");
 auto same=[](const auto&a,const auto&b){return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);};
 std::set<std::pair<int,int>>expected;
 for(const auto&p:asset->primitives){work.step(32);if(p.source_mesh<0||p.source_primitive<0||!expected.emplace(p.source_mesh,p.source_primitive).second)
   throw std::invalid_argument("Native anatomical retained primitive identities are ambiguous");}
 // These bounded vectors fit the capture planner's per-object/per-group
 // allowance; all source arrays/images remain shared rather than copied.
 std::vector<std::vector<uint64_t>>paths;paths.reserve(scene.objects().size());
 std::set<uint64_t>chosen;
 for(auto id:selected){work.step(32);if(!chosen.insert(id).second)throw std::invalid_argument("Duplicate anatomical output target");const auto&o=scene.object(id);
   if(!o.appearance||!same(o.appearance->asset,asset)||!same(o.appearance->original_bytes,bytes)||!o.group)
     throw std::invalid_argument("Anatomical output targets cross current source owners");}
 size_t first_index=SIZE_MAX;
 for(size_t i=0;i<scene.objects().size();++i){const auto&o=scene.objects()[i];work.step(32);std::vector<uint64_t>path;
   if(o.group)for(const auto&g:scene.group_path(o.group)){work.step(32);path.push_back(g.id);}
   if(o.id==first.id)first_index=i;paths.push_back(std::move(path));}
 if(first_index==SIZE_MAX||paths[first_index].empty())throw std::invalid_argument("Anatomical current target group path missing");
 NativeAnatomicalDomain result;
 const auto&ancestors=paths[first_index];
 for(size_t level=ancestors.size();level;--level){
   const auto candidate=ancestors[level-1];std::set<std::pair<int,int>>present;std::vector<uint64_t>members;size_t selected_members=0;bool valid=true;
   for(size_t i=0;i<scene.objects().size();++i){work.step(32+paths[i].size()*4);const auto&o=scene.objects()[i];
     if(std::find(paths[i].begin(),paths[i].end(),candidate)==paths[i].end())continue;
     if(chosen.contains(o.id))++selected_members;
     if(!o.mesh_active||!o.painted||!o.appearance)continue;const auto&a=*o.appearance;
     if(!same(a.asset,asset)||!same(a.original_bytes,bytes))continue;
     work.step(32);const auto key=std::pair{a.source_mesh,a.source_primitive};
     if(!expected.contains(key)||!present.insert(key).second){valid=false;continue;}members.push_back(o.id);
   }
   if(valid&&selected_members==chosen.size()&&present==expected){result={candidate,std::move(members)};break;}
 }
 if(!result.group)throw std::invalid_argument("Native anatomical bind needs one complete unambiguous current source instance");
 std::optional<uint64_t>instance;std::shared_ptr<const CreatorSourceModel>model;
 for(auto id:result.objects){work.step(64);const auto&o=scene.object(id);const auto&a=*o.appearance;const auto&node=scene.group(o.group).source_placement;
   if(node){if(!node->model||!node->source_instance||a.source_node<0||node->source_node!=uint32_t(a.source_node)||!same(node->model->asset,asset)||!same(node->model->original_bytes,bytes))
     throw std::invalid_argument("Anatomical current primitive contradicts its retained node/model placement");
     if(instance&&(*instance!=node->source_instance||!same(model,node->model)))throw std::invalid_argument("Anatomical complete current scope crosses retained source instances");
     instance=node->source_instance;model=node->model;}
 }
 return result;
}
} // namespace dome::detail
