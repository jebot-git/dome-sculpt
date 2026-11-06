// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-range.hpp"
#include "emission-paint-access.hpp"
#include "dome/document.hpp"
#include "paint-overlay-packet.hpp"
#include <bit>
namespace dome::detail {
namespace {
bool material(const Material&a,const Material&b){
 auto exact=[](float x,float y){return std::bit_cast<uint32_t>(x)==std::bit_cast<uint32_t>(y);};
 if(a.name!=b.name||a.toon!=b.toon||a.double_sided!=b.double_sided||a.alpha!=b.alpha||a.unlit!=b.unlit||!exact(a.alpha_cutoff,b.alpha_cutoff)||!exact(a.metallic,b.metallic)||!exact(a.roughness,b.roughness))return false;
 for(size_t k=0;k<4;++k)if(!exact(a.color[k],b.color[k]))return false;
 for(size_t k=0;k<3;++k)if(!exact(a.emissive[k],b.emissive[k]))return false;
 return true;
}
}
std::shared_ptr<const TextureSetRangeProof> prepare_texture_set_range(
    std::shared_ptr<const TextureSetSurfacePlan>plan,size_t field,const std::function<bool()>&cancelled){
 if(!plan)throw std::invalid_argument("Texture-set draw requires its actual owning plan");plan->authenticate_owner(plan);
 if(field>=plan->fields().size()||field>=plan->draws().size())throw std::out_of_range("Unknown texture-set draw field");
 if(cancelled&&cancelled())throw std::runtime_error("Texture-set field preparation cancelled");
 const auto&view=plan->fields()[field];
 auto result=std::shared_ptr<TextureSetRangeProof>(new TextureSetRangeProof);result->plan_=std::move(plan);result->field_=field;
 result->maps_=EmissionExportAccess::requires_preview(*view.painted,view.material,view.material_bindings.get())?
   EmissionExportAccess::preview(*view.painted,view.material,cancelled):view.painted->preview(view.material,cancelled);
 if(!result->maps_||result->maps_->source_only||result->maps_->source_maps||result->maps_->normal_projection||result->maps_->color_projection)
  throw std::invalid_argument("Texture-set complex field cannot borrow the plain range proof");
 if(const auto&packet=result->maps_->overlay){
  const auto&mesh=packet->native_geometry();
  if(!mesh||mesh!=result->plan_->geometry()||mesh.owner_before(result->plan_->geometry())||result->plan_->geometry().owner_before(mesh)||packet->native_domain())
   throw std::invalid_argument("Texture-set emission packet lost its independent actual geometry owner");
 }
 if(cancelled&&cancelled())throw std::runtime_error("Texture-set field preparation cancelled");
 std::shared_ptr<const TextureSetRangeProof>sealed(new const TextureSetRangeProof(std::move(*result)));sealed->self_=sealed;return sealed;
}
uint32_t TextureSetRangeProof::logical_corner(size_t corner,bool reversed)const{
 if(corner>=draw().count)throw std::out_of_range("Texture-set draw corner exceeds its actual partition");
 if(reversed&&corner%3)corner=corner/3*3+3-corner%3;
 return plan_->logical_corners()[draw().first+corner];
}
void TextureSetRangeProof::authenticate(const SurfaceRange&r)const{
 const auto self=self_.lock();
 if(!self||r.texture_set_proof!=self||r.texture_set_proof.owner_before(self)||self.owner_before(r.texture_set_proof))throw std::invalid_argument("Texture-set range borrowed a foreign proof control block");
 plan_->authenticate_owner(plan_);
 if(r.object!=plan_->object()||r.texture_set!=field().id||r.count!=draw().count||!material(r.material,field().material)||r.textures!=maps_||r.textures.owner_before(maps_)||maps_.owner_before(r.textures)||
    r.appearance||r.material_bindings||r.paint_sampling_attachment||r.normal_preview_proof||r.source_frame||r.canonical_frame||r.source_normal_only)
  throw std::invalid_argument("Texture-set range lost its own material, field, domain or role authority");
}
} // namespace dome::detail
