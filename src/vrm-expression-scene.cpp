// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/vrm-expression.hpp"
#include "dome/source-placement.hpp"
#include "dome/material-bindings.hpp"
#include "source-paint-parameters.hpp"
#include "import-texture-transform.hpp"
#include "vrm-painted.hpp"
#include <set>
#include <limits>
namespace dome {
namespace {
using J=nlohmann::json;
std::shared_ptr<const SourceNodePlacement> placement(const Scene &s,const SculptObject &o){
 std::shared_ptr<const SourceNodePlacement> result;
 for(const auto &g:s.group_path(o.group))if(g.source_placement)result=g.source_placement;
 if(!result||!result->model||!o.appearance||result->model->asset!=o.appearance->asset||
    result->model->original_bytes!=o.appearance->original_bytes||o.appearance->source_node<0||
    result->source_node!=uint32_t(o.appearance->source_node))
  throw std::invalid_argument("VRM1 expression needs authenticated source-node placement");
 result->validate();const auto path=s.group_path(o.group);
 if(std::none_of(path.begin(),path.end(),[&](const auto &g){return g.id==result->source_instance;}))
  throw std::invalid_argument("VRM1 expression source-instance root is not live in its owning path");
 return result;
}
J original(const CreatorSourceModel &model,const std::function<bool()> &cancelled){
 if(!model.asset||model.asset->avatar_version!=ImportedAvatarVersion::Vrm1||!model.original_bytes)
  throw std::invalid_argument("Static VRM1 expressions require actual retained VRM1.0 source");
 const auto &bytes=*model.original_bytes;
 auto word=[&](size_t n){if(n>bytes.size()||bytes.size()-n<4)throw std::invalid_argument("VRM1 source GLB is truncated");return uint32_t(bytes[n])|(uint32_t(bytes[n+1])<<8)|(uint32_t(bytes[n+2])<<16)|(uint32_t(bytes[n+3])<<24);};
 if(bytes.size()<28||word(0)!=0x46546c67||word(4)!=2||word(8)!=bytes.size()||word(16)!=0x4e4f534a)
  throw std::invalid_argument("VRM1 expression source is not its original captured GLB");
 const size_t size=word(12);if(size>16*1024*1024||size>bytes.size()-20)throw std::length_error("VRM1 expression source metadata exceeds bounds");
 size_t visits=0;return J::parse(bytes.begin()+20,bytes.begin()+20+size,[&](int depth,J::parse_event_t,const J&){
  if(depth>128||++visits>1024*1024)throw std::length_error("VRM1 expression source metadata work exceeds bounds");
  if((visits&1023)==0&&cancelled&&cancelled())throw std::runtime_error("VRM1 expression source read cancelled");return true;});
}
std::array<float,3> array(Vec3 v){return {v.x,v.y,v.z};}
Vec3 vector(std::array<float,3> v){return {v[0],v[1],v[2]};}
template<size_t N>std::array<float,N> color(std::array<float,N> initial,const std::vector<Vrm1ExpressionColor> &binds,uint32_t material,std::string_view type){
 auto result=initial;for(size_t k=0;k<N;++k){double value=initial[k];for(const auto &bind:binds)if(bind.material==material&&bind.type==type)value+=(double(bind.target[k])-initial[k])*bind.weight;
  if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())throw std::invalid_argument("VRM1 expression color is outside finite native representation");result[k]=float(value);}
 return result;
}
std::array<float,6> uv(std::array<float,6> initial,float rotation,const std::vector<Vrm1ExpressionUv> &binds,uint32_t material){
 double weight=0;std::array<double,2> scale{},offset{};
 for(const auto &b:binds)if(b.material==material){weight+=b.weight;for(size_t k=0;k<2;++k){scale[k]+=double(b.scale[k])*b.weight;offset[k]+=double(b.offset[k])*b.weight;}}
 const double c=std::cos(double(rotation)),s=std::sin(double(rotation));
 return {float(initial[0]*(1-weight)+c*scale[0]),float(initial[1]*(1-weight)-s*scale[1]),
         float(initial[2]*(1-weight)+s*scale[0]),float(initial[3]*(1-weight)+c*scale[1]),
         float(initial[4]*(1-weight)+offset[0]),float(initial[5]*(1-weight)+offset[1])};
}
}
std::vector<Vrm1ExpressionDescription> describe_vrm1_expressions(const Scene &scene,uint64_t object,
 const std::function<bool()> &cancelled){
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("VRM1 expression catalog cancelled");};stop();
 const auto owner=placement(scene,scene.object(object));const auto json=original(*owner->model,cancelled);
 const auto evaluated=evaluate_vrm1_expressions(json,{},cancelled);(void)evaluated;
 std::vector<Vrm1ExpressionDescription> result;
 const auto definitions=json.at("extensions").at("VRMC_vrm").value("expressions",J::object());
 for(bool custom:{false,true}){
  const auto table=definitions.value(custom?"custom":"preset",J::object());
  for(const auto &entry:table.items()){stop();result.push_back({entry.key(),custom,entry.value().value("isBinary",false)});}
 }
 stop();return result;
}
Scene bake_vrm1_expression_pose(const Scene &baseline,uint64_t object,
 const Vrm1ExpressionWeights &weights,const std::function<bool()> &cancelled){
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Static VRM1 expression authoring cancelled");};stop();
 baseline.validate();const auto owner=placement(baseline,baseline.object(object));const auto json=original(*owner->model,cancelled);
 const auto evaluated=evaluate_vrm1_expressions(json,weights,cancelled);Scene result=baseline.snapshot();
 std::set<uint32_t> driven_nodes;
 for(const auto &entry:baseline.objects()){
  stop();if(!entry.appearance)continue;
  // Numerical instance IDs alone never authorize edits to another model.
  const auto path=baseline.group_path(entry.group);std::shared_ptr<const SourceNodePlacement> candidate;
  for(const auto &g:path)if(g.source_placement)candidate=g.source_placement;
  if(!candidate||candidate->model!=owner->model||candidate->source_instance!=owner->source_instance)continue;
  const auto authenticated=placement(baseline,entry);(void)authenticated;
  auto &current=result.object(entry.id);const auto &a=*entry.appearance;
  if(!entry.mesh_active||!entry.painted||source_is_dormant(entry))throw std::invalid_argument("Static VRM1 expression needs live source geometry");
  if(entry.locked)throw std::invalid_argument("Static VRM1 expression cannot edit a locked linked source part");
  driven_nodes.insert(uint32_t(a.source_node));std::set<uint32_t> seen;
  if(entry.deformation&&entry.deformation->morphs){
   auto deformation=std::make_shared<DeformationState>(*entry.deformation);const size_t n=deformation->morphs->targets.size();
   auto &values=deformation->morph_weights;if(values.empty())values.resize(n);
   for(size_t t=0;t<n;++t){stop();std::optional<uint32_t> source;
    if(entry.material_bindings&&entry.material_bindings->normal_frames){const auto &frames=*entry.material_bindings->normal_frames;
     if(frames.current_targets.size()!=n)throw std::invalid_argument("VRM1 expression morph frame identity is stale");
     const auto at=std::find_if(frames.targets.begin(),frames.targets.end(),[&](const auto &q){return q.id==frames.current_targets[t];});
     if(at==frames.targets.end())throw std::invalid_argument("VRM1 expression morph has no actual frame target");source=at->source_target;
    }else{if(a.source_morph_targets.size()!=n)throw std::invalid_argument("VRM1 expression requires explicit original morph identities");source=a.source_morph_targets[t];}
    if(!source)continue; // Native-only authored targets retain their current weight.
    if(!seen.insert(*source).second)throw std::invalid_argument("VRM1 expression has conflicting original morph identities");
    const auto at=evaluated.morphs.find({uint32_t(a.source_node),*source});values[t]=at==evaluated.morphs.end()?0:at->second;
   }
   deformation->validate(entry.painted->mesh());current.deformation=std::move(deformation);
  }
  for(const auto &[target,weight]:evaluated.morphs)
   if(weight!=0&&target.first==uint32_t(a.source_node)&&!seen.contains(target.second))
    throw std::invalid_argument("Static VRM1 expression original morph target is absent from a live source primitive");
  if(a.source_material<0)continue;const auto material=uint32_t(a.source_material);
  const bool colors=std::any_of(evaluated.colors.begin(),evaluated.colors.end(),[&](const auto &b){return b.material==material&&b.weight!=0;});
  const bool textures=std::any_of(evaluated.textures.begin(),evaluated.textures.end(),[&](const auto &b){return b.material==material&&b.weight!=0;});
  if(!colors&&!textures)continue;
  const auto binding=entry.material_bindings;
  if(!binding||!binding->has_mtoon||binding->material.version()!=MToonVersion::V1)
   throw std::invalid_argument("Static VRM1 plain-PBR material authoring needs its separate native factor adapter");
  if(binding->color_preparation||binding->normal_preparation||binding->key_derivation)
   throw std::invalid_argument("Static VRM1 prepared material expressions need an authenticated before-state parameter adapter");
  auto edited=std::make_shared<SourceMaterialBindings>(*binding);auto &parameters=edited->material.parameters;
  // Fold the current inspector correction into the initial source term only.
  // Paint remains independent and Material stays exact, including unit intent.
  for(size_t k=0;k<4;++k)parameters.base_color[k]=detail::vrm_native_factor(parameters.base_color[k],binding->native_reference.color[k],entry.material.color[k]);
  auto emission=array(parameters.emission);for(size_t k=0;k<3;++k)emission[k]=detail::vrm_native_factor(emission[k],binding->native_reference.emission[k],entry.material.emissive[k]);
  if(binding->physical_emission&&std::any_of(evaluated.colors.begin(),evaluated.colors.end(),[&](const auto &b){return b.material==material&&b.type=="emissionColor"&&b.weight!=0;}))
   throw std::invalid_argument("Static VRM1 emission expression needs its explicit absolute-background adapter");
  parameters.base_color=color(parameters.base_color,evaluated.colors,material,"color");
  parameters.emission=vector(color(emission,evaluated.colors,material,"emissionColor"));
  parameters.shade_color=vector(color(array(parameters.shade_color),evaluated.colors,material,"shadeColor"));
  parameters.matcap_color=vector(color(array(parameters.matcap_color),evaluated.colors,material,"matcapColor"));
  parameters.rim_color=vector(color(array(parameters.rim_color),evaluated.colors,material,"rimColor"));
  parameters.outline_color=vector(color(array(parameters.outline_color),evaluated.colors,material,"outlineColor"));
  edited->native_reference.color=entry.material.color;edited->native_reference.emission=entry.material.emissive;
  if(textures){
   if(binding->roles[size_t(MToonSlot::Normal)].image)
    throw std::invalid_argument("Static VRM1 normal-texture expressions require exact tangent-frame transport");
   if(!entry.painted->source_sampling_prepared())throw std::invalid_argument("Static VRM1 UV authoring requires a genuine independent source paint owner");
   for(size_t slot=0;slot<edited->material.textures.size();++slot){if(slot==size_t(MToonSlot::Matcap))continue;auto &texture=edited->material.textures[slot];
    const auto initial=texture;double w=0;std::array<double,2> scale{},offset{};
    for(const auto &b:evaluated.textures)if(b.material==material){w+=b.weight;for(size_t k=0;k<2;++k){scale[k]+=double(b.scale[k])*b.weight;offset[k]+=double(b.offset[k])*b.weight;}}
    for(size_t k=0;k<2;++k){texture.scale[k]=float(initial.scale[k]*(1-w)+scale[k]);texture.offset[k]=float(initial.offset[k]*(1-w)+offset[k]);}
   }
   const auto before=entry.painted->source_sampling_original();const auto maps=before->source_maps();if(!maps)throw std::invalid_argument("VRM1 expression lost original source role bundle");
   const auto &raw=owner->model->asset->materials.at(size_t(a.source_material)+1);
   const std::array<float,5> rotation{raw.color_transform.rotation,raw.metal_rough_transform.rotation,raw.emissive_transform.rotation,raw.normal_transform.rotation,raw.occlusion_transform.rotation};
   const std::array<const PaintSourceTexture*,5> views{&maps->color,&maps->metal_rough,&maps->emission,&maps->normal,&maps->occlusion};
   if(maps->normal.image)throw std::invalid_argument("Static VRM1 PBR normal-texture expressions require exact tangent-frame transport");
   std::array<std::array<float,6>,5> transforms;for(size_t k=0;k<5;++k)transforms[k]=views[k]->image?uv(views[k]->uv_transform,rotation[k],evaluated.textures,material):views[k]->uv_transform;
   SourcePaintOptions options;options.cancelled=cancelled;current.painted=detail::rebind_source_paint_transforms(*entry.painted,transforms,options);
  }
  edited->validate();current.material_bindings=std::move(edited);
 }
 for(const auto &[id,weight]:evaluated.morphs)if(weight!=0&&!driven_nodes.contains(id.first))throw std::invalid_argument("Static VRM1 expression target node is absent from its authenticated live instance");
 stop();result.validate();stop();return result;
}
}
