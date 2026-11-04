// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/color-preparation.hpp"
#include "dome/color-paint-sampling.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/material-visibility.hpp"
#include "normal-frame-runtime.hpp"
#include "paint-color-access.hpp"
#include "source-colors.hpp"
#include "color-projection-quality.hpp"
#include "color-bound-memo.hpp"
#include "source-paint-pairing.hpp"
#include "polygon-color-target.hpp"
#include <mutex>
namespace dome {
namespace {
uint64_t integer(const nlohmann::json &j,uint64_t maximum) {
  if((!j.is_number_unsigned() && (!j.is_number_integer() || j.get<int64_t>()<0)) || j.get<uint64_t>()>maximum)
    throw std::runtime_error("Color preparation integer is outside its declared range");
  return j.get<uint64_t>();
}
// Weak owners authenticate immutable cache entries without retaining old source
// avatars/images or permitting address-reuse hits. Receipts are small values.
struct InitialProof {
  std::weak_ptr<const PaintedMesh> paint;
  std::weak_ptr<const CreatorAppearance> appearance;
  std::weak_ptr<const PaintColorProjection> projection;
  std::weak_ptr<const PolygonColorSurface> polygon_colors;
  nlohmann::json receipt;
};
std::mutex proof_mutex;
std::deque<InitialProof> initial_proofs;
template<class T>bool same_owner(const std::weak_ptr<const T>&a,const std::shared_ptr<const T>&b){return a.lock()==b&&!a.owner_before(b)&&!b.owner_before(a);}
std::optional<detail::ColorBoundMemoKey> bound_key(
    const Mesh &mesh,const Mesh &current_mesh,const SourceColorPreparation &preparation,
    const CreatorAppearance *appearance,const PaintColorProjection &projection,
    const std::shared_ptr<const PolygonColorSurface> &descriptor,
    std::span<const std::array<float,4>> colors,bool initial,uint64_t max_cells) {
  if(!detail::color_bound_scope_active()||appearance||!descriptor||!descriptor->field||
     !descriptor->topology||projection.quality_cells<512)return {};
  std::shared_ptr<const Mesh> owner,current_owner;
  if(!initial&&preparation.paint_sampling){
    owner=preparation.paint_sampling->domain()->basis();
    current_owner=preparation.paint_sampling->geometry_attachment()->authored_geometry();
  }else if(preparation.original_paint)owner=current_owner=preparation.original_paint->mesh_owner();
  if(owner.get()!=&mesh||current_owner.get()!=&current_mesh)return {};
  // A borrowed/evaluated copy is not either genuine COW owner.
  std::shared_ptr<const PaintColorProjection> projected;
  if(preparation.projection.get()==&projection)projected=preparation.projection;
  else if(preparation.initial_projection.get()==&projection)projected=preparation.initial_projection;
  if(!projected||!projection.vertex_rgba)return {};
  detail::ColorBoundMemoKey key;key.mesh=std::move(owner);key.current_mesh=std::move(current_owner);key.field=descriptor->field;
  key.topology=descriptor->topology;key.projection=std::move(projected);key.image=projection.vertex_rgba;
  key.source_paint=preparation.original_paint;key.sampling=preparation.paint_sampling;
  key.material=preparation.original_material;key.deformation=preparation.original_deformation;
  key.recipe=&preparation;key.max_cells=max_cells;key.claimed_cells=projection.quality_cells;
  key.error_bits=std::bit_cast<std::array<uint32_t,4>>(projection.maximum_error);
  key.dimensions={key.image->width,key.image->height};key.source_field=uint32_t(preparation.receipt.source_field);key.initial=initial;
  key.image_content=detail::color_bound_digest(key.image->rgba);
  key.colors_content=detail::color_bound_digest({reinterpret_cast<const uint8_t*>(colors.data()),colors.size_bytes()});
  return key;
}
void verify_field(const Mesh &mesh,const SourceColorPreparation &preparation,const CreatorAppearance *appearance,const PaintColorProjection &projection,
                  const std::function<bool()> &cancelled,bool initial=false) {
  if(cancelled&&cancelled())throw std::runtime_error("Color projection validation cancelled");
  const auto &descriptor=initial?preparation.polygon_colors:preparation.sampling_descriptor();
  const auto &sampling_mesh=!initial&&preparation.paint_sampling?*preparation.paint_sampling->domain()->basis():mesh;
  if(descriptor&&!detail::source_vertex_colors(appearance,mesh.positions.size(),cancelled).empty())
    throw std::invalid_argument("Current corner projection conflicts with raw creator color authority");
  auto colors=descriptor
    ?detail::polygon_target_colors(sampling_mesh,*descriptor,cancelled)
    :preparation.vertex_field(mesh.positions.size(),appearance,cancelled);
  if(colors.empty())throw std::invalid_argument("Prepared color projection lost its raw original color authority");
  constexpr uint64_t max_cells=128'000'000;
  detail::validate_color_projection_bound_input(sampling_mesh,colors,*projection.vertex_rgba,max_cells);
  auto key=bound_key(sampling_mesh,mesh,preparation,appearance,projection,descriptor,colors,initial,max_cells);
  auto memo=key?detail::find_color_bound(*key):std::nullopt;
  if(memo&&cancelled&&cancelled())throw std::runtime_error("Color projection validation cancelled");
  // A callback may have changed a legally mutable input. Never use/store a
  // result under the old content/scalar identity in that case.
  if(memo){auto current=bound_key(sampling_mesh,mesh,preparation,appearance,projection,descriptor,colors,initial,max_cells);
    if(!current||!detail::same_color_bound_key(*key,*current)){memo.reset();key=std::move(current);}}
  if(!projection.vertex_rgba)throw std::invalid_argument("Color projection lost its current image during validation");
  auto bound=memo?*memo:detail::color_projection_bound(sampling_mesh,colors,*projection.vertex_rgba,max_cells,cancelled);
  if(!memo&&key){auto current=bound_key(sampling_mesh,mesh,preparation,appearance,projection,descriptor,colors,initial,max_cells);
    if(current&&detail::same_color_bound_key(*key,*current))detail::remember_color_bound(std::move(*key),bound);}
  if(bound.tested_cells!=projection.quality_cells)throw std::invalid_argument("Current color projection quality work differs from its recorded authority");
  for(size_t c=0;c<4;++c)if(bound.maximum_error[c]>projection.maximum_error[c])
    throw std::invalid_argument("Current color projection pixels exceed their recorded source-field error bound");
}
}
nlohmann::json encode_color_projection(const PaintColorProjection &p,PaintImageArchive &images) {
  if(!p.vertex_rgba)throw std::invalid_argument("Missing color projection image");
  p.validate(p.vertex_rgba->width);
  return {{"version",1},{"image",images.add(p.vertex_rgba)},{"error_bound",p.maximum_error},{"quality_cells",p.quality_cells}};
}
std::shared_ptr<const PaintColorProjection> decode_color_projection(const nlohmann::json &j,const PaintImageArchive &images) {
  if(!j.is_object() || j.size()!=4 || integer(j.at("version"),1)!=1)throw std::runtime_error("Unsupported color projection record");
  auto output=std::make_shared<PaintColorProjection>();output->vertex_rgba=images.at(uint32_t(integer(j.at("image"),UINT32_MAX)));
  output->maximum_error=j.at("error_bound").get<std::array<float,4>>();output->quality_cells=integer(j.at("quality_cells"),1'000'000'000);
  output->validate(output->vertex_rgba->width);return output;
}
nlohmann::json ColorPreparationReceipt::encode() const {
  validate();return {{"version",version},{"resolution",resolution},{"source_field",int(source_field)},
    {"quality_material",{{"color",quality_material.color},{"emission",quality_material.emission},{"alpha",quality_material.alpha},{"alpha_cutoff",quality_material.alpha_cutoff},{"double_sided",quality_material.double_sided}}},
    {"projection_error_bound",projection_error_bound},{"observed_base_error",observed_base_error},{"accepted_projection_error",accepted_projection_error},
    {"projected_texels",projected_texels},{"tested_quality_cells",tested_quality_cells},{"probe_samples",probe_samples},{"observed_mask_disagreements",observed_mask_disagreements}};
}
ColorPreparationReceipt ColorPreparationReceipt::decode(const nlohmann::json &j) {
  if(!j.is_object() || j.size()!=11 || !integer(j.at("version"),2))throw std::runtime_error("Unsupported color preparation quality receipt");
  ColorPreparationReceipt out;out.version=uint32_t(integer(j.at("version"),2));out.resolution=uint32_t(integer(j.at("resolution"),2048));out.source_field=SourceColorField(integer(j.at("source_field"),2));
  const auto &m=j.at("quality_material");if(!m.is_object() || m.size()!=5)throw std::runtime_error("Invalid color preparation material reference");
  out.quality_material={m.at("color").get<std::array<float,4>>(),m.at("emission").get<std::array<float,3>>(),int(integer(m.at("alpha"),3)),m.at("alpha_cutoff").get<float>(),m.at("double_sided").get<bool>()};
  out.projection_error_bound=j.at("projection_error_bound").get<std::array<float,4>>();out.observed_base_error=j.at("observed_base_error").get<std::array<float,4>>();
  out.accepted_projection_error=j.at("accepted_projection_error").get<float>();
  out.projected_texels=integer(j.at("projected_texels"),4'194'304);out.tested_quality_cells=integer(j.at("tested_quality_cells"),1'000'000'000);
  out.probe_samples=integer(j.at("probe_samples"),64'000'000);out.observed_mask_disagreements=integer(j.at("observed_mask_disagreements"),64'000'000);
  out.validate();return out;
}
void ColorPreparationReceipt::validate() const {
  quality_material.validate();
  if(size_t(source_field)>2 || version!=(source_field==SourceColorField::PolygonCorners?2u:1u) || resolution<64 || resolution>2048 || (resolution&(resolution-1)) ||
     !projected_texels || projected_texels>size_t(resolution)*resolution ||
     !tested_quality_cells || tested_quality_cells>1'000'000'000 ||
     !probe_samples || probe_samples>64'000'000 || observed_mask_disagreements>probe_samples)
    throw std::invalid_argument("Invalid color preparation quality receipt");
  if(!std::isfinite(accepted_projection_error)||accepted_projection_error<0||accepted_projection_error>1)
    throw std::invalid_argument("Invalid accepted color projection tolerance");
  for(float value:projection_error_bound)if(value>accepted_projection_error)
    throw std::invalid_argument("Initial color projection exceeds the accepted tolerance");
  for(const auto &values:{projection_error_bound,observed_base_error})for(float value:values)
    if(!std::isfinite(value) || value<0 || value>1)throw std::invalid_argument("Invalid color preparation error bound");
}
void SourceColorPreparation::validate(bool deep) const {
  // Runtime DTOs must be bounded too; archive ordering alone cannot protect a
  // caller-created cyclic predecessor chain. This guard never allocates.
  static thread_local std::array<const SourceColorPreparation*,64> ancestors{};
  static thread_local size_t depth=0;
  if(depth==ancestors.size()||std::find(ancestors.begin(),ancestors.begin()+depth,this)!=ancestors.begin()+depth)
    throw std::invalid_argument("Colour preparation history is cyclic or exceeds its depth budget");
  ancestors[depth++]=this;struct Pop {size_t &depth;~Pop(){--depth;}} pop{depth};
  if(paint_sampling){
    const auto &source=paint_sampling->before_material();
    if(!source||!source->color_preparation)throw std::invalid_argument("Retained paint sampling has no prepared source owner");
    const auto &prior=*source->color_preparation;
    if(projection!=prior.projection||initial_projection!=prior.initial_projection||original_paint!=prior.original_paint||
       original_material!=prior.original_material||original_appearance!=prior.original_appearance||original_deformation!=prior.original_deformation||
       polygon_colors!=prior.polygon_colors||uv_reprojection!=prior.uv_reprojection||receipt.encode()!=prior.receipt.encode())
      throw std::invalid_argument("Retained paint sampling cannot replace initial field or projection authority");
    paint_sampling->validate(deep);
  }
  if(uv_reprojection){
    const auto &u=*uv_reprojection;
    if(!u.before_material||!u.before_material->color_preparation)
      throw std::invalid_argument("Current colour UV recipe lacks its own prepared predecessor");
    const auto &prior=*u.before_material->color_preparation;
    if(projection!=u.projection||initial_projection!=prior.initial_projection||
       original_paint!=prior.original_paint||original_material!=prior.original_material||
       original_appearance!=prior.original_appearance||original_deformation!=prior.original_deformation||
       polygon_colors!=prior.polygon_colors||receipt.encode()!=prior.receipt.encode())
      throw std::invalid_argument("Current colour UV recipe cannot replace its immutable initial authority");
    u.validate(deep);
  }
  receipt.validate();
  if(bool(polygon_colors)!=(receipt.source_field==SourceColorField::PolygonCorners) ||
     (polygon_colors&&(!polygon_colors->topology||!polygon_colors->field||polygon_colors->topology->faces().empty())))
    throw std::invalid_argument("Current corner color receipt requires its complete geometry-owned field");
  if(!projection || !initial_projection || !original_paint || original_paint->in_stroke() || original_paint->color_projection())
    throw std::invalid_argument("Color preparation requires an immutable unprepared original");
  if(original_paint->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored)
    throw std::invalid_argument("Color preparation original paint requires explicit strict conversion; polygon authority cannot be inferred");
  projection->validate(receipt.resolution);
  for(float value:projection->maximum_error)if(value>receipt.accepted_projection_error)
    throw std::invalid_argument("Current color projection exceeds the accepted tolerance");
  initial_projection->validate(receipt.resolution);
  if(initial_projection->maximum_error!=receipt.projection_error_bound || initial_projection->quality_cells!=receipt.tested_quality_cells)
    throw std::invalid_argument("Initial projection and its immutable quality receipt differ");
  if(original_paint->resolution()!=receipt.resolution)
    throw std::invalid_argument("Color preparation original resolution differs");
  if(original_material && original_material->color_preparation)
    throw std::invalid_argument("Color preparation origins cannot recursively contain preparations");
  // Cheap publications must enforce retained policy authority too. A current
  // object's unrelated PolygonAuthority cannot authenticate this before-binding.
  if(original_material)original_material->validate(deep);
  detail::source_paint_pairing(original_paint.get(),original_material.get(),original_appearance.get(),deep);
  if(original_appearance && original_appearance->source_vertices.size()!=original_paint->mesh().positions.size())
    throw std::invalid_argument("Color preparation original appearance mapping differs");
  if(deep) {
    if(original_deformation)original_deformation->validate(original_paint->mesh());
    const auto encoded=receipt.encode();
    {
      std::lock_guard lock(proof_mutex);
      if(std::any_of(initial_proofs.begin(),initial_proofs.end(),[&](const auto &p){return same_owner(p.paint,original_paint)&&same_owner(p.appearance,original_appearance)&&same_owner(p.projection,initial_projection)&&same_owner(p.polygon_colors,polygon_colors)&&p.receipt==encoded;}))return;
    }
    Material material;material.color=receipt.quality_material.color;material.emissive=receipt.quality_material.emission;
    material.alpha=Material::AlphaMode(receipt.quality_material.alpha);material.alpha_cutoff=receipt.quality_material.alpha_cutoff;material.double_sided=receipt.quality_material.double_sided;
    ColorPreparationOptions options;options.max_projection_error=receipt.accepted_projection_error;options.max_working_bytes=512*1024*1024;
    auto colors=polygon_colors?detail::polygon_target_colors(original_paint->mesh(),*polygon_colors):vertex_field(original_paint->mesh().positions.size(),original_appearance.get());
    verify_field(original_paint->mesh(),*this,original_appearance.get(),*initial_projection,{},true);
    auto recomputed=SourceColorPreparationAccess::prepare(*original_paint,colors,material,options,receipt.source_field,initial_projection);
    auto proof=recomputed.receipt.encode(),claimed=encoded;
    proof.erase("observed_base_error");claimed.erase("observed_base_error");
    bool exact=proof==claimed;for(size_t c=0;c<4;++c)exact&=std::abs(recomputed.receipt.observed_base_error[c]-receipt.observed_base_error[c])<=2e-6f;
    if(!exact)
      throw std::invalid_argument("Initial color preparation pixels or quality receipt differ from the retained preconversion source");
    std::lock_guard lock(proof_mutex);if(initial_proofs.size()==32)initial_proofs.pop_front();initial_proofs.push_back({original_paint,original_appearance,initial_projection,polygon_colors,encoded});
  }
}
const std::shared_ptr<const PolygonColorSurface> &SourceColorPreparation::current_descriptor() const noexcept {
  return paint_sampling?paint_sampling->current_descriptor():uv_reprojection?uv_reprojection->polygon_colors:polygon_colors;
}
const std::shared_ptr<const PolygonColorSurface> &SourceColorPreparation::sampling_descriptor() const noexcept {
  return paint_sampling?paint_sampling->sampling_descriptor():uv_reprojection?uv_reprojection->polygon_colors:polygon_colors;
}
void SourceColorPreparation::validate_current(const Mesh &mesh,const CreatorAppearance *appearance,
                                             const std::function<bool()> &cancelled) const {
  validate(false);if(paint_sampling){if(appearance)throw std::invalid_argument("Current corner retained paint conflicts with raw appearance");paint_sampling->validate_current(mesh,cancelled);}else if(uv_reprojection)uv_reprojection->validate_current(mesh,appearance,cancelled);verify_field(mesh,*this,appearance,*projection,cancelled);
}
std::vector<std::array<float,4>> SourceColorPreparation::vertex_field(size_t count,const CreatorAppearance *appearance,
                                                   const std::function<bool()> &cancelled) const {
  if(cancelled&&cancelled())throw std::runtime_error("Color field validation cancelled");
  if(receipt.source_field==SourceColorField::PolygonCorners){
    const auto &descriptor=current_descriptor();
    if(!descriptor||!descriptor->topology||!descriptor->field)throw std::invalid_argument("Current corner projection lost its explicit geometry authority");
    if(!detail::source_vertex_colors(appearance,count,cancelled).empty())throw std::invalid_argument("Current corner projection conflicts with raw creator color authority");
    PolygonLimits limits;limits.cancelled=cancelled;auto geometry=descriptor->topology->triangulate(limits);if(count!=geometry.mesh.positions.size())throw std::invalid_argument("Current corner projection lost its actual ordered vertex domain");
    return detail::polygon_target_colors(geometry.mesh,*descriptor,cancelled);
  }
  auto colors=detail::source_vertex_colors(appearance,count,cancelled);
  if(receipt.source_field==SourceColorField::Identity){if(!colors.empty())throw std::invalid_argument("Identity color field cannot replace retained authored color accessors");colors.assign(count,{1,1,1,1});}
  else if(receipt.source_field!=SourceColorField::OriginalAccessor||colors.empty())throw std::invalid_argument("Prepared color field lost its explicit original accessor authority");
  return colors;
}
ColorPreparationResult prepare_source_colors(const ColorPreparationInput &input,
                                             const ColorPreparationOptions &options) {
  if(options.cancelled && options.cancelled())throw std::runtime_error("Color preparation cancelled");
  if(!input.paint || input.paint->in_stroke())throw std::invalid_argument("Finish the color target paint stroke before preparation");
  input.material.validate();
  if(!options.max_quality_cells || options.max_quality_cells>1'000'000'000 ||
     !options.max_probe_samples || options.max_probe_samples>64'000'000 ||
     !options.max_working_bytes || options.max_working_bytes>512*1024*1024 ||
     !options.max_layer_bytes || options.max_layer_bytes>128*1024*1024 ||
     !std::isfinite(options.max_projection_error) || options.max_projection_error<0 || options.max_projection_error>1)
    throw std::invalid_argument("Invalid color preparation work or quality limits");
  if(input.material_bindings)input.material_bindings->validate();
  if(input.paint->color_projection()) {
    if(input.paint->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored)
      throw std::invalid_argument("Prepared polygon paint requires explicit strict conversion with its owning authority");
    if(!input.material_bindings || !input.material_bindings->color_preparation ||
       input.material_bindings->color_preparation->projection->vertex_rgba!=input.paint->color_projection()->vertex_rgba)
      throw std::invalid_argument("Current projected colors have no matching preparation provenance");
    const auto &recipe=*input.material_bindings->color_preparation;
    if(bool(input.polygon_colors)!=bool(recipe.polygon_colors) ||
       (input.polygon_colors&&input.polygon_colors->field!=recipe.polygon_colors->field))
      throw std::invalid_argument("Current corner field differs from its accepted color projection");
    if(input.polygon_colors)detail::polygon_target_colors(input.paint->mesh(),*input.polygon_colors,options.cancelled);
    return {input.paint->snapshot(),input.material_bindings,false};
  }
  if(input.material_bindings && input.material_bindings->color_preparation)
    throw std::invalid_argument("Color preparation holder differs from current paint");
  auto colors=detail::source_vertex_colors(input.appearance.get(),input.paint->mesh().positions.size(),options.cancelled);
  const auto field=input.polygon_colors?SourceColorField::PolygonCorners:colors.empty()?SourceColorField::Identity:SourceColorField::OriginalAccessor;
  if(input.polygon_colors){
    if(!colors.empty())throw std::invalid_argument("Current corner projection cannot replace raw creator colors");
    colors=detail::polygon_target_colors(input.paint->mesh(),*input.polygon_colors,options.cancelled);
  }
  if(!options.resolve_identity && (colors.empty() || std::all_of(colors.begin(),colors.end(),[](auto c){return c==std::array<float,4>{1,1,1,1};})))
    return {input.paint->snapshot(),input.material_bindings,false};
  if(input.paint->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored)
    throw std::invalid_argument("Color preparation requires explicit strict conversion of polygon-authored paint; original paint was retained");
  if(colors.empty())colors.assign(input.paint->mesh().positions.size(),{1,1,1,1});
  if(const auto &maps=input.paint->source_maps())for(const auto *view:{&maps->color,&maps->metal_rough,&maps->normal,&maps->emission,&maps->occlusion})
    if(view->texcoord!=0)throw std::invalid_argument("Resolve original PBR UV1 through source-aware sampling before color preparation");
  if(input.deformation)input.deformation->validate(input.paint->mesh());
  if(input.material_bindings && input.material_bindings->normal_frames)
    detail::normal_frame_current_authority(*input.material_bindings,input.paint->mesh(),input.deformation.get(),options.cancelled);
  auto prepared=SourceColorPreparationAccess::prepare(*input.paint,colors,input.material,options,field);
  auto receipt=std::make_shared<SourceColorPreparation>();
  receipt->projection=receipt->initial_projection=prepared.paint->color_projection();receipt->receipt=prepared.receipt;
  receipt->original_paint=input.paint->snapshot();receipt->original_appearance=input.appearance;
  receipt->original_deformation=input.deformation;receipt->original_material=input.material_bindings;
  if(input.polygon_colors)receipt->polygon_colors=std::make_shared<const PolygonColorSurface>(*input.polygon_colors);
  receipt->validate();
  auto binding=input.material_bindings?std::make_shared<SourceMaterialBindings>(*input.material_bindings):std::make_shared<SourceMaterialBindings>();
  if(!input.material_bindings) {
    auto typed=retained_mtoon(input.appearance.get());binding->has_mtoon=bool(typed);
    if(typed)binding->material=*typed;
    binding->native_reference=input.appearance?native_source(*input.appearance):SourceNativeMaterial{input.material.color,input.material.emissive,int(input.material.alpha),input.material.alpha_cutoff,input.material.double_sided};
    if(typed)for(size_t role=0;role<size_t(MToonSlot::Count);++role) {
      const auto index=typed->textures[role].index;if(index<0)continue;
      if(!input.appearance || !input.appearance->asset || !input.appearance->decoded_images)throw std::invalid_argument("Typed color preparation source is incomplete");
      const auto &texture=input.appearance->asset->textures.at(size_t(index));auto &view=binding->roles[role];
      view.image=input.appearance->decoded_images->at(size_t(texture.image));view.wrap_s=texture.wrap_s;view.wrap_t=texture.wrap_t;view.min_filter=texture.min_filter;view.mag_filter=texture.mag_filter;
    }
  }
  if(!binding->uv_origin) {
    auto origin=std::make_shared<SourceUvOrigin>();origin->paint=receipt->original_paint;origin->appearance=input.appearance;origin->deformation=input.deformation;
    if(binding->has_mtoon)origin->material=binding->material;
    origin->roles=binding->roles;
    binding->uv_origin=std::move(origin);
  }
  binding->color_preparation=std::move(receipt);binding->validate();
  if(options.cancelled && options.cancelled())throw std::runtime_error("Color preparation cancelled");
  return {std::move(prepared.paint),std::move(binding),true};
}
} // namespace dome
