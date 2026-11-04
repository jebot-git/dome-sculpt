// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-normal-owner.hpp"
#include "baked-normal-field.hpp"
#include "paint-normal-access.hpp"
#include "text-utf8.hpp"
#include <nlohmann/json.hpp>
namespace dome::detail {
namespace {
nlohmann::json material_record(const Material &m){
  return {{"name",m.name},{"color",m.color},{"metallic",m.metallic},{"roughness",m.roughness},
    {"emission",m.emissive},{"toon",m.toon},{"double_sided",m.double_sided},
    {"alpha",int(m.alpha)},{"alpha_cutoff",m.alpha_cutoff},{"unlit",m.unlit}};
}
Material material_read(const nlohmann::json &j){
  if(!j.is_object()||j.size()!=10||!j.at("name").is_string()||j.at("name").get_ref<const std::string&>().size()>256)
    throw std::invalid_argument("Invalid canonical conversion material record");
  Material m;m.name=j.at("name").get<std::string>();m.color=j.at("color").get<std::array<float,4>>();
  m.emissive=j.at("emission").get<std::array<float,3>>();m.metallic=j.at("metallic").get<float>();m.roughness=j.at("roughness").get<float>();
  m.toon=j.at("toon").get<bool>();m.double_sided=j.at("double_sided").get<bool>();m.alpha_cutoff=j.at("alpha_cutoff").get<float>();m.unlit=j.at("unlit").get<bool>();
  if(!j.at("alpha").is_number_integer()||j.at("alpha").get<int64_t>()<0||j.at("alpha").get<int64_t>()>3)
    throw std::invalid_argument("Invalid canonical conversion material alpha");
  m.alpha=Material::AlphaMode(j.at("alpha").get<int>());m.validate();return m;
}
uint64_t integer(const nlohmann::json &v,uint64_t cap){
  if((!v.is_number_unsigned()&&(!v.is_number_integer()||v.get<int64_t>()<0))||v.get<uint64_t>()>cap)
    throw std::invalid_argument("Canonical conversion integer exceeds its declared bound");
  return v.get<uint64_t>();
}
}
std::shared_ptr<const CanonicalNormalConversion> CanonicalNormalConversion::create(
    const ColorPreparationInput &input,const NormalPreparationOptions &options){
  if(!input.paint)throw std::invalid_argument("Canonical conversion needs its genuine input paint");
  if(!std::isfinite(options.max_angular_error)||options.max_angular_error<0||options.max_angular_error>.25||
      !options.max_samples||options.max_samples>64'000'000||!options.max_working_bytes||options.max_working_bytes>512*1024*1024)
    throw std::invalid_argument("Invalid canonical normal conversion owner policy");
  input.material.validate();
  if(input.material.name.size()>256||!text::valid_utf8(input.material.name)||input.material.name.find('\0')!=std::string::npos)
    throw std::invalid_argument("Invalid canonical conversion material name");
  if(input.deformation)input.deformation->validate(input.paint->mesh());
  size_t overhead=64*1024;
  auto charge=[&](size_t n,size_t unit){
    if(overhead>options.max_working_bytes||n>(options.max_working_bytes-overhead)/unit)
      throw std::length_error("Canonical conversion snapshot exceeds its working memory budget");
    overhead+=n*unit;
  };
  if(input.deformation){charge(input.deformation->resource_entries(),sizeof(Vec3));
    if(input.deformation->skeleton)for(const auto &bone:input.deformation->skeleton->bones){charge(1,sizeof(Bone)+64);charge(bone.name.size(),1);}
  }
  for(const auto &layer:input.paint->layers()){
    charge(1,sizeof(PaintLayer));charge(layer.name.size(),1);
    charge(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size(),sizeof(std::shared_ptr<const PaintTile>));
  }
  if(overhead>=options.max_working_bytes)throw std::length_error("Canonical conversion snapshot exceeds its working memory budget");
  auto working=options;working.max_working_bytes-=overhead;
  auto value=std::shared_ptr<CanonicalNormalConversion>(new CanonicalNormalConversion);
  value->material_=input.material;
  // The live editable PaintedMesh object is not a historical owner. Snapshot
  // its COW state, then keep that immutable object through all descendants.
  value->paint_=input.paint->snapshot();
  value->deformation_=input.deformation?std::make_shared<const DeformationState>(*input.deformation):nullptr;
  value->binding_=input.material_bindings;
  ColorPreparationInput captured{value->paint_,input.appearance,value->binding_,value->deformation_,value->material_,{}};
  auto candidate=convert_native_canonical_normals(captured,working);
  value->frames_=std::move(candidate.target_frames);value->projection_=std::move(candidate.projection);
  value->accepted_=options.max_angular_error;value->observed_=candidate.maximum_center_angular_error;
  value->samples_=candidate.sampled_centers;value->raster_=candidate.raster_samples;
  value->max_samples_=options.max_samples;value->max_bytes_=options.max_working_bytes;
  value->owner_=value;value->validate();return value;
}
NormalPreparationReceipt CanonicalNormalConversion::receipt() const {
  NormalPreparationReceipt r;r.resolution=paint_->resolution();
  r.authored_native_normals=!binding_||!binding_->normal_frames;
  r.authored_native_tangents=!binding_||!binding_->normal_frames||binding_->normal_frames->pbr->tangents.empty();
  r.accepted_angular_error=accepted_;r.observed_initial_error=observed_;r.sampled_texels=samples_;r.validate();return r;
}
void CanonicalNormalConversion::validate(bool deep) const {
  if(owner_.expired()||!paint_||paint_->in_stroke()||paint_->normal_projection()||!paint_->baked()||
      paint_->baked()->normal_interpretation!=BakedNormalInterpretation::CanonicalMikk||paint_->baked()->maps.normal.rgba.empty()||
      !frames_||!projection_||projection_->version!=1||!frames_->pbr||frames_->canonical||
      !samples_||raster_<samples_||raster_>max_samples_||!max_samples_||max_samples_>64'000'000||
      !max_bytes_||max_bytes_>512*1024*1024)
    throw std::invalid_argument("Incomplete actual canonical conversion lease");
  receipt();material_.validate();projection_->validate(paint_->resolution(),deep);frames_->validate(deep);
  if(binding_)binding_->validate(deep);
  if(deep&&deformation_)deformation_->validate(paint_->mesh());
}
nlohmann::json CanonicalNormalConversion::encode() const {
  validate();return {{"version",1},{"model","native-canonical-raw-to-pbr-columns-v1"},
    {"accepted_angle",accepted_},{"maximum_samples",max_samples_},{"maximum_bytes",max_bytes_},
    {"sampled_centers",samples_},{"raster_samples",raster_},{"observed_center_error",observed_},
    {"material",material_record(material_)}};
}
std::shared_ptr<const CanonicalNormalConversion> CanonicalNormalConversion::restore(
    const nlohmann::json &j,const ColorPreparationInput &input,
    std::shared_ptr<const NormalFrameSet> frames,std::shared_ptr<const PaintNormalProjection> projection){
  if(!j.is_object()||j.size()!=9||j.at("version")!=1||j.at("model")!="native-canonical-raw-to-pbr-columns-v1"||
      !j.at("accepted_angle").is_number()||!j.at("observed_center_error").is_number())
    throw std::invalid_argument("Unsupported canonical conversion replay record");
  NormalPreparationOptions options;options.max_angular_error=j.at("accepted_angle").get<double>();
  options.max_samples=integer(j.at("maximum_samples"),64'000'000);options.max_working_bytes=size_t(integer(j.at("maximum_bytes"),512*1024*1024));
  integer(j.at("sampled_centers"),4'194'304);integer(j.at("raster_samples"),64'000'000);
  ColorPreparationInput source=input;source.material=material_read(j.at("material"));
  if(!frames||!projection||!projection->pbr||!projection->mtoon||!input.paint)
    throw std::invalid_argument("Canonical conversion replay lacks complete serialized target data");
  frames->validate();projection->validate(input.paint->resolution());
  const auto replay=create(source,options);
  NormalFrameArchive a,b;a.add(replay->frames_);b.add(frames);
  if(replay->encode()!=j||a.encode()!=b.encode()||!projection||projection->version!=1||projection->sampling_domain||
      projection->pbr->rgba!=replay->projection_->pbr->rgba||projection->mtoon->rgba!=replay->projection_->mtoon->rgba||
      projection->pbr->width!=replay->projection_->pbr->width||projection->mtoon->width!=replay->projection_->mtoon->width||
      projection->pbr->height!=replay->projection_->pbr->height||projection->mtoon->height!=replay->projection_->mtoon->height)
    throw std::invalid_argument("Canonical conversion record differs from its actual retained-source replay");
  // Replay proves the serialized DTO; restore the archive's exact shared
  // owners so all normal/key/UV descendants retain one genuine authority.
  auto value=std::shared_ptr<CanonicalNormalConversion>(new CanonicalNormalConversion);
  value->paint_=input.paint;value->binding_=input.material_bindings;value->deformation_=input.deformation;
  value->material_=source.material;value->frames_=std::move(frames);value->projection_=std::move(projection);
  value->accepted_=replay->accepted_;value->observed_=replay->observed_;value->samples_=replay->samples_;
  value->raster_=replay->raster_;value->max_samples_=replay->max_samples_;value->max_bytes_=replay->max_bytes_;
  value->owner_=value;value->validate();return value;
}
} // namespace dome::detail
namespace dome {
NormalPreparationResult convert_canonical_normals(const ColorPreparationInput &input,
    const NormalPreparationOptions &options){
  if(input.polygon_colors)throw std::invalid_argument("Canonical corner colour conversion requires its owning adapter");
  const auto owner=detail::CanonicalNormalConversion::create(input,options);
  auto binding=input.material_bindings?std::make_shared<SourceMaterialBindings>(*input.material_bindings):std::make_shared<SourceMaterialBindings>();
  binding->has_mtoon=false;binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;
  binding->normal_frames=owner->frames();binding->normal_preparation.reset();binding->canonical_attributes=owner;
  if(!binding->uv_origin){auto origin=std::make_shared<SourceUvOrigin>();origin->paint=owner->before_paint();origin->deformation=owner->before_deformation();binding->uv_origin=std::move(origin);}
  if(!input.material_bindings){binding->native_reference.color=input.material.color;binding->native_reference.emission=input.material.emissive;
    binding->native_reference.alpha=int(input.material.alpha);binding->native_reference.alpha_cutoff=input.material.alpha_cutoff;binding->native_reference.double_sided=input.material.double_sided;}
  detail::BakedNormalFieldOptions field_options;field_options.max_working_bytes=options.max_working_bytes;field_options.cancelled=options.cancelled;
  auto paint=SourceNormalPreparationAccess::adopt_baked_field(*owner->before_paint(),detail::BakedNormalField::from_canonical(owner,field_options),options.cancelled);
  binding->validate();if(options.cancelled&&options.cancelled())throw std::runtime_error("Canonical normal conversion cancelled");
  return {std::move(paint),std::move(binding),true};
}
} // namespace dome
