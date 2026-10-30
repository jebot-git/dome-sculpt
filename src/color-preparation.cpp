// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/color-preparation.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/material-visibility.hpp"
#include "normal-frame-runtime.hpp"
#include "paint-color-access.hpp"
#include "source-colors.hpp"
#include "color-projection-quality.hpp"
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
  nlohmann::json receipt;
};
std::mutex proof_mutex;
std::deque<InitialProof> initial_proofs;
template<class T>bool same_owner(const std::weak_ptr<const T>&a,const std::shared_ptr<const T>&b){return a.lock()==b&&!a.owner_before(b)&&!b.owner_before(a);}
void verify_field(const Mesh &mesh,const SourceColorPreparation &preparation,const CreatorAppearance *appearance,const PaintColorProjection &projection,
                  const std::function<bool()> &cancelled) {
  if(cancelled&&cancelled())throw std::runtime_error("Color projection validation cancelled");
  auto colors=preparation.vertex_field(mesh.positions.size(),appearance,cancelled);
  if(colors.empty())throw std::invalid_argument("Prepared color projection lost its raw original color authority");
  auto bound=detail::color_projection_bound(mesh,colors,*projection.vertex_rgba,128'000'000,cancelled);
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
  validate();return {{"version",1},{"resolution",resolution},{"source_field",int(source_field)},
    {"quality_material",{{"color",quality_material.color},{"emission",quality_material.emission},{"alpha",quality_material.alpha},{"alpha_cutoff",quality_material.alpha_cutoff},{"double_sided",quality_material.double_sided}}},
    {"projection_error_bound",projection_error_bound},{"observed_base_error",observed_base_error},{"accepted_projection_error",accepted_projection_error},
    {"projected_texels",projected_texels},{"tested_quality_cells",tested_quality_cells},{"probe_samples",probe_samples},{"observed_mask_disagreements",observed_mask_disagreements}};
}
ColorPreparationReceipt ColorPreparationReceipt::decode(const nlohmann::json &j) {
  if(!j.is_object() || j.size()!=11 || integer(j.at("version"),1)!=1)throw std::runtime_error("Unsupported color preparation quality receipt");
  ColorPreparationReceipt out;out.resolution=uint32_t(integer(j.at("resolution"),2048));out.source_field=SourceColorField(integer(j.at("source_field"),1));
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
  if(size_t(source_field)>1 || version!=1 || resolution<64 || resolution>2048 || (resolution&(resolution-1)) ||
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
  receipt.validate();
  if(!projection || !initial_projection || !original_paint || original_paint->in_stroke() || original_paint->color_projection())
    throw std::invalid_argument("Color preparation requires an immutable unprepared original");
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
  if(original_appearance && original_appearance->source_vertices.size()!=original_paint->mesh().positions.size())
    throw std::invalid_argument("Color preparation original appearance mapping differs");
  if(deep) {
    if(original_deformation)original_deformation->validate(original_paint->mesh());
    if(original_material)original_material->validate();
    const auto encoded=receipt.encode();
    {
      std::lock_guard lock(proof_mutex);
      if(std::any_of(initial_proofs.begin(),initial_proofs.end(),[&](const auto &p){return same_owner(p.paint,original_paint)&&same_owner(p.appearance,original_appearance)&&same_owner(p.projection,initial_projection)&&p.receipt==encoded;}))return;
    }
    Material material;material.color=receipt.quality_material.color;material.emissive=receipt.quality_material.emission;
    material.alpha=Material::AlphaMode(receipt.quality_material.alpha);material.alpha_cutoff=receipt.quality_material.alpha_cutoff;material.double_sided=receipt.quality_material.double_sided;
    ColorPreparationOptions options;options.max_projection_error=receipt.accepted_projection_error;options.max_working_bytes=512*1024*1024;
    auto colors=vertex_field(original_paint->mesh().positions.size(),original_appearance.get());
    verify_field(original_paint->mesh(),*this,original_appearance.get(),*initial_projection,{});
    auto recomputed=SourceColorPreparationAccess::prepare(*original_paint,colors,material,options,receipt.source_field,initial_projection);
    auto proof=recomputed.receipt.encode(),claimed=encoded;
    proof.erase("observed_base_error");claimed.erase("observed_base_error");
    bool exact=proof==claimed;for(size_t c=0;c<4;++c)exact&=std::abs(recomputed.receipt.observed_base_error[c]-receipt.observed_base_error[c])<=2e-6f;
    if(!exact)
      throw std::invalid_argument("Initial color preparation pixels or quality receipt differ from the retained preconversion source");
    std::lock_guard lock(proof_mutex);if(initial_proofs.size()==32)initial_proofs.pop_front();initial_proofs.push_back({original_paint,original_appearance,initial_projection,encoded});
  }
}
void SourceColorPreparation::validate_current(const Mesh &mesh,const CreatorAppearance *appearance,
                                             const std::function<bool()> &cancelled) const {
  validate(false);verify_field(mesh,*this,appearance,*projection,cancelled);
}
std::vector<std::array<float,4>> SourceColorPreparation::vertex_field(size_t count,const CreatorAppearance *appearance,
                                                   const std::function<bool()> &cancelled) const {
  if(cancelled&&cancelled())throw std::runtime_error("Color field validation cancelled");
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
    if(!input.material_bindings || !input.material_bindings->color_preparation ||
       input.material_bindings->color_preparation->projection->vertex_rgba!=input.paint->color_projection()->vertex_rgba)
      throw std::invalid_argument("Current projected colors have no matching preparation provenance");
    return {input.paint->snapshot(),input.material_bindings,false};
  }
  if(input.material_bindings && input.material_bindings->color_preparation)
    throw std::invalid_argument("Color preparation holder differs from current paint");
  auto colors=detail::source_vertex_colors(input.appearance.get(),input.paint->mesh().positions.size(),options.cancelled);
  const auto field=colors.empty()?SourceColorField::Identity:SourceColorField::OriginalAccessor;
  if(!options.resolve_identity && (colors.empty() || std::all_of(colors.begin(),colors.end(),[](auto c){return c==std::array<float,4>{1,1,1,1};})))
    return {input.paint->snapshot(),input.material_bindings,false};
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
