// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/material-bindings.hpp"
#include "dome/creator-archive.hpp"
#include "dome/paint.hpp"
#include <algorithm>
namespace dome {
namespace {
constexpr size_t role_count=size_t(MToonSlot::Count);
void roles_valid(const std::optional<MToonMaterial> &material,
                 const std::array<PaintSourceTexture,role_count> &roles) {
  if(material) material->validate();
  for(size_t role=0;role<role_count;++role) {
    const auto &view=roles[role];view.validate();
    if((material && material->textures[role].index>=0)!=bool(view.image))
      throw std::invalid_argument("Resolved material role table is incomplete");
    if(view.texcoord!=0 || view.uv_transform!=std::array<float,6>{1,0,0,1,0,0})
      throw std::invalid_argument("Resolved role UV transforms belong to the typed material");
  }
}
nlohmann::json encode_roles(const std::array<PaintSourceTexture,role_count> &roles,PaintImageArchive &images) {
  auto output=nlohmann::json::array();
  for(const auto &view:roles) output.push_back({
    {"image",view.image?nlohmann::json(images.add(view.image)):nlohmann::json(nullptr)},
    {"wrap",{view.wrap_s,view.wrap_t}}, {"filter",{view.min_filter,view.mag_filter}}});
  return output;
}
std::array<PaintSourceTexture,role_count> decode_roles(const nlohmann::json &input,const PaintImageArchive &images) {
  if(!input.is_array() || input.size()!=role_count) throw std::runtime_error("Invalid resolved material role count");
  std::array<PaintSourceTexture,role_count> output;
  for(size_t role=0;role<role_count;++role) {
    auto &view=output[role];const auto &value=input[role];
    if(!value.at("image").is_null()) {
      const auto &index=value.at("image");
      if((!index.is_number_unsigned() && (!index.is_number_integer() || index.get<int64_t>()<0)) ||
         index.get<uint64_t>()>UINT32_MAX) throw std::runtime_error("Invalid material image identity");
      view.image=images.at(index.get<uint32_t>());
    }
    const auto &wrap=value.at("wrap"),&filter=value.at("filter");
    if(!wrap.is_array() || wrap.size()!=2 || !filter.is_array() || filter.size()!=2)
      throw std::runtime_error("Invalid resolved material sampler");
    view.wrap_s=wrap[0].get<int>();view.wrap_t=wrap[1].get<int>();
    view.min_filter=filter[0].get<int>();view.mag_filter=filter[1].get<int>();view.validate();
  }
  return output;
}
size_t metadata_bytes(const std::optional<MToonMaterial> &material) {
  if(!material)return 0;
  auto bytes=material->encode().dump().size();
  if(bytes>2*1024*1024) throw std::length_error("Resolved material metadata exceeds budget");
  return bytes;
}
size_t index(const nlohmann::json &value,size_t size) {
  if((!value.is_number_unsigned() && (!value.is_number_integer() || value.get<int64_t>()<0)) ||
     value.get<uint64_t>()>=size) throw std::runtime_error("Invalid resolved material archive identity");
  return size_t(value.get<uint64_t>());
}
}
void SourceNativeMaterial::validate() const {
  auto unit=[](float value){return std::isfinite(value) && value>=0 && value<=1;};
  if(!std::all_of(color.begin(),color.end(),unit) || !std::all_of(emission.begin(),emission.end(),unit) ||
     alpha<0 || alpha>3 || !unit(alpha_cutoff))
    throw std::invalid_argument("Invalid native factor baseline for resolved material");
}
void SourceMaterialBindings::validate() const {
  if(normal_frame!=SourceMaterialNormalFrame::UvDerivative || size_t(animation_policy)>1 ||
     !std::isfinite(freeze_seconds) || freeze_seconds<0 || freeze_seconds>1e9 ||
     (animation_frozen && animation_policy!=SourceUvAnimationPolicy::FreezeAtTime))
    throw std::invalid_argument("Invalid derived material frame or animation provenance");
  native_reference.validate();
  roles_valid(has_mtoon?std::optional<MToonMaterial>(material):std::nullopt,roles);
  if(animation_frozen && has_mtoon &&
     (material.parameters.scroll!=std::array<float,2>{} || material.parameters.rotation_speed!=0))
    throw std::invalid_argument("An explicitly frozen material cannot retain active UV animation");
  if(uv_origin) {
    if(!uv_origin->paint || uv_origin->paint->in_stroke())
      throw std::invalid_argument("Derived UV material has no finished immutable source");
    roles_valid(uv_origin->material,uv_origin->roles);
    if(uv_origin->appearance && uv_origin->appearance->source_vertices.size()!=uv_origin->paint->mesh().positions.size())
      throw std::invalid_argument("Original UV material source remap differs from its retained paint mesh");
  }
}
uint32_t SourceMaterialArchive::add(std::shared_ptr<const SourceMaterialBindings> value) {
  if(!value) throw std::invalid_argument("Missing resolved material holder");
  auto found=std::find(bindings_.begin(),bindings_.end(),value);
  if(found!=bindings_.end())return uint32_t(found-bindings_.begin());
  if(bindings_.size()>=256)throw std::length_error("Resolved material archive count exceeds budget");
  value->validate();
  if(value->uv_origin && std::find(origins_.begin(),origins_.end(),value->uv_origin)==origins_.end()) {
    if(origins_.size()>=256)throw std::length_error("Original UV source archive count exceeds budget");
    origins_.push_back(value->uv_origin);
  }
  bindings_.push_back(std::move(value));return uint32_t(bindings_.size()-1);
}
const std::shared_ptr<const SourceMaterialBindings> &SourceMaterialArchive::at(uint32_t id) const {
  return bindings_.at(id);
}
nlohmann::json SourceMaterialArchive::encode(PaintImageArchive &images,CreatorAssetArchive &models) const {
  nlohmann::json output={{"version",1},{"paints",nlohmann::json::array()},{"origins",nlohmann::json::array()},{"bindings",nlohmann::json::array()}};
  std::vector<std::shared_ptr<const PaintedMesh>> paints;
  size_t metadata=0;
  for(const auto &origin:origins_) {
    metadata+=metadata_bytes(origin->material);
    if(metadata>16*1024*1024)throw std::length_error("Scene resolved material metadata exceeds budget");
    auto paint=std::find(paints.begin(),paints.end(),origin->paint);
    size_t paint_id=size_t(paint-paints.begin());
    if(paint==paints.end()) {paints.push_back(origin->paint);output["paints"].push_back(origin->paint->encode(&images));}
    output["origins"].push_back({{"paint",paint_id},
      {"appearance",origin->appearance?encode_creator_appearance(*origin->appearance,models,images):nlohmann::json(nullptr)},
      {"material",origin->material?origin->material->encode():nlohmann::json(nullptr)},
      {"roles",encode_roles(origin->roles,images)}});
  }
  for(const auto &binding:bindings_) {
    binding->validate();metadata+=metadata_bytes(binding->has_mtoon?std::optional<MToonMaterial>(binding->material):std::nullopt);
    if(metadata>16*1024*1024)throw std::length_error("Scene resolved material metadata exceeds budget");
    auto origin=std::find(origins_.begin(),origins_.end(),binding->uv_origin);
    output["bindings"].push_back({{"material",binding->has_mtoon?binding->material.encode():nlohmann::json(nullptr)},
      {"roles",encode_roles(binding->roles,images)}, {"normal_frame","uv-derivative"},
      {"animation_policy",int(binding->animation_policy)}, {"freeze_seconds",binding->freeze_seconds},
      {"animation_frozen",binding->animation_frozen},
      {"native_reference",{{"color",binding->native_reference.color},{"emission",binding->native_reference.emission},
        {"alpha",binding->native_reference.alpha},{"alpha_cutoff",binding->native_reference.alpha_cutoff},
        {"double_sided",binding->native_reference.double_sided}}},
      {"origin",binding->uv_origin?nlohmann::json(size_t(origin-origins_.begin())):nlohmann::json(nullptr)}});
  }
  return output;
}
SourceMaterialArchive SourceMaterialArchive::decode(const nlohmann::json &input,const PaintImageArchive &images,
                                                   const CreatorAssetArchive &models) {
  if(input.at("version")!=1 || !input.at("paints").is_array() || input.at("paints").size()>256 || !input.at("origins").is_array() || input.at("origins").size()>256 ||
     !input.at("bindings").is_array() || input.at("bindings").size()>256)
    throw std::runtime_error("Invalid resolved material archive");
  // Preflight all implicit SurfaceMap allocations before constructing any.
  size_t texels=0,vertices=0,tile_bytes=0,metadata=0;
  for(const auto &paint:input.at("paints")) {
    const auto &resolution=paint.at("resolution");
    if(!resolution.is_number_unsigned() && (!resolution.is_number_integer() || resolution.get<int64_t>()<64))
      throw std::runtime_error("Invalid original UV paint resolution");
    auto r=resolution.get<uint64_t>();
    if(r<64 || r>2048 || (r&(r-1)))throw std::runtime_error("Invalid original UV paint resolution");
    texels+=size_t(r)*r;
    const auto &positions=paint.at("mesh").at("positions");
    if(!positions.is_binary())throw std::runtime_error("Invalid original UV geometry");
    vertices+=positions.get_binary().size()/12;
    const auto &layers=paint.at("layers");
    if(!layers.is_array() || layers.empty() || layers.size()>16)throw std::runtime_error("Invalid original UV layers");
    for(const auto &layer:layers) {
      if(!layer.at("tiles").is_array())throw std::runtime_error("Invalid original UV tiles");
      tile_bytes+=layer.at("tiles").size()*sizeof(PaintTile);
    }
    if(paint.contains("baked"))for(const auto *key:{"color","orm","emission","normal","coverage"}) {
      const auto &bytes=paint.at("baked").at(key);
      if(!bytes.is_binary())throw std::runtime_error("Invalid original UV baked buffer");
      tile_bytes+=bytes.get_binary().size();
    }
    if(texels>16*1024*1024 || vertices>8'000'000 || tile_bytes>256*1024*1024)
      throw std::length_error("Original UV source archive exceeds scene budgets");
  }
  SourceMaterialArchive output;
  std::vector<std::shared_ptr<const PaintedMesh>> paints;
  for(const auto &paint:input.at("paints"))paints.push_back(PaintedMesh::decode(paint,&images));
  for(const auto &value:input.at("origins")) {
    auto origin=std::make_shared<SourceUvOrigin>();
    origin->paint=paints.at(index(value.at("paint"),paints.size()));
    if(!value.at("appearance").is_null())origin->appearance=decode_creator_appearance(value.at("appearance"),models);
    if(!value.at("material").is_null())origin->material=MToonMaterial::decode(value.at("material"));
    metadata+=metadata_bytes(origin->material);
    if(metadata>16*1024*1024)throw std::length_error("Scene resolved material metadata exceeds budget");
    origin->roles=decode_roles(value.at("roles"),images);output.origins_.push_back(std::move(origin));
  }
  for(const auto &value:input.at("bindings")) {
    if(value.at("normal_frame")!="uv-derivative")throw std::runtime_error("Unsupported derived material normal frame");
    auto binding=std::make_shared<SourceMaterialBindings>();
    binding->has_mtoon=!value.at("material").is_null();
    if(binding->has_mtoon)binding->material=MToonMaterial::decode(value.at("material"));
    metadata+=metadata_bytes(binding->has_mtoon?std::optional<MToonMaterial>(binding->material):std::nullopt);
    if(metadata>16*1024*1024)throw std::length_error("Scene resolved material metadata exceeds budget");
    binding->roles=decode_roles(value.at("roles"),images);
    int policy=value.at("animation_policy").get<int>();
    if(policy<0 || policy>1)throw std::runtime_error("Invalid derived material animation policy");
    binding->animation_policy=SourceUvAnimationPolicy(policy);
    binding->freeze_seconds=value.at("freeze_seconds").get<double>();
    binding->animation_frozen=value.at("animation_frozen").get<bool>();
    const auto &native=value.at("native_reference");
    binding->native_reference.color=native.at("color").get<std::array<float,4>>();
    binding->native_reference.emission=native.at("emission").get<std::array<float,3>>();
    binding->native_reference.alpha=native.at("alpha").get<int>();
    binding->native_reference.alpha_cutoff=native.at("alpha_cutoff").get<float>();
    binding->native_reference.double_sided=native.at("double_sided").get<bool>();
    if(!value.at("origin").is_null())binding->uv_origin=output.origins_.at(index(value.at("origin"),output.origins_.size()));
    binding->validate();output.bindings_.push_back(std::move(binding));
  }
  return output;
}
} // namespace dome
