// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/normal-preview.hpp"
#include "dome/material-bindings.hpp"
#include "normal-preparation-state.hpp"
#include <bit>
namespace dome {namespace {
template<class T>bool owner(const std::shared_ptr<const T> &a,const std::shared_ptr<const T> &b)noexcept{return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
bool receipt(const NormalPreparationReceipt &a,const NormalPreparationReceipt &b)noexcept {
 return a.version==b.version&&a.resolution==b.resolution&&a.pbr==b.pbr&&a.mtoon==b.mtoon&&
   a.authored_native_normals==b.authored_native_normals&&a.authored_native_tangents==b.authored_native_tangents&&a.sampled_texels==b.sampled_texels&&
   std::bit_cast<uint64_t>(a.accepted_angular_error)==std::bit_cast<uint64_t>(b.accepted_angular_error)&&std::bit_cast<uint64_t>(a.observed_initial_error)==std::bit_cast<uint64_t>(b.observed_initial_error);
}
}
NormalPreviewProof::NormalPreviewProof(std::shared_ptr<const SourceMaterialBindings> binding,std::shared_ptr<const TextureMaps> maps)
 :binding_(std::move(binding)),maps_(std::move(maps)),recipe_(binding_->normal_preparation),source_(*recipe_),maps_projection_(maps_->normal_projection),baked_(maps_->baked_base){}
bool NormalPreviewProof::matches(const std::shared_ptr<const SourceMaterialBindings> &binding,const std::shared_ptr<const TextureMaps> &maps)const noexcept {
 if(!owner(binding,binding_)||!owner(maps,maps_)||!binding||!maps||!owner(binding->normal_preparation,recipe_))return false;
 const auto &p=*recipe_;
 return p.sampling_domain&&p.sampling_domain->has_sealed_metadata()&&receipt(p.receipt,source_.receipt)&&
   owner(p.projection,source_.projection)&&owner(p.initial_projection,source_.initial_projection)&&owner(p.target_frames,source_.target_frames)&&owner(p.original_paint,source_.original_paint)&&
   owner(p.original_appearance,source_.original_appearance)&&owner(p.original_deformation,source_.original_deformation)&&owner(p.original_material,source_.original_material)&&owner(p.uv_reprojection,source_.uv_reprojection)&&owner(p.sampling_domain,source_.sampling_domain)&&
   owner(maps->normal_projection,maps_projection_)&&owner(maps->baked_base,baked_)&&!maps->source_only;
}
std::shared_ptr<const NormalPreviewProof> prepare_normal_preview(const PaintedMesh &paint,std::shared_ptr<const SourceMaterialBindings> binding,const Material &material,const std::function<bool()> &cancelled){
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Normal preview authentication cancelled");};stop();
 const auto recipe=binding?binding->normal_preparation:nullptr;
 const auto node=recipe?recipe->sampling_domain:nullptr;
 if(!node||!node->metadata_sampling_domain||!node->has_sealed_metadata())return {};
 binding->validate(false);
 // The seal proves the normal node's own before/layout replay. Independently
 // bind that layout and event to this actual current paint before minting a
 // render proof; a staged colour/UV derivative may still carry the old normal.
 node->validate_current(paint.mesh(),nullptr,cancelled);
 node->validate_paint(paint,*binding,true,cancelled);
 auto maps=paint.preview(material);
 detail::prepared_normal_state(binding.get(),maps.get(),false);
 if(!detail::normal_projection_identity(*maps->normal_projection,*node->projection))throw std::invalid_argument("Normal preview differs from its independently sealed field");
 const auto &domain=*node->projection->sampling_domain;
 const bool baked_detail=maps->baked_base&&!maps->baked_base->maps.normal.rgba.empty();
 if(baked_detail&&!detail::same_normal_detail(domain.detail.get(),maps->baked_base.get(),recipe->receipt.resolution,true))throw std::invalid_argument("Normal preview lost exact normal-specific metadata");
 const auto pbr=baked_detail?domain.resolved_pbr:node->projection->pbr;
 const auto toon=baked_detail?domain.resolved_mtoon:node->projection->mtoon;
 if(maps->normal.width!=pbr->width||maps->normal.height!=pbr->height||maps->normal.rgba!=pbr->rgba||maps->mtoon_normal.width!=toon->width||maps->mtoon_normal.height!=toon->height||maps->mtoon_normal.rgba!=toon->rgba)
   throw std::invalid_argument("Normal preview pixels differ from their independent retained interpretation");
 stop();auto proof=std::shared_ptr<const NormalPreviewProof>(new NormalPreviewProof(std::move(binding),std::move(maps)));stop();return proof;
}
} // namespace dome
