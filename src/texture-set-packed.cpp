// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-packed.hpp"
#include <bit>
#include <map>
namespace dome::detail {
namespace {
void stop(const TextureSetLimits&l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Texture-set attribute layout cancelled");}
void charge(size_t&t,size_t n,size_t unit,size_t maximum){if(t>maximum||(unit&&n>(maximum-t)/unit))throw std::length_error("Texture-set packed geometry exceeds its working bound");t+=n*unit;}
bool field_canonical(const TextureSetView&field){return field.painted->baked()&&!field.painted->baked()->maps.normal.rgba.empty();}
template<class T>bool owner(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
}
std::shared_ptr<const TextureSetPackedGeometry> prepare_texture_set_packed(
    const std::shared_ptr<const TextureSetSurfacePlan>&plan,const TextureSetLimits&limits){
 limits.validate();stop(limits);if(!plan)throw std::invalid_argument("Texture-set layout needs its genuine current field plan");plan->authenticate_owner(plan);
 const auto&rest=*plan->geometry();const auto count=plan->indices().size();size_t normal_corners=0;
 for(size_t f=0;f<plan->fields().size();++f)if(field_canonical(plan->fields()[f]))normal_corners+=plan->draws()[f].count;
 size_t work=plan->preparation_work();charge(work,count,8,limits.maximum_work);charge(work,rest.positions.size(),8,limits.maximum_work);
 charge(work,normal_corners,2*std::bit_width(normal_corners)+24,limits.maximum_work);
 if(plan->deformation()){
  const auto entries=plan->deformation()->resource_entries();if(entries>8'000'000)throw std::length_error("Texture-set pose exceeds its native deformation bound");
  charge(work,entries,128,limits.maximum_work);plan->deformation()->validate(rest);
 }
 size_t bytes=2*sizeof(TextureSetPackedGeometry)+128;
 charge(bytes,count,3*sizeof(uint32_t)+sizeof(Tangent),limits.maximum_bytes);
 charge(bytes,rest.positions.size(),sizeof(uint32_t),limits.maximum_bytes);
 // A complete signature uses only necessary source-vertex/tangent splits.
 // Reserve bounded comparison-tree nodes before Mikk/layout allocation.
 charge(bytes,normal_corners,10*sizeof(void*)+6*sizeof(uint32_t),limits.maximum_bytes);
 if(normal_corners)charge(bytes,rest.indices.size(),sizeof(Tangent),limits.maximum_bytes);
 if(plan->deformation()){
  charge(bytes,rest.positions.size()+rest.normals.size(),sizeof(Vec3),limits.maximum_bytes);
  charge(bytes,rest.uv.size(),sizeof(std::array<float,2>),limits.maximum_bytes);charge(bytes,rest.indices.size(),sizeof(uint32_t),limits.maximum_bytes);
  charge(bytes,rest.uv_pins.size()+rest.uv_seams.size(),1,limits.maximum_bytes);
 }
 auto layout=std::shared_ptr<TextureSetPackedGeometry>(new TextureSetPackedGeometry);
 layout->bytes_=bytes;layout->work_=work;layout->object_=plan->object();layout->rest_=plan->geometry();layout->deformation_=plan->deformation();layout->membership_=plan->collection()->primary_domain();
 layout->posed_=layout->deformation_?std::make_shared<const Mesh>(layout->deformation_->preview(rest)):layout->rest_;stop(limits);
 const auto frames=normal_corners?tangent_corners(*layout->posed_,limits.cancelled):std::vector<Tangent>{};
 layout->draws_.assign(plan->draws().begin(),plan->draws().end());layout->corners_.assign(plan->logical_corners().begin(),plan->logical_corners().end());
 layout->indices_.reserve(count);layout->vertices_.reserve(count);layout->tangents_.reserve(count);layout->canonical_.reserve(plan->fields().size());
 std::vector<uint32_t>common(rest.positions.size(),UINT32_MAX);
 using Signature=std::array<uint32_t,5>;std::map<Signature,uint32_t>split;
 const Tangent plain{1,0,0,1};const std::array<uint32_t,4>plain_bits{std::bit_cast<uint32_t>(1.f),0,0,std::bit_cast<uint32_t>(1.f)};
 auto append=[&](uint32_t vertex,Tangent tangent){const auto index=uint32_t(layout->vertices_.size());layout->vertices_.push_back(vertex);layout->tangents_.push_back(tangent);return index;};
 for(size_t f=0;f<plan->fields().size();++f){
  const bool normal=field_canonical(plan->fields()[f]);layout->canonical_.push_back(normal);const auto&draw=plan->draws()[f];
  for(size_t c=0;c<draw.count;++c){if((c&1023)==0)stop(limits);const auto logical=plan->logical_corners()[draw.first+c];const auto vertex=rest.indices.at(logical);
   const auto tangent=normal?frames.at(logical):plain;
   const std::array<uint32_t,4>bits{std::bit_cast<uint32_t>(tangent[0]),std::bit_cast<uint32_t>(tangent[1]),std::bit_cast<uint32_t>(tangent[2]),std::bit_cast<uint32_t>(tangent[3])};
   uint32_t index=UINT32_MAX;
   if(bits==plain_bits){auto&existing=common.at(vertex);if(existing==UINT32_MAX)existing=append(vertex,tangent);index=existing;}
   else{Signature key{vertex,bits[0],bits[1],bits[2],bits[3]};auto found=split.find(key);if(found!=split.end())index=found->second;else{index=append(vertex,tangent);split.emplace(key,index);}}
   layout->indices_.push_back(index);
  }
 }
 if(layout->indices_.size()!=count||layout->vertices_.size()>count)throw std::invalid_argument("Texture-set attribute layout lost its actual corner partition");
 stop(limits);std::shared_ptr<const TextureSetPackedGeometry>sealed(new const TextureSetPackedGeometry(std::move(*layout)));sealed->self_=sealed;return sealed;
}
void TextureSetPackedGeometry::authenticate_owner(const std::shared_ptr<const TextureSetPackedGeometry>&value)const{
 const auto expected=self_.lock();if(!expected||!owner(expected,value))throw std::invalid_argument("Texture-set packed layout borrowed another control block");
}
bool TextureSetPackedGeometry::matches_plan(const TextureSetSurfacePlan&plan)const{
 if(object_!=plan.object()||!owner(rest_,plan.geometry())||!owner(deformation_,plan.deformation())||!owner(membership_,plan.collection()->primary_domain())||draws_.size()!=plan.draws().size()||canonical_.size()!=plan.fields().size())
  return false;
 for(size_t f=0;f<draws_.size();++f)if(draws_[f].set!=plan.draws()[f].set||draws_[f].first!=plan.draws()[f].first||draws_[f].count!=plan.draws()[f].count||bool(canonical_[f])!=field_canonical(plan.fields()[f]))return false;
 return true;
}
void TextureSetPackedGeometry::authenticate_plan(const TextureSetSurfacePlan&plan)const{
 if(!matches_plan(plan))throw std::invalid_argument("Texture-set layout differs from its exact geometry, membership, pose or frame roles");
}
} // namespace dome::detail
