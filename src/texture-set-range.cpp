// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-range.hpp"
#include "emission-paint-access.hpp"
#include "texture-set-field-source.hpp"
#include "polygon-surface-paint.hpp"
#include "polygon-surface-preview.hpp"
#include "dome/document.hpp"
#include "paint-overlay-packet.hpp"
#include "current-paint-preview.hpp"
#include <bit>
#include <limits>
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
    std::shared_ptr<const TextureSetSurfacePlan>plan,std::shared_ptr<const TextureSetPackedGeometry>layout,
    size_t field,size_t first,size_t vertex,const std::function<bool()>&cancelled,bool render_only){
 if(!plan)throw std::invalid_argument("Texture-set draw requires its actual owning plan");plan->authenticate_owner(plan);
 if(!layout)throw std::invalid_argument("Texture-set range requires its actual packed attribute layout");layout->authenticate_owner(layout);layout->authenticate_plan(*plan);
 if(field>=plan->fields().size()||field>=plan->draws().size())throw std::out_of_range("Unknown texture-set draw field");
 if(cancelled&&cancelled())throw std::runtime_error("Texture-set field preparation cancelled");
 const auto&view=plan->fields()[field];
 if(first>std::numeric_limits<size_t>::max()-plan->draws()[field].first||vertex>UINT32_MAX||layout->vertices().size()>uint64_t(UINT32_MAX)+1-vertex)throw std::length_error("Texture-set packed location exceeds native index bounds");
 auto result=std::shared_ptr<TextureSetRangeProof>(new TextureSetRangeProof);result->plan_=std::move(plan);result->field_=field;result->layout_=std::move(layout);result->first_=first+result->draw().first;result->vertex_=vertex;
 if(view.source){
  const auto read=view.source->read_view(view);result->maps_=PolygonSurfacePaintCodec::preview(*read,view.material_bindings,view.material,cancelled,view.source->source_preview());
  if(!result->maps_||!result->maps_->polygon_surface||!result->maps_->polygon_surface->matches(view.material_bindings,read->paint_sampling_attachment())||result->layout_->canonical(field))throw std::invalid_argument("Retained texture-set range lost its actual source/current field packet");
 }else{
  result->maps_=render_only?CurrentGridPaintPreview::for_field(result->plan_->object(),view,cancelled):nullptr;
  if(!result->maps_)result->maps_=EmissionExportAccess::requires_preview(*view.painted,view.material,view.material_bindings.get())?
    EmissionExportAccess::preview(*view.painted,view.material,cancelled):view.painted->preview(view.material,cancelled);
  if(!result->maps_||result->maps_->source_only||result->maps_->source_maps||result->maps_->normal_projection||result->maps_->color_projection||result->maps_->polygon_surface)throw std::invalid_argument("Texture-set complex field cannot borrow the plain range proof");
  if((!result->maps_->normal.rgba.empty())!=result->layout_->canonical(field))throw std::invalid_argument("Texture-set canonical tangent layout differs from its actual field");
 }
 if(const auto&packet=result->maps_->overlay){
  const auto&mesh=packet->native_geometry();
  if(!mesh||mesh!=result->plan_->geometry()||mesh.owner_before(result->plan_->geometry())||result->plan_->geometry().owner_before(mesh)||(view.source?packet->native_domain()!=view.source->read_view(view)->paint_sampling_domain():bool(packet->native_domain())))
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
 plan_->authenticate_owner(plan_);layout_->authenticate_owner(layout_);layout_->authenticate_plan(*plan_);
 if(r.first!=first_||r.first_vertex!=vertex_||r.object!=plan_->object()||r.texture_set!=field().id||r.count!=draw().count||!material(r.material,field().material)||r.textures!=maps_||r.textures.owner_before(maps_)||maps_.owner_before(r.textures)||
    r.appearance||r.normal_preview_proof||r.canonical_frame)
  throw std::invalid_argument("Texture-set range lost its own material, field, domain or role authority");
 if(field().source){
  field().source->authenticate_owner(field().source);field().source->validate_field(field(),{},false);
  const auto&packet=maps_->polygon_surface;const auto&binding=field().material_bindings;
  if(!packet||r.material_bindings!=binding||r.material_bindings.owner_before(binding)||binding.owner_before(r.material_bindings)||!packet->matches(binding,r.paint_sampling_attachment)||!r.source_frame||r.source_normal_only!=(packet->model()==PolygonSurfaceNormalModel::NativeNOnly))throw std::invalid_argument("Retained texture-set range changed its actual source/before-role owners");
 }else if(r.material_bindings||r.paint_sampling_attachment||r.source_frame||r.source_normal_only)throw std::invalid_argument("Plain texture-set range fabricated source roles");
}
void TextureSetRangeProof::authenticate_tint(const Mesh&mesh,const SurfaceRange&r,std::span<const std::array<float,4>>colors)const{
 authenticate(r);if(colors.empty())return;
 if(colors.size()!=mesh.positions.size()||r.first>mesh.indices.size()||r.count>mesh.indices.size()-r.first)throw std::invalid_argument("Texture-set tint stream differs from its exact geometry range");
 for(size_t c=0;c<r.count;++c){const auto vertex=mesh.indices.at(r.first+c);if(vertex>=colors.size()||colors[vertex]!=std::array<float,4>{1,1,1,1})throw std::invalid_argument("Plain texture-set field cannot borrow a foreign source/corner tint");}
}
void TextureSetRangeProof::authenticate_stream(const Mesh&mesh,const SurfaceRange&r)const{
 authenticate(r);
 if(r.first>mesh.indices.size()||r.count>mesh.indices.size()-r.first)throw std::invalid_argument("Texture-set stream index range exceeds its actual mesh");
 const auto&source=*plan_->geometry();
 for(size_t c=0;c<r.count;++c){
  const auto local=r.paint_winding_reversed&&c%3?c/3*3+3-c%3:c;
  const auto packed=layout_->indices()[draw().first+local];const auto actual=mesh.indices[r.first+c];
  if(uint64_t(vertex_)+packed!=actual||packed>=layout_->vertices().size()||actual>=mesh.uv.size())throw std::invalid_argument("Texture-set EBO differs from its sealed material/corner layout");
  const auto vertex=layout_->vertices()[packed];const auto logical=logical_corner(c,r.paint_winding_reversed);
  if(source.indices.at(logical)!=vertex)throw std::invalid_argument("Texture-set index map changed its actual logical corner identity");
  const auto expected=source.uv.at(vertex),value=mesh.uv.at(actual);
  for(size_t k=0;k<2;++k)if(std::bit_cast<uint32_t>(expected[k])!=std::bit_cast<uint32_t>(value[k]))throw std::invalid_argument("Texture-set stream lost its actual corner UV coordinates");
 }
}
} // namespace dome::detail
