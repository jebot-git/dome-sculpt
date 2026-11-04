#include "color-bound-memo.hpp"
#include "native-paint-sampling.hpp"
// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/material-bindings.hpp"
#include "normal-key-derivation.hpp"
#include "key-derivation-codec.hpp"
#include "frame-position-domain.hpp"
#include "deformation-json.hpp"
#include "dome/color-preparation.hpp"
#include "dome/color-paint-sampling.hpp"
#include "paint-sampling-access.hpp"
#include "paint-archive-shape.hpp"
#include "source-uv-archive.hpp"
#include "material-archive-version.hpp"
#include "material-key-context.hpp"
#include "emission-authority.hpp"
#include "emission-linear.hpp"
#include "dome/normal-preparation.hpp"
#include "normal-sampling-access.hpp"
#include "canonical-normal-owner.hpp"
#include "baked-normal-archive.hpp"
#include "canonical-normal-uv.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/creator-archive.hpp"
#include "dome/paint.hpp"
#include "dome/deformation.hpp"
#include "dome/humanoid-creator.hpp"
#include "polygon-color-archive.hpp"
#include <algorithm>
#include <set>
namespace dome {
namespace {
template<class T>bool owning_key_ref(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
constexpr size_t role_count=size_t(MToonSlot::Count);
thread_local std::array<const SourceMaterialBindings*,64> material_validation_path{};
thread_local size_t material_validation_depth=0;
struct MaterialValidationProof { const SourceMaterialBindings *binding=nullptr;bool deep=false,complete=false; };
thread_local std::array<MaterialValidationProof,256> material_validation_proofs{};
thread_local size_t material_validation_proof_count=0;
struct MaterialValidationScope {
  const SourceMaterialBindings *binding;
  bool proved=false;
  MaterialValidationScope(const SourceMaterialBindings *value,bool deep):binding(value){
    // This ledger lives only for one synchronous top-level validation. The
    // caller owns the immutable graph throughout; no raw-pointer proof or
    // omitted mutation check survives the call, including exceptional unwind.
    if(!material_validation_depth)material_validation_proof_count=0;
    if(material_validation_depth==material_validation_path.size()||std::find(material_validation_path.begin(),material_validation_path.begin()+material_validation_depth,binding)!=material_validation_path.begin()+material_validation_depth)
      throw std::invalid_argument("Material preparation origin graph is cyclic or exceeds its depth budget");
    const auto end=material_validation_proofs.begin()+material_validation_proof_count;
    const auto found=std::find_if(material_validation_proofs.begin(),end,[&](const auto &proof){return proof.binding==binding;});
    proved=found!=end&&found->complete&&(!deep||found->deep);
    if(found==end&&material_validation_proof_count==material_validation_proofs.size())
      throw std::length_error("Material validation exceeds its unique owning-node budget");
    if(found==end)material_validation_proofs[material_validation_proof_count++]={binding,false,false};
    material_validation_path[material_validation_depth++]=binding;
  }
  void complete(bool deep) noexcept {
    const auto end=material_validation_proofs.begin()+material_validation_proof_count;
    const auto found=std::find_if(material_validation_proofs.begin(),end,[&](const auto &proof){return proof.binding==binding;});
    // Admission reserved this entry before descending, so child completion
    // cannot exhaust space needed by an ancestor's noexcept completion.
    found->deep=found->deep||deep;found->complete=true;
  }
  ~MaterialValidationScope(){
    material_validation_path[--material_validation_depth]=nullptr;
    if(!material_validation_depth){
      std::fill_n(material_validation_proofs.begin(),material_validation_proof_count,MaterialValidationProof{});
      material_validation_proof_count=0;
    }
  }
};
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
unsigned material_shape_version(const std::vector<std::shared_ptr<const SourceMaterialBindings>> &bindings,
                                const std::vector<std::shared_ptr<const SourceUvOrigin>> &origins) {
  unsigned result=1;
  for(const auto &o:origins)if(o->deformation||!o->creator_kind.empty())result=std::max(result,2u);
  for(const auto &b:bindings){
    if(b->key_derivation)result=std::max(result,10u);
    if(b->normal_frames||(b->uv_origin&&!b->sampling_reparameterized))result=std::max(result,2u);
    if(const auto &p=b->color_preparation){
      result=std::max(result,3u);if(p->polygon_colors)result=std::max(result,6u);
      if(p->uv_reprojection)result=std::max(result,8u);if(p->paint_sampling)result=std::max(result,9u);
    }
    if(const auto &p=b->normal_preparation){
      result=std::max(result,4u);if(p->uv_reprojection)result=std::max(result,5u);
      if(p->sampling_domain){result=std::max(result,7u);if(p->sampling_domain->metadata_sampling_domain)result=std::max(result,9u);}
    }
  }
  return result;
}
std::set<const PaintedMesh*> material_paint_owners(
    const std::vector<std::shared_ptr<const SourceMaterialBindings>> &bindings,
    const std::vector<std::shared_ptr<const SourceUvOrigin>> &origins) {
  std::set<const PaintedMesh*> owners;
  auto add=[&](const auto &p){if(p)owners.insert(p.get());};
  for(const auto &o:origins)add(o->paint);
  for(const auto &b:bindings){
    if(b->canonical_attributes)add(b->canonical_attributes->before_paint());
    if(b->key_derivation)add(b->key_derivation->before_paint());
    if(b->native_paint_sampling)add(b->native_paint_sampling->before_paint());
    if(const auto &p=b->color_preparation){add(p->original_paint);if(p->uv_reprojection)add(p->uv_reprojection->before_paint);if(p->paint_sampling)add(p->paint_sampling->before_paint());}
    if(const auto &p=b->normal_preparation){add(p->original_paint);if(p->uv_reprojection)add(p->uv_reprojection->before_paint);if(p->sampling_domain)add(p->sampling_domain->before_paint);}
  }
  return owners;
}
bool physical_material_state(const std::set<const PaintedMesh*> &paints,const std::vector<std::shared_ptr<const SourceMaterialBindings>> &bindings){
  return std::any_of(paints.begin(),paints.end(),[](const auto *p){return p->has_physical_emission();})||std::any_of(bindings.begin(),bindings.end(),[](const auto &b){return detail::physical_emission(*b)||(b->native_paint_sampling&&detail::hdr_emission(b->native_paint_sampling->before_native_material().emissive));});
}
size_t index(const nlohmann::json &value,size_t size) {
  if((!value.is_number_unsigned() && (!value.is_number_integer() || value.get<int64_t>()<0)) ||
     value.get<uint64_t>()>=size) throw std::runtime_error("Invalid resolved material archive identity");
  return size_t(value.get<uint64_t>());
}
}
void SourceNativeMaterial::validate() const {
  auto unit=[](float value){return std::isfinite(value) && value>=0 && value<=1;};
  if(!std::all_of(color.begin(),color.end(),unit) || !std::all_of(emission.begin(),emission.end(),[](float v){return std::isfinite(v)&&v>=0&&v<=maximum_native_emission;}) ||
     alpha<0 || alpha>3 || !unit(alpha_cutoff))
    throw std::invalid_argument("Invalid native factor baseline for resolved material");
}
void SourceMaterialBindings::validate(bool deep) const {
  detail::ColorBoundScope color_bound_scope;
  MaterialValidationScope validation(this,deep);
  if(validation.proved)return;
  if(size_t(normal_frame)>1 || size_t(animation_policy)>1 ||
     !std::isfinite(freeze_seconds) || freeze_seconds<0 || freeze_seconds>1e9 ||
     (animation_frozen && animation_policy!=SourceUvAnimationPolicy::FreezeAtTime))
    throw std::invalid_argument("Invalid derived material frame or animation provenance");
  if(sampling_reparameterized && !uv_origin)throw std::invalid_argument("Reparameterized sampling requires retained original provenance");
  if(color_preparation)color_preparation->validate(deep);
  if(canonical_uv){
    if(!canonical_uv->valid_owner(canonical_uv)||canonical_uv->origin()!=canonical_attributes)
      throw std::invalid_argument("Canonical UV correspondence lost its actual attribute origin");
    canonical_uv->validate_current(*canonical_uv->mesh());
  }
  if(canonical_attributes){
    if(!canonical_attributes->valid_owner(canonical_attributes)||has_mtoon||normal_preparation||
        normal_frame!=SourceMaterialNormalFrame::OriginalAttributes||!normal_frames)
      throw std::invalid_argument("Canonical attributes require their genuine separate native provenance");
    canonical_attributes->validate(deep);
    if(normal_frames!=canonical_attributes->frames()&&
       (!key_derivation||!key_derivation->before_material()||key_derivation->before_material()->canonical_attributes!=canonical_attributes))
      throw std::invalid_argument("Canonical attributes borrowed another current frame owner");
  }
  if(key_derivation){if(!key_derivation->valid_owner(key_derivation))throw std::invalid_argument("Material key derivation has no genuine owning node lease");if(!normal_frames||!owning_key_ref(key_derivation->frames(),normal_frames)||!owning_key_ref(key_derivation->position_domain(),normal_frames->position_domain))throw std::invalid_argument("Material key derivation lost its own native frame/POSITION authority");key_derivation->validate(deep);}
  if(normal_frames&&normal_frames->position_domain&&!key_derivation)throw std::invalid_argument("Derived native frame POSITION geometry needs an owning material key derivation");
  if(normal_preparation){if(!owning_key_ref(normal_preparation->key_derivation,key_derivation))throw std::invalid_argument("Prepared normal key replay differs from its actual material key derivation");normal_preparation->validate(deep);
    if(normal_frames!=normal_preparation->target_frames||normal_frame!=SourceMaterialNormalFrame::OriginalAttributes)
      throw std::invalid_argument("Prepared normal interpretation lost its exact target frame authority");
    if(normal_preparation->uv_reprojection&&has_mtoon){const auto &role=roles[size_t(MToonSlot::Normal)];if(role.image!=normal_preparation->projection->mtoon||role.wrap_s!=33071||role.wrap_t!=33071||role.min_filter!=9729||role.mag_filter!=9729)throw std::invalid_argument("Normal UV current resolved sampler/image differs from its declared projection");}
    if(has_mtoon){const auto expected=material.version()==MToonVersion::V0?PreparedMToonNormalEvaluation::LegacyRawInterpolated:PreparedMToonNormalEvaluation::MToon1;
      if(normal_preparation->receipt.mtoon!=expected)throw std::invalid_argument("Prepared normal interpretation cannot silently change its declared MToon version");
      const auto &normal=material.textures[size_t(MToonSlot::Normal)];
      const auto &sampling=material.version()==MToonVersion::V0?material.textures[0]:normal;
      if(normal.texcoord!=0||normal.transform_texcoord>0||sampling.offset!=std::array<float,2>{}||sampling.scale!=std::array<float,2>{1,1}||sampling.rotation!=0||material.parameters.scroll!=std::array<float,2>{}||material.parameters.rotation_speed!=0)
        throw std::invalid_argument("Prepared normal sampling edits require explicit source-aware reprojection; original material retained");
    }
  }
  if(native_paint_sampling){
    if(!native_paint_sampling->valid_owner(native_paint_sampling))throw std::invalid_argument("Native paint material has no genuine owning lease");
    native_paint_sampling->validate();
    if(canonical_attributes||canonical_uv||has_mtoon||normal_frame!=SourceMaterialNormalFrame::UvDerivative||color_preparation||normal_preparation||normal_frames||key_derivation||!sampling_reparameterized||!uv_origin||uv_origin->appearance||uv_origin->deformation||uv_origin->material||!uv_origin->creator_kind.empty()||uv_origin->creator_version||!uv_origin->creator_parameters.empty()||!owning_key_ref(uv_origin->paint,native_paint_sampling->original_paint()))throw std::invalid_argument("Native paint material lost its exact unprepared source interpretation");
    for(const auto &role:roles)if(role.image)throw std::invalid_argument("Native paint image roles need their independent adapter");
    for(const auto &role:uv_origin->roles)if(role.image)throw std::invalid_argument("Native paint original role cannot borrow an image");
    const auto &m=native_paint_sampling->original_native_material();
    if(native_reference.color!=m.color||native_reference.emission!=m.emissive||native_reference.alpha!=int(m.alpha)||native_reference.alpha_cutoff!=m.alpha_cutoff||native_reference.double_sided!=m.double_sided)throw std::invalid_argument("Native paint comparison baseline differs from its own source");
  }
  native_reference.validate();
  if(physical_emission)detail::validate_emission_rgb(*physical_emission,maximum_native_emission);
  roles_valid(has_mtoon?std::optional<MToonMaterial>(material):std::nullopt,roles);
  if(animation_frozen && has_mtoon &&
     (material.parameters.scroll!=std::array<float,2>{} || material.parameters.rotation_speed!=0))
    throw std::invalid_argument("An explicitly frozen material cannot retain active UV animation");
  if(uv_origin) {
    if(uv_origin->paint&&uv_origin->paint->normal_projection())throw std::invalid_argument("Original UV paint cannot infer a prepared-normal receipt from another material holder");
    if(uv_origin->paint&&uv_origin->paint->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored)throw std::invalid_argument("Source UV origin paint requires explicit strict conversion; polygon authority cannot be inferred");
    if(!uv_origin->paint || uv_origin->paint->in_stroke())
      throw std::invalid_argument("Derived UV material has no finished immutable source");
    roles_valid(uv_origin->material,uv_origin->roles);
    if(uv_origin->deformation && deep)uv_origin->deformation->validate(uv_origin->paint->mesh());
    if(uv_origin->creator_kind.empty()) {
      if(uv_origin->creator_version || !uv_origin->creator_parameters.empty())throw std::invalid_argument("Partial creator frame provenance");
    } else {
      if(uv_origin->creator_kind!="vroid-shape" || uv_origin->creator_version!=1 || uv_origin->creator_parameters.size()>64*1024)throw std::invalid_argument("Unknown creator frame provenance");
      if(deep)HumanoidControls::decode(nlohmann::json::parse(uv_origin->creator_parameters)).validate();
    }
    if(uv_origin->appearance && uv_origin->appearance->source_vertices.size()!=uv_origin->paint->mesh().positions.size())
      throw std::invalid_argument("Original UV material source remap differs from its retained paint mesh");
  }
  if(normal_frame==SourceMaterialNormalFrame::OriginalAttributes && !normal_frames)throw std::invalid_argument("Original attributes require explicit normal frames");
  if(normal_frames) {
    if(normal_frame!=SourceMaterialNormalFrame::OriginalAttributes)throw std::invalid_argument("Original frame payload cannot reinterpret legacy converted normal pixels");
    normal_frames->validate(deep);
    if(!uv_origin || !uv_origin->paint)throw std::invalid_argument("Normal frames require retained native rest authority");
    if(normal_frames->position_domain){const auto &root=normal_frames->position_domain->source_frame_basis();const auto original=uv_origin->paint->mesh_owner();if(root!=original||root.owner_before(original)||original.owner_before(root))throw std::invalid_argument("Derived native POSITION basis borrowed another original attribute-coordinate owner");}
    const auto &mesh=detail::frame_position_basis(*this);
    for(const auto &f:{normal_frames->pbr,normal_frames->mtoon,normal_frames->canonical})if(f) {
      if(f->origin_vertex_count!=mesh.positions.size() || f->normals.size()!=mesh.indices.size())throw std::invalid_argument("Normal frame origin geometry differs");
      if(deep && f->origin_vertices!=mesh.indices)throw std::invalid_argument("Normal frames must retain exact ordered origin corners");
      if(!f->source_vertices.empty()) {
        if(!uv_origin->appearance || !uv_origin->appearance->asset)throw std::invalid_argument("Original source corner frames require retained primitive authority");
        if(deep) {
          const auto &a=*uv_origin->appearance;const ImportedPrimitive *primitive=nullptr;
          for(const auto &p:a.asset->primitives)if(p.source_mesh==a.source_mesh && p.source_primitive==a.source_primitive){if(primitive)throw std::invalid_argument("Normal frame source primitive is ambiguous");primitive=&p;}
          if(!primitive || primitive->mesh.positions.size()!=f->source_vertex_count || primitive->mesh.indices.size()!=f->source_corner_count)throw std::invalid_argument("Normal frame source domain differs");
          for(size_t c=0;c<f->source_vertices.size();++c)if(f->source_vertices[c]!=a.source_vertices.at(uv_origin->paint->mesh().indices[c]) || primitive->mesh.indices.at(f->source_corners[c])!=f->source_vertices[c])throw std::invalid_argument("Normal frame source corner provenance differs");
        }
      }
    }
    std::set<uint32_t> source_targets;
    for(const auto &t:normal_frames->targets) {
      if(t.origin_slot && (!uv_origin->deformation || !uv_origin->deformation->morphs || *t.origin_slot>=uv_origin->deformation->morphs->targets.size()))throw std::invalid_argument("Normal frame origin target is unavailable");
      if(t.source_target && !source_targets.insert(*t.source_target).second)throw std::invalid_argument("Ambiguous original source morph identity");
      if(t.source_target && (!uv_origin->appearance || !t.origin_slot || *t.origin_slot>=uv_origin->appearance->source_morph_targets.size() || uv_origin->appearance->source_morph_targets[*t.origin_slot]!=*t.source_target))throw std::invalid_argument("Normal frame source target provenance differs");
    }
  }
  validation.complete(deep);
}
uint32_t SourceMaterialArchive::add(std::shared_ptr<const SourceMaterialBindings> value,bool deep) {
  if(!value) throw std::invalid_argument("Missing resolved material holder");
  auto found=std::find(bindings_.begin(),bindings_.end(),value);
  if(found!=bindings_.end())return uint32_t(found-bindings_.begin());
  if(bindings_.size()>=256)throw std::length_error("Resolved material archive count exceeds budget");
  value->validate(deep);
  if(value->canonical_attributes&&value->canonical_attributes->before_material())add(value->canonical_attributes->before_material(),deep);
  if(value->color_preparation && value->color_preparation->original_material)
    add(value->color_preparation->original_material,deep);
  if(value->color_preparation&&value->color_preparation->uv_reprojection)
    add(value->color_preparation->uv_reprojection->before_material,deep);
  if(value->color_preparation&&value->color_preparation->paint_sampling)
    add(value->color_preparation->paint_sampling->before_material(),deep);
  if(value->key_derivation)add(value->key_derivation->before_material(),deep);
  if(value->native_paint_sampling&&value->native_paint_sampling->before_material())add(value->native_paint_sampling->before_material(),deep);
  if(value->normal_preparation&&value->normal_preparation->original_material)
    add(value->normal_preparation->original_material,deep);
  if(value->normal_preparation&&value->normal_preparation->uv_reprojection)
    add(value->normal_preparation->uv_reprojection->before_material,deep);
  if(value->normal_preparation&&value->normal_preparation->sampling_domain){
    add(value->normal_preparation->sampling_domain->before_material,deep);
    add(value->normal_preparation->sampling_domain->sampling_material,deep);
  }
  if(bindings_.size()>=256)throw std::length_error("Resolved material archive count exceeds budget");
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
  return encode(images,models,nullptr);
}
nlohmann::json SourceMaterialArchive::encode(PaintImageArchive &images,CreatorAssetArchive &models,detail::PolygonColorArchive *colors) const {
  return encode(images,models,colors,nullptr);
}
nlohmann::json SourceMaterialArchive::encode(PaintImageArchive &images,CreatorAssetArchive &models,
    detail::PolygonColorArchive *colors,NormalSamplingArchive *domains) const {
  return encode(images,models,colors,domains,nullptr);
}
nlohmann::json SourceMaterialArchive::encode(PaintImageArchive &images,CreatorAssetArchive &models,
    detail::PolygonColorArchive *colors,NormalSamplingArchive *domains,PaintSamplingArchive *paint_domains) const {
  const bool keyed=std::any_of(bindings_.begin(),bindings_.end(),[](const auto&b){return bool(b->key_derivation);});
  const bool canonical_uv=std::any_of(bindings_.begin(),bindings_.end(),[](const auto &b){return bool(b->canonical_uv);});
  const bool layered=std::any_of(bindings_.begin(),bindings_.end(),[](const auto&b){return bool(b->canonical_attributes);});
  const bool corners=std::any_of(bindings_.begin(),bindings_.end(),[](const auto &b){return b->color_preparation&&b->color_preparation->polygon_colors;});
  if(corners&&!colors)throw std::invalid_argument("Current corner color preparation requires its shared Scene20 archive");
  const bool native_paint=std::any_of(bindings_.begin(),bindings_.end(),[](const auto &b){return bool(b->native_paint_sampling);});
  const bool paint_sampling=std::any_of(bindings_.begin(),bindings_.end(),[](const auto &b){return b->native_paint_sampling||(b->color_preparation&&b->color_preparation->paint_sampling)||(b->normal_preparation&&b->normal_preparation->sampling_domain&&b->normal_preparation->sampling_domain->metadata_sampling_domain);});
  if(paint_sampling&&!paint_domains)
    throw std::invalid_argument("Retained writable paint requires its complete material9/Scene24 context");
  const bool prepared=std::any_of(bindings_.begin(),bindings_.end(),[](const auto &b){return bool(b->color_preparation);});
  const bool normal_prepared=std::any_of(bindings_.begin(),bindings_.end(),[](const auto &b){return bool(b->normal_preparation);});
  const bool sampling=std::any_of(bindings_.begin(),bindings_.end(),[](const auto &b){return b->normal_preparation&&b->normal_preparation->sampling_domain;});
  if(sampling&&!domains)throw std::invalid_argument("Retained normal interpretation requires its shared owning archive");
  const bool extended=prepared || normal_prepared || std::any_of(bindings_.begin(),bindings_.end(),[](const auto &b){return bool(b->normal_frames) || (b->uv_origin && !b->sampling_reparameterized);}) || std::any_of(origins_.begin(),origins_.end(),[](const auto &o){return bool(o->deformation)||!o->creator_kind.empty();});
  NormalFrameArchive frames;
  nlohmann::json output={{"version",material_shape_version(bindings_,origins_)},{"paints",nlohmann::json::array()},{"origins",nlohmann::json::array()},{"bindings",nlohmann::json::array()}};
  std::vector<std::shared_ptr<const PaintedMesh>> paints;
  auto paint_id=[&](const std::shared_ptr<const PaintedMesh> &paint){
    auto found=std::find(paints.begin(),paints.end(),paint);
    if(found!=paints.end())return size_t(found-paints.begin());
    if(paints.size()>=256)throw std::length_error("Retained paint archive count exceeds budget");
    const auto id=paints.size();paints.push_back(paint);
    if(layered)detail::BakedNormalArchive::writer().register_paint(paint,uint32_t(id));
    output["paints"].push_back(paint->encode(&images,domains,paint_domains));return id;
  };
  for(const auto &b:bindings_)if(b->canonical_attributes){detail::BakedNormalArchive::writer().add_origin(b->canonical_attributes);paint_id(b->canonical_attributes->before_paint());}
  std::vector<std::shared_ptr<const detail::NativePaintSampling>> native_paints;
  if(native_paint){
    output["native_paints"]=nlohmann::json::array();size_t layout_bytes=0;
    for(const auto &binding:bindings_)if(const auto &node=binding->native_paint_sampling;node&&std::find(native_paints.begin(),native_paints.end(),node)==native_paints.end()){
      if(native_paints.size()>=128)throw std::length_error("Native paint archive count exceeds budget");
      const auto bytes=node->layout()->storage_bytes();if(bytes>128*1024*1024-layout_bytes)throw std::length_error("Native paint layouts exceed archive budget");layout_bytes+=bytes;
      const auto before=std::find(bindings_.begin(),bindings_.end(),node->before_material());
      if(node->before_material()&&before==bindings_.end())throw std::logic_error("Missing native paint before material");
      native_paints.push_back(node);output["native_paints"].push_back({{"version",1},{"paint",paint_id(node->before_paint())},
        {"before_binding",node->before_material()?nlohmann::json(size_t(before-bindings_.begin())):nlohmann::json(nullptr)},
        {"native_material",detail::encode_native_material(node->before_native_material())},{"layout",node->layout()->encode()},
        {"domain",paint_domains->add(node->domain())},{"depth",node->history_depth()}});
    }
  }
  std::vector<std::shared_ptr<const detail::SourceKeyDerivation>> key_derivations;
  if(keyed){output["key_derivations"]=nlohmann::json::array();for(const auto &binding:bindings_)if(binding->key_derivation&&std::find(key_derivations.begin(),key_derivations.end(),binding->key_derivation)==key_derivations.end()){const auto node=binding->key_derivation;const auto before=std::find(bindings_.begin(),bindings_.end(),node->before_material());if(before==bindings_.end())throw std::logic_error("Missing key derivation before material");key_derivations.push_back(node);output["key_derivations"].push_back(detail::KeyDerivationCodec::encode(*node,frames,uint32_t(paint_id(node->before_paint())),uint32_t(before-bindings_.begin()),models,images));}}
  std::vector<std::shared_ptr<const SourceColorPreparation>> preparations;size_t color_uv_layout_bytes=0;
  if(prepared||normal_prepared||keyed) {
    output["color_preparations"]=nlohmann::json::array();
    for(const auto &binding:bindings_)if(binding->color_preparation && std::find(preparations.begin(),preparations.end(),binding->color_preparation)==preparations.end()) {
      const auto &p=*binding->color_preparation;p.validate();preparations.push_back(binding->color_preparation);
      auto before=std::find(bindings_.begin(),bindings_.end(),p.original_material);
      if(p.original_material && before==bindings_.end())throw std::logic_error("Missing color preparation original binding");
      output["color_preparations"].push_back({{"version",1},{"projection",encode_color_projection(*p.projection,images)},
        {"initial_projection",encode_color_projection(*p.initial_projection,images)},{"receipt",p.receipt.encode()},
        {"paint",paint_id(p.original_paint)},
        {"appearance",p.original_appearance?encode_creator_appearance(*p.original_appearance,models,images):nlohmann::json(nullptr)},
        {"deformation",p.original_deformation?p.original_deformation->encode(p.original_paint->mesh()):nlohmann::json(nullptr)},
        {"before_binding",p.original_material?nlohmann::json(size_t(before-bindings_.begin())):nlohmann::json(nullptr)}});
      if(p.polygon_colors){
        auto &entry=output["color_preparations"].back();entry["version"]=2;
        entry["polygon_colors"]={{"topology",p.polygon_colors->topology->encode()},{"field",colors->add(p.polygon_colors->field)}};
      }
      if(p.uv_reprojection){const auto &u=*p.uv_reprojection;
        const auto bytes=u.layout->storage_bytes();if(bytes>128*1024*1024-color_uv_layout_bytes)throw std::length_error("Colour UV layouts exceed archive budget");color_uv_layout_bytes+=bytes;
        const auto previous=std::find(bindings_.begin(),bindings_.end(),u.before_material);
        if(previous==bindings_.end())throw std::logic_error("Missing colour UV prepared predecessor");
        auto &entry=output["color_preparations"].back();entry["version"]=3;
        entry["uv_reprojection"]={{"version",1},{"receipt",u.receipt.encode()},{"layout",u.layout->encode()},
          {"paint",paint_id(u.before_paint)},{"appearance",u.before_appearance?encode_creator_appearance(*u.before_appearance,models,images):nlohmann::json(nullptr)},
          {"deformation",u.before_deformation?u.before_deformation->encode(u.before_paint->mesh()):nlohmann::json(nullptr)},
          {"before_binding",size_t(previous-bindings_.begin())},
          {"polygon_colors",{{"topology",u.polygon_colors->topology->encode()},{"field",colors->add(u.polygon_colors->field)}}}};
      }
      if(p.paint_sampling){const auto &u=*p.paint_sampling;
        const auto bytes=u.layout()->storage_bytes();if(bytes>128*1024*1024-color_uv_layout_bytes)throw std::length_error("Retained paint layouts exceed archive budget");color_uv_layout_bytes+=bytes;
        const auto previous=std::find(bindings_.begin(),bindings_.end(),u.before_material());
        if(previous==bindings_.end())throw std::logic_error("Missing retained corner paint predecessor");
        auto &entry=output["color_preparations"].back();entry["version"]=4;
        entry["paint_sampling"]={{"version",1},{"receipt",u.receipt().encode()},{"layout",u.layout()->encode()},
          {"domain",paint_domains->add(u.domain())},{"paint",paint_id(u.before_paint())},
          {"appearance",u.before_appearance()?encode_creator_appearance(*u.before_appearance(),models,images):nlohmann::json(nullptr)},
          {"deformation",u.before_deformation()?u.before_deformation()->encode(u.before_paint()->mesh()):nlohmann::json(nullptr)},
          {"before_binding",size_t(previous-bindings_.begin())},
          {"polygon_colors",{{"topology",u.current_descriptor()->topology->encode()},{"field",colors->add(u.current_descriptor()->field)}}}};
      }
    }
  }
  std::vector<std::shared_ptr<const SourceNormalPreparation>> normal_preparations;size_t normal_uv_layout_bytes=0, sampling_metadata=0;
  std::vector<std::shared_ptr<const detail::CanonicalNormalConversion>> canonical_conversions;
  if(normal_prepared||corners||keyed){
    output["normal_preparations"]=nlohmann::json::array();
    for(const auto &binding:bindings_)if(binding->normal_preparation&&std::find(normal_preparations.begin(),normal_preparations.end(),binding->normal_preparation)==normal_preparations.end()){
      const auto &p=*binding->normal_preparation;p.validate();normal_preparations.push_back(binding->normal_preparation);
      const auto before=std::find(bindings_.begin(),bindings_.end(),p.original_material);
      if(p.original_material&&before==bindings_.end())throw std::logic_error("Missing normal preparation original binding");
      output["normal_preparations"].push_back({{"version",1},{"projection",encode_normal_projection(*p.projection,images,domains)},
        {"initial_projection",encode_normal_projection(*p.initial_projection,images,domains)},{"receipt",p.receipt.encode()},
        {"frames",frames.add(p.target_frames)},{"paint",paint_id(p.original_paint)},
        {"appearance",p.original_appearance?encode_creator_appearance(*p.original_appearance,models,images):nlohmann::json(nullptr)},
        {"deformation",p.original_deformation?p.original_deformation->encode(p.original_paint->mesh()):nlohmann::json(nullptr)},
        {"before_binding",p.original_material?nlohmann::json(size_t(before-bindings_.begin())):nlohmann::json(nullptr)}});
      if(p.uv_reprojection){const auto &u=*p.uv_reprojection;const auto layout_bytes=u.layout->storage_bytes();if(layout_bytes>128*1024*1024-normal_uv_layout_bytes)throw std::length_error("Normal UV layout archive exceeds global budget");normal_uv_layout_bytes+=layout_bytes;const auto original=std::find(bindings_.begin(),bindings_.end(),u.before_material);if(original==bindings_.end())throw std::logic_error("Missing normal UV before-state binding");auto &entry=output["normal_preparations"].back();entry["version"]=2;
        entry["uv_reprojection"]={{"version",1},{"receipt",u.receipt.encode()},{"layout",u.layout->encode()},{"projection",encode_normal_projection(*u.projection,images,domains)},{"paint",paint_id(u.before_paint)},
          {"appearance",u.before_appearance?encode_creator_appearance(*u.before_appearance,models,images):nlohmann::json(nullptr)},
          {"deformation",u.before_deformation?u.before_deformation->encode(u.before_paint->mesh()):nlohmann::json(nullptr)},
          {"before_binding",size_t(original-bindings_.begin())}};
      }
      if(p.sampling_domain){const auto &u=*p.sampling_domain;
        sampling_metadata+=metadata_bytes(u.current_sampling_reference);
        if(sampling_metadata>16*1024*1024)throw std::length_error("Retained normal sampling metadata exceeds budget");
        const auto bytes=u.layout->storage_bytes();if(bytes>128*1024*1024-normal_uv_layout_bytes)
          throw std::length_error("Normal domain layouts exceed their global archive budget");
        normal_uv_layout_bytes+=bytes;
        const auto before=std::find(bindings_.begin(),bindings_.end(),u.before_material);
        const auto sampler=std::find(bindings_.begin(),bindings_.end(),u.sampling_material);
        if(before==bindings_.end()||sampler==bindings_.end())throw std::logic_error("Missing retained normal source binding");
        auto &entry=output["normal_preparations"].back();entry["version"]=3;
        entry["sampling_domain"]={{"version",1},{"receipt",u.receipt.encode()},
          {"layout",u.layout->encode()},{"projection",encode_normal_projection(*u.projection,images,domains)},
          {"paint",paint_id(u.before_paint)},
          {"appearance",u.before_appearance?encode_creator_appearance(*u.before_appearance,models,images):nlohmann::json(nullptr)},
          {"deformation",u.before_deformation?u.before_deformation->encode(u.before_paint->mesh()):nlohmann::json(nullptr)},
          {"before_binding",size_t(before-bindings_.begin())},{"sampling_binding",size_t(sampler-bindings_.begin())},
          {"current_sampling_reference",u.current_sampling_reference?u.current_sampling_reference->encode():nlohmann::json(nullptr)}};
        if(u.metadata_sampling_domain){entry["version"]=4;entry["sampling_domain"]["version"]=2;entry["sampling_domain"]["metadata_sampling_domain"]=paint_domains->add(u.metadata_sampling_domain);}
      }
      if(p.key_derivation){const auto key=std::find(key_derivations.begin(),key_derivations.end(),p.key_derivation);if(key==key_derivations.end())throw std::logic_error("Prepared key recipe lacks its owning material node");auto &entry=output["normal_preparations"].back();entry["version"]=p.key_derivation->uses_explicit_mirror()?6:5;entry["key_derivation"]=size_t(key-key_derivations.begin());}
      if(p.canonical_conversion){
        const auto &owner=p.canonical_conversion;auto at=std::find(canonical_conversions.begin(),canonical_conversions.end(),owner);
        if(at==canonical_conversions.end()){
          if(p.key_derivation||p.uv_reprojection||p.sampling_domain||p.target_frames!=owner->frames())
            throw std::logic_error("Canonical conversion must archive its original owning anchor before descendants");
          if(canonical_conversions.empty())output["canonical_conversions"]=nlohmann::json::array();
          canonical_conversions.push_back(owner);at=canonical_conversions.end()-1;
          output["canonical_conversions"].push_back({{"version",1},{"anchor",normal_preparations.size()-1},{"receipt",owner->encode()}});
        }
        auto &entry=output["normal_preparations"].back();entry["base_version"]=entry.at("version");entry["version"]=7;
        entry["canonical_conversion"]=size_t(at-canonical_conversions.begin());
      }
    }
  }
  size_t metadata=sampling_metadata;
  for(const auto &origin:origins_) {
    metadata+=metadata_bytes(origin->material);
    if(metadata>16*1024*1024)throw std::length_error("Scene resolved material metadata exceeds budget");
    output["origins"].push_back({{"paint",paint_id(origin->paint)},
      {"appearance",origin->appearance?encode_creator_appearance(*origin->appearance,models,images):nlohmann::json(nullptr)},
      {"material",origin->material?origin->material->encode():nlohmann::json(nullptr)},
      {"roles",encode_roles(origin->roles,images)}});
    if(extended){output["origins"].back()["deformation"]=origin->deformation?origin->deformation->encode(origin->paint->mesh()):nlohmann::json(nullptr);
      output["origins"].back()["creator"]=origin->creator_kind.empty()?nlohmann::json(nullptr):nlohmann::json{{"kind",origin->creator_kind},{"version",origin->creator_version},{"parameters",origin->creator_parameters}};}
  }
  for(const auto &binding:bindings_) {
    binding->validate();metadata+=metadata_bytes(binding->has_mtoon?std::optional<MToonMaterial>(binding->material):std::nullopt);
    if(metadata>16*1024*1024)throw std::length_error("Scene resolved material metadata exceeds budget");
    auto origin=std::find(origins_.begin(),origins_.end(),binding->uv_origin);
    output["bindings"].push_back({{"material",binding->has_mtoon?binding->material.encode():nlohmann::json(nullptr)},
      {"roles",encode_roles(binding->roles,images)}, {"normal_frame",binding->normal_frame==SourceMaterialNormalFrame::UvDerivative?"uv-derivative":"original-attributes"},
      {"animation_policy",int(binding->animation_policy)}, {"freeze_seconds",binding->freeze_seconds},
      {"animation_frozen",binding->animation_frozen},
      {"native_reference",{{"color",binding->native_reference.color},{"emission",binding->native_reference.emission},
        {"alpha",binding->native_reference.alpha},{"alpha_cutoff",binding->native_reference.alpha_cutoff},
        {"double_sided",binding->native_reference.double_sided}}},
      {"origin",binding->uv_origin?nlohmann::json(size_t(origin-origins_.begin())):nlohmann::json(nullptr)}});
    if(binding->physical_emission)output["bindings"].back()["physical_emission"]=*binding->physical_emission;
    if(canonical_uv)output["bindings"].back()["canonical_uv"]=binding->canonical_uv?
      nlohmann::json(detail::BakedNormalArchive::writer().add_uv(binding->canonical_uv)):nlohmann::json(nullptr);
    if(layered)output["bindings"].back()["canonical_attributes"]=binding->canonical_attributes?
      nlohmann::json(detail::BakedNormalArchive::writer().add_origin(binding->canonical_attributes)):nlohmann::json(nullptr);
    if(native_paint){const auto found=std::find(native_paints.begin(),native_paints.end(),binding->native_paint_sampling);
      output["bindings"].back()["native_paint_sampling"]=binding->native_paint_sampling?nlohmann::json(size_t(found-native_paints.begin())):nlohmann::json(nullptr);}

    if(normal_prepared||corners||keyed){const auto found=std::find(normal_preparations.begin(),normal_preparations.end(),binding->normal_preparation);
      output["bindings"].back()["normal_preparation"]=binding->normal_preparation?nlohmann::json(size_t(found-normal_preparations.begin())):nlohmann::json(nullptr);
    }
    if(prepared||normal_prepared||keyed) {
      const auto found=std::find(preparations.begin(),preparations.end(),binding->color_preparation);
      output["bindings"].back()["color_preparation"]=binding->color_preparation?nlohmann::json(size_t(found-preparations.begin())):nlohmann::json(nullptr);
    }
    if(keyed){const auto found=std::find(key_derivations.begin(),key_derivations.end(),binding->key_derivation);output["bindings"].back()["key_derivation"]=binding->key_derivation?nlohmann::json(size_t(found-key_derivations.begin())):nlohmann::json(nullptr);}
    if(extended){output["bindings"].back()["frames"]=binding->normal_frames?nlohmann::json(frames.add(binding->normal_frames)):nlohmann::json(nullptr);output["bindings"].back()["sampling_reparameterized"]=binding->sampling_reparameterized;}
  }
  if(extended)output["normal_frames"]=keyed?detail::KeyFrameArchive::encode(frames):frames.encode();
  const bool physical=physical_material_state(material_paint_owners(bindings_,origins_),bindings_);
  const bool explicit_mirror=std::any_of(bindings_.begin(),bindings_.end(),[](const auto &b){return b->key_derivation&&b->key_derivation->uses_explicit_mirror();});
  if(native_paint){output["base_version"]=output.at("version");output["version"]=14;output["physical_emission"]=physical;output["explicit_mirror"]=explicit_mirror;output["canonical_normals"]=false;output["native_paint_sampling"]=true;}
  else if(explicit_mirror){output["base_version"]=10;output["version"]=12;output["physical_emission"]=physical;}
  else if(physical){output["base_version"]=output.at("version");output["version"]=11;}
  if(!canonical_conversions.empty()){
    // The conversion pool is already staged, but its wire capability has not
    // been wrapped yet. The strict decoder must reject that intermediate DTO;
    // take the writer's actual selected layout before publishing material13.
    const auto base=output.contains("base_version")?output.at("base_version"):output.at("version");
    output["base_version"]=base;output["version"]=13;
    output["physical_emission"]=physical;output["explicit_mirror"]=explicit_mirror;output["canonical_normals"]=true;
  }
  if(layered){
    const auto base=output.contains("base_version")?output.at("base_version"):output.at("version");
    output["base_version"]=base;output["version"]=canonical_uv?16:15;if(canonical_uv)output["retained_canonical_uv"]=true;output["physical_emission"]=physical;
    output["explicit_mirror"]=explicit_mirror;output["canonical_normals"]=!canonical_conversions.empty();output["layered_normals"]=true;
  }
  if(native_paint&&(layered||!canonical_conversions.empty())){
    output["version"]=17;output["native_paint_sampling"]=true;
    output["layered_normals"]=layered;output["retained_canonical_uv"]=canonical_uv;
  }
  return output;
}
size_t SourceMaterialArchive::deformation_entries(const nlohmann::json &input) {
  size_t entries=0;
  if(detail::explicit_mirror_material_archive(input)&&!detail::encoded_key_context(input))throw std::invalid_argument("Material12 requires actual owning record4 preflight");
  if(detail::material_archive_base_version(input)==1)return 0;
  if((detail::material_archive_base_version(input)!=2 && detail::material_archive_base_version(input)!=3 && detail::material_archive_base_version(input)!=4 && detail::material_archive_base_version(input)!=5 && detail::material_archive_base_version(input)!=6 && detail::material_archive_base_version(input)!=7 && detail::material_archive_base_version(input)!=8 && detail::material_archive_base_version(input)!=9 && detail::material_archive_base_version(input)!=10) || !input.at("origins").is_array() || input.at("origins").size()>256)
    throw std::runtime_error("Invalid original deformation archive");
  auto add=[&](size_t n){if(n>8'000'000-entries)throw std::length_error("Original deformation archive exceeds scene budget");entries+=n;};
  auto count=[&](const nlohmann::json &d) {
    if(d.is_null())return;
    if(!d.at("pose").is_array() || d.at("pose").size()>256 || !d.at("morph_weights").is_array() || d.at("morph_weights").size()>128)
      throw std::runtime_error("Invalid original deformation preview dimensions");
    add(d.at("pose").size());add(d.at("morph_weights").size());add(detail::encoded_pose_matrix_entries(d));
    if(!d.at("skin").is_null()) {
      const auto &vertices=d.at("skin").at("vertices");
      if(!vertices.is_array() || vertices.size()>2'000'000)throw std::runtime_error("Invalid original skin count");
      for(const auto &v:vertices){if(!v.is_array() || v.size()>32)throw std::runtime_error("Invalid original skin influences");add(v.size());}
    }
    if(!d.at("morphs").is_null()) {
      const auto &targets=d.at("morphs").at("targets");
      if(!targets.is_array() || targets.size()>128)throw std::runtime_error("Invalid original morph count");
      for(const auto &t:targets){const auto &v=t.at("delta");if(!v.is_array() || v.size()>2'000'000)throw std::runtime_error("Invalid original morph positions");add(v.size());}
    }
  };
  for(const auto &origin:input.at("origins"))count(origin.at("deformation"));
  if(detail::material_archive_base_version(input)==3||detail::material_archive_base_version(input)==4||detail::material_archive_base_version(input)==5||detail::material_archive_base_version(input)==6||detail::material_archive_base_version(input)==7||detail::material_archive_base_version(input)==8||detail::material_archive_base_version(input)==9||detail::material_archive_base_version(input)==10) {
    const auto &preparations=input.at("color_preparations");
    if(!preparations.is_array() || (detail::material_archive_base_version(input)==3&&preparations.empty()) || preparations.size()>256)throw std::runtime_error("Invalid color preparation archive");
    size_t color_uv_bytes=0;bool found_color_uv=false;
    for(const auto &p:preparations){count(p.at("deformation"));if(p.contains("uv_reprojection")){
      if((detail::material_archive_base_version(input)!=8&&detail::material_archive_base_version(input)!=9 && detail::material_archive_base_version(input)!=10)||(p.at("version")!=3&&p.at("version")!=4)||!p.contains("polygon_colors"))throw std::invalid_argument("Colour UV replay requires material8/entry3");
      const auto &u=p.at("uv_reprojection");if(!u.is_object()||u.size()!=8||u.at("version")!=1)throw std::invalid_argument("Unsupported colour UV record");
      ColorUvReprojectionReceipt::decode(u.at("receipt"));const auto bytes=SourceColorUvLayout::encoded_storage_bytes(u.at("layout"));
      if(bytes>128*1024*1024-color_uv_bytes)throw std::length_error("Colour UV layouts exceed archive budget");color_uv_bytes+=bytes;count(u.at("deformation"));found_color_uv=true;
    }}
    for(const auto &p:preparations)if(p.contains("paint_sampling")){
      if((detail::material_archive_base_version(input)!=9&&detail::material_archive_base_version(input)!=10)||p.at("version")!=4||!p.contains("polygon_colors"))throw std::invalid_argument("Retained paint colour requires material9/entry4");
      const auto &u=p.at("paint_sampling");if(!u.is_object()||u.size()!=9||u.at("version")!=1)throw std::invalid_argument("Unsupported retained paint colour source record");
      CornerPaintSamplingReceipt::decode(u.at("receipt"));const auto bytes=SourceColorUvLayout::encoded_storage_bytes(u.at("layout"));
      if(bytes>128*1024*1024-color_uv_bytes)throw std::length_error("Retained paint layouts exceed material budget");color_uv_bytes+=bytes;count(u.at("deformation"));
    }
    if(detail::material_archive_base_version(input)==8&&!found_color_uv)throw std::invalid_argument("Material8 requires actual colour UV authority");
  }
  if(detail::material_archive_base_version(input)==4||detail::material_archive_base_version(input)==5||detail::material_archive_base_version(input)==6||detail::material_archive_base_version(input)==7||detail::material_archive_base_version(input)==8||detail::material_archive_base_version(input)==9||detail::material_archive_base_version(input)==10){const auto &preparations=input.at("normal_preparations");
    if(!preparations.is_array()||(detail::material_archive_base_version(input)!=6&&detail::material_archive_base_version(input)!=8&&detail::material_archive_base_version(input)!=9&&detail::material_archive_base_version(input)!=10&&preparations.empty())||preparations.size()>256)throw std::invalid_argument("Invalid normal preparation archive");
    size_t uv_bytes=0;bool found_uv=false,found_sampling=false;
    for(const auto &p:preparations){count(p.at("deformation"));if(p.contains("uv_reprojection")){if(detail::material_archive_base_version(input)<5||(detail::normal_preparation_base_version(p,input)!=2&&!(detail::material_archive_base_version(input)==10&&(detail::normal_preparation_base_version(p,input)==5||(detail::explicit_mirror_material_archive(input)&&detail::normal_preparation_base_version(p,input)==6)))))throw std::invalid_argument("Normal UV projections require material archive5/entry2");const auto &u=p.at("uv_reprojection");if(!u.is_object()||u.size()!=8||u.at("version")!=1)throw std::invalid_argument("Unsupported normal UV projection record");const auto bytes=SourceNormalUvLayout::encoded_storage_bytes(u.at("layout"));if(bytes>128*1024*1024-uv_bytes)throw std::length_error("Normal UV layout archive exceeds its global budget");uv_bytes+=bytes;count(u.at("deformation"));found_uv=true;}}
    for(const auto &p:preparations)if(p.contains("sampling_domain")){
      if((detail::material_archive_base_version(input)!=7&&detail::material_archive_base_version(input)!=8&&detail::material_archive_base_version(input)!=9 && detail::material_archive_base_version(input)!=10)||(detail::normal_preparation_base_version(p,input)!=3&&detail::normal_preparation_base_version(p,input)!=4&&!(detail::material_archive_base_version(input)==10&&(detail::normal_preparation_base_version(p,input)==5||(detail::explicit_mirror_material_archive(input)&&detail::normal_preparation_base_version(p,input)==6))))||p.contains("uv_reprojection"))
        throw std::invalid_argument("Retained normal domains require archive7/entry3");
      const auto &u=p.at("sampling_domain");
      if(!u.is_object()||u.size()!=(u.contains("metadata_sampling_domain")?11u:10u)||u.at("version")!=(u.contains("metadata_sampling_domain")?2:1)||(u.contains("metadata_sampling_domain")&&detail::material_archive_base_version(input)!=9&&detail::material_archive_base_version(input)!=10))
        throw std::invalid_argument("Unsupported retained normal source record");
      const auto bytes=SourceNormalUvLayout::encoded_storage_bytes(u.at("layout"));
      if(bytes>128*1024*1024-uv_bytes)throw std::length_error("Normal source layouts exceed their shared archive budget");
      uv_bytes+=bytes;count(u.at("deformation"));found_sampling=true;
    }
    if(detail::material_archive_base_version(input)==7&&!found_sampling)throw std::invalid_argument("Material archive7 requires its retained normal feature");
    if(detail::material_archive_base_version(input)==5&&!found_uv)throw std::invalid_argument("Material archive5 requires its normal UV feature");
  }
  if(detail::material_archive_base_version(input)==10){const auto &keys=input.at("key_derivations");if(!keys.is_array()||keys.empty()||keys.size()>256)throw std::invalid_argument("Material10 requires actual owning key records");for(const auto &k:keys){if(!k.is_object()||k.size()!=7||!k.at("version").is_number_integer()||(k.at("version")!=1&&k.at("version")!=2&&k.at("version")!=3&&!(detail::explicit_mirror_material_archive(input)&&k.at("version")==4))||!k.at("targets").is_array()||k.at("targets").empty()||k.at("targets").size()>128)throw std::invalid_argument("Invalid owning key record preflight");index(k.at("paint"),input.at("paints").size());count(k.at("deformation"));}for(size_t i=0;i<input.at("bindings").size();++i){const auto &ref=input.at("bindings")[i].at("key_derivation");if(!ref.is_null()){const auto &k=keys.at(index(ref,keys.size()));index(k.at("before_binding"),i);}}}
  return entries;
}
SourceMaterialArchive SourceMaterialArchive::decode(const nlohmann::json &input,const PaintImageArchive &images,
                                                   const CreatorAssetArchive &models) {
  return decode(input,images,models,nullptr);
}
SourceMaterialArchive SourceMaterialArchive::decode(const nlohmann::json &input,const PaintImageArchive &images,
                                                   const CreatorAssetArchive &models,const detail::PolygonColorArchive *colors) {
  return decode(input,images,models,colors,nullptr);
}
SourceMaterialArchive SourceMaterialArchive::decode(const nlohmann::json &input,const PaintImageArchive &images,
    const CreatorAssetArchive &models,const detail::PolygonColorArchive *colors,const NormalSamplingArchive *domains) {
  return decode(input,images,models,colors,domains,nullptr);
}
SourceMaterialArchive SourceMaterialArchive::decode(const nlohmann::json &input,const PaintImageArchive &images,
    const CreatorAssetArchive &models,const detail::PolygonColorArchive *colors,const NormalSamplingArchive *domains,const PaintSamplingArchive *paint_domains) {
  return decode(input,images,models,colors,domains,paint_domains,SourcePaintDecodeContext{});
}
SourceMaterialArchive SourceMaterialArchive::decode(const nlohmann::json &input,const PaintImageArchive &images,
    const CreatorAssetArchive &models,const detail::PolygonColorArchive *colors,const NormalSamplingArchive *domains,const PaintSamplingArchive *paint_domains,const SourcePaintDecodeContext &supplied) {
  const bool physical=detail::physical_material_archive(input);
  const bool layered=detail::layered_material_archive(input);
  const bool canonical_uv=detail::canonical_uv_material_archive(input);
  if(!canonical_uv)for(const auto &b:input.at("bindings"))if(b.contains("canonical_uv"))
    throw std::invalid_argument("Retained canonical UV reference requires actual material16/Scene34");
  if(layered)(void)detail::BakedNormalArchive::reader();
  else for(const auto &b:input.at("bindings"))if(b.contains("canonical_attributes"))throw std::invalid_argument("Canonical attribute reference requires actual material15/Scene33");
  const bool native_paint=detail::native_paint_material_archive(input);
  if(native_paint&&!paint_domains)throw std::invalid_argument("Native paint14 requires its genuine owning sampling pool");
  if(!native_paint)for(const auto &b:input.at("bindings"))if(b.contains("native_paint_sampling"))throw std::invalid_argument("Native paint binding requires capability14");
  SourcePaintDecodeContext context=supplied;context.physical_emission=physical;
  if(!detail::material_archive_base_version(input).is_number_integer() || (detail::material_archive_base_version(input)!=1 && detail::material_archive_base_version(input)!=2 && detail::material_archive_base_version(input)!=3 && detail::material_archive_base_version(input)!=4 && detail::material_archive_base_version(input)!=5 && detail::material_archive_base_version(input)!=6 && detail::material_archive_base_version(input)!=8 && detail::material_archive_base_version(input)!=7 && detail::material_archive_base_version(input)!=9 && detail::material_archive_base_version(input)!=10) || !input.at("paints").is_array() || input.at("paints").size()>256 || !input.at("origins").is_array() || input.at("origins").size()>256 ||
     !input.at("bindings").is_array() || input.at("bindings").size()>256)
    throw std::runtime_error("Invalid resolved material archive");
  const bool keyed=detail::material_archive_base_version(input)==10;
  if(detail::explicit_mirror_material_archive(input)&&!detail::encoded_key_context(input))throw std::invalid_argument("Material12 requires actual owning record4 semantics");
  if(!keyed){if(input.contains("key_derivations"))throw std::invalid_argument("Key derivations require owning material10");for(const auto &b:input.at("bindings"))if(b.contains("key_derivation"))throw std::invalid_argument("Key binding reference requires owning material10");if(input.contains("normal_preparations"))for(const auto &p:input.at("normal_preparations"))if(p.contains("key_derivation"))throw std::invalid_argument("Normal key entry requires owning material10");}
  if(detail::material_archive_base_version(input)==9&&!paint_domains)throw std::invalid_argument("Material9 requires its actual paired Paint sampling pool");
  auto has_leaf=[&](const char *table,const char *key){if(!input.contains(table))return false;const auto &a=input.at(table);if(!a.is_array()||a.size()>256)throw std::invalid_argument("Invalid material feature table");return std::any_of(a.begin(),a.end(),[&](const auto &p){return p.contains(key);});};
  const bool keyed_corners=keyed&&has_leaf("color_preparations","polygon_colors");
  const bool keyed_paint=keyed&&(has_leaf("color_preparations","paint_sampling")||std::any_of(input.at("normal_preparations").begin(),input.at("normal_preparations").end(),[](const auto&p){return p.contains("sampling_domain")&&p.at("sampling_domain").contains("metadata_sampling_domain");}));
  if(keyed_corners&&!colors)throw std::invalid_argument("Material10 current-corner leaves require their actual shared field pool");
  if(keyed_paint&&!paint_domains)throw std::invalid_argument("Material10 writable paint leaves require their actual owning pool");
  const bool corners=detail::material_archive_base_version(input)>=6;
  const bool sampling=detail::material_archive_base_version(input)>=7;
  if((detail::material_archive_base_version(input)==6||detail::material_archive_base_version(input)==8||detail::material_archive_base_version(input)==9)&&!colors)throw std::invalid_argument("Current corner color preparation requires Scene20 geometry authority");
  if(detail::material_archive_base_version(input)==7&&!domains)throw std::invalid_argument("Retained normal preparation requires its owning Scene21 field pool");
  if(!domains&&input.contains("normal_preparations"))for(const auto &p:input.at("normal_preparations"))if(p.contains("sampling_domain"))throw std::invalid_argument("Retained normal recipe needs its actual shared context");
  const bool normal_uv=detail::material_archive_base_version(input)>=5;
  const bool normal_prepared=detail::material_archive_base_version(input)==4||normal_uv;
  const bool prepared=detail::material_archive_base_version(input)==3||normal_prepared;
  const bool extended=detail::material_archive_base_version(input)!=1;
  if(!normal_prepared){
    if(input.contains("normal_preparations"))throw std::invalid_argument("Normal preparations require material archive4");
    for(const auto &b:input.at("bindings"))if(b.contains("normal_preparation"))throw std::invalid_argument("Normal preparation references require material archive4");
    for(const auto &record:input.at("paints"))detail::visit_paint_archive_payloads(record,(native_paint||detail::material_archive_base_version(input)==9),context,[&](const nlohmann::json &p){
      if(p.contains("normal_projection")||(p.contains("baked")&&p.at("baked").at("version")==3))throw std::invalid_argument("Prepared normal paint requires material archive4");
    });
  }
  if(!prepared) {
    if(input.contains("color_preparations"))throw std::runtime_error("Color preparations require material archive3");
    for(const auto &b:input.at("bindings"))if(b.contains("color_preparation"))throw std::runtime_error("Color preparation references require material archive3");
    for(const auto &record:input.at("paints"))detail::visit_paint_archive_payloads(record,(native_paint||detail::material_archive_base_version(input)==9),context,[&](const nlohmann::json &p) {
      if(p.contains("color_projection") || (p.contains("baked") && p.at("baked").at("version")!=1&&!(layered&&p.at("baked").at("version")==5)))throw std::runtime_error("Prepared paint requires material archive3");
      for(const auto &l:p.at("layers"))if(!native_paint&&l.contains("pbr_color_tiles"))throw std::runtime_error("Prepared paint layers require material archive3");
    });
  }
  if(!extended){if(input.contains("normal_frames"))throw std::runtime_error("Normal frames require material archive2");for(const auto &o:input.at("origins"))if(o.contains("deformation")||o.contains("creator"))throw std::runtime_error("Native frame origin requires material archive2");for(const auto &b:input.at("bindings"))if(b.contains("frames") || b.contains("sampling_reparameterized"))throw std::runtime_error("Normal frame references require material archive2");}
  deformation_entries(input);
  if(detail::material_archive_base_version(input)==9||detail::material_archive_base_version(input)==10){
    // All new graph references and topology dimensions are checked before any
    // SurfaceMap/normal-frame bulk reconstruction. No count-only authority.
    size_t topology_work=0;
    auto charge=[&](size_t n){if(n>8'000'000-topology_work)throw std::length_error("Retained paint topology exceeds aggregate material budget");topology_work+=n;};
    auto topology=[&](const nlohmann::json &record){
      if(!record.is_object()||record.size()!=2)throw std::invalid_argument("Invalid current-corner descriptor shape");
      const auto &t=record.at("topology");if(!t.is_object()||t.size()!=5||t.at("version")!=1||!t.at("vertices").is_array()||!t.at("edges").is_array()||!t.at("faces").is_array()||t.at("vertices").size()>500'000||t.at("faces").size()>500'000||t.at("edges").size()>2'000'000)throw std::invalid_argument("Retained paint topology shape exceeds bounds");
      charge(t.at("vertices").size());charge(t.at("edges").size());charge(t.at("faces").size());
      for(const auto &v:t.at("vertices")){if(!v.is_array()||v.size()!=5||!v[4].is_array()||v[4].size()>128)throw std::invalid_argument("Invalid retained paint vertex source provenance");charge(v[4].size());}
      for(const auto &f:t.at("faces")){if(!f.is_object()||f.size()!=3||!f.at("corners").is_array()||f.at("corners").size()<3||f.at("corners").size()>256)throw std::invalid_argument("Invalid retained paint face-corner dimensions");charge(f.at("corners").size());}
      if(!colors)throw std::invalid_argument("Retained paint requires actual shared current corner fields");index(record.at("field"),colors->fields().size());
    };
    const auto &preps=input.at("color_preparations"),&normals=input.at("normal_preparations");
    for(const auto &p:preps){if(p.contains("polygon_colors"))topology(p.at("polygon_colors"));if(p.contains("uv_reprojection"))topology(p.at("uv_reprojection").at("polygon_colors"));if(p.contains("paint_sampling")){const auto &u=p.at("paint_sampling");topology(u.at("polygon_colors"));index(u.at("paint"),input.at("paints").size());index(u.at("before_binding"),input.at("bindings").size());paint_domains->at(uint32_t(index(u.at("domain"),paint_domains->size())));}}
    for(size_t i=0;i<input.at("bindings").size();++i){const auto &b=input.at("bindings")[i];if(!b.at("color_preparation").is_null()){const auto &p=preps.at(index(b.at("color_preparation"),preps.size()));if(p.contains("paint_sampling"))index(p.at("paint_sampling").at("before_binding"),i);}
      if(!b.at("normal_preparation").is_null()){const auto &p=normals.at(index(b.at("normal_preparation"),normals.size()));if(p.contains("sampling_domain")){const auto &u=p.at("sampling_domain");index(u.at("before_binding"),i);index(u.at("sampling_binding"),i);index(u.at("paint"),input.at("paints").size());if(u.contains("metadata_sampling_domain"))paint_domains->at(uint32_t(index(u.at("metadata_sampling_domain"),paint_domains->size())));}}}
  }
  if(native_paint){
    size_t bytes=0;
    for(const auto &n:input.at("native_paints")){
      if(!n.is_object()||n.size()!=7||n.at("version")!=1||!n.at("depth").is_number_integer()||n.at("depth").get<int64_t>()<1||n.at("depth").get<int64_t>()>32)throw std::invalid_argument("Invalid native paint owning record");
      index(n.at("paint"),input.at("paints").size());paint_domains->at(uint32_t(index(n.at("domain"),paint_domains->size())));
      if(!n.at("before_binding").is_null())index(n.at("before_binding"),input.at("bindings").size());
      const auto size=SourceNormalUvLayout::encoded_storage_bytes(n.at("layout"));if(size>128*1024*1024-bytes)throw std::length_error("Native paint layout archive exceeds budget");bytes+=size;
      detail::decode_native_material(n.at("native_material"));
    }
    for(size_t i=0;i<input.at("bindings").size();++i){const auto &b=input.at("bindings")[i];if(!b.contains("native_paint_sampling"))throw std::invalid_argument("Native paint capability lacks binding references");if(!b.at("native_paint_sampling").is_null()){const auto &n=input.at("native_paints").at(index(b.at("native_paint_sampling"),input.at("native_paints").size()));if(!n.at("before_binding").is_null())index(n.at("before_binding"),i);}}
  }
  NormalFrameArchive frames;std::optional<detail::KeyFrameArchive> key_frames;if(extended){if(keyed)key_frames.emplace(input.at("normal_frames"));else frames=NormalFrameArchive::decode(input.at("normal_frames"));}
  auto frame_at=[&](const nlohmann::json &ref){return keyed?key_frames->at(uint32_t(index(ref,key_frames->size()))):frames.at(uint32_t(index(ref,frames.sets().size())));};
  // Preflight all implicit SurfaceMap allocations before constructing any.
  size_t texels=0,vertices=0,tile_bytes=0,metadata=0;
  std::function<void(const nlohmann::json&)> preflight_paint=[&](const nlohmann::json &record) {
    const auto &paint=detail::paint_archive_payload(record,native_paint||detail::material_archive_base_version(input)==9||(keyed&&keyed_paint),context);
    if(record.contains("version")&&record.at("version")==4){
      tile_bytes+=record.at("vertices").get_binary().size();preflight_paint(record.at("before"));
    }
    if(paint.contains("source_maps"))tile_bytes+=detail::source_uv_archive_measure(paint.at("source_maps")).bytes;
    if(record.contains("version")&&(record.at("version")==2||record.at("version")==3)&&!record.at("domain").is_null())paint_domains->at(uint32_t(index(record.at("domain"),paint_domains->size())));
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
      if(layer.contains("pbr_color_tiles")) {
        if(!layer.at("pbr_color_tiles").is_array())throw std::runtime_error("Invalid original PBR color tiles");
        tile_bytes+=layer.at("pbr_color_tiles").size()*sizeof(PaintColorTile);
      }
      if(layer.contains("physical_emission_tiles")){
        const auto &tiles=layer.at("physical_emission_tiles");
        if(!physical||!detail::physical_paint_authority(record,context)||!tiles.is_array()||tiles.empty()||tiles.size()>4096)
          throw std::invalid_argument("Physical paint requires actual material11/Paint3 capability");
        tile_bytes+=tiles.size()*sizeof(PaintEmissionTile);
      }
    }
    if(paint.contains("baked"))for(const auto *key:{"color","orm","emission","normal","coverage"}) {
      const auto &bytes=paint.at("baked").at(key);
      if(!bytes.is_binary())throw std::runtime_error("Invalid original UV baked buffer");
      tile_bytes+=bytes.get_binary().size();
    }
    if(paint.contains("baked") && paint.at("baked").contains("pbr_color")) {
      const auto &bytes=paint.at("baked").at("pbr_color");
      if(!bytes.is_binary())throw std::runtime_error("Invalid original baked PBR buffer");
      tile_bytes+=bytes.get_binary().size();
    }
    if(paint.contains("baked")&&paint.at("baked").contains("mtoon_normal")){
      const auto &bytes=paint.at("baked").at("mtoon_normal");if(!bytes.is_binary())throw std::invalid_argument("Invalid original baked MToon normal buffer");tile_bytes+=bytes.get_binary().size();
    }
    if(texels>16*1024*1024 || vertices>8'000'000 || tile_bytes>256*1024*1024)
      throw std::length_error("Original UV source archive exceeds scene budgets");
  };
  for(const auto &record:input.at("paints"))preflight_paint(record);
  SourceMaterialArchive output;
  std::vector<std::shared_ptr<const PaintedMesh>> paints(input.at("paints").size());
  std::vector<uint8_t> decoding_paint(paints.size());
  std::function<std::shared_ptr<const PaintedMesh>(uint32_t)> decode_paint=[&](uint32_t id){
    if(id>=paints.size())throw std::invalid_argument("Original layered paint identity exceeds material pool");
    if(paints[id])return paints[id];
    if(decoding_paint[id])throw std::invalid_argument("Cyclic original normal/paint ownership");
    decoding_paint[id]=1;paints[id]=PaintedMesh::decode(input.at("paints")[id],&images,false,domains,paint_domains,context);if(native_paint)paints[id]=std::make_shared<const PaintedMesh>(*paints[id]);decoding_paint[id]=0;return paints[id];
  };
  if(layered)detail::BakedNormalArchive::reader().paint_resolver(decode_paint);
  for(size_t i=0;i<paints.size();++i)decode_paint(uint32_t(i));
  std::vector<std::shared_ptr<const PaintSamplingAttachment>> bound_paints(paints.size());
  auto bind_paint=[&](size_t id,const std::shared_ptr<const SourceMaterialBindings> &before){
    if(!paints[id]->paint_sampling_domain())return paints[id];
    const auto attachment=detail::material_paint_attachment(before.get());
    if(!attachment)throw std::invalid_argument("Retained beforePaint lacks its own complete material witness");
    if(bound_paints[id]&&bound_paints[id]!=attachment)throw std::invalid_argument("One retained paint record cannot acquire conflicting geometry/material owners");
    if(!bound_paints[id]){auto rebound=rebind_paint_sampling_geometry(*paints[id],attachment);paints[id]=native_paint?std::make_shared<const PaintedMesh>(*rebound):std::move(rebound);bound_paints[id]=attachment;}
    return paints[id];
  };
  for(const auto &value:input.at("origins")) {
    auto origin=std::make_shared<SourceUvOrigin>();
    origin->paint=paints.at(index(value.at("paint"),paints.size()));
    if(!value.at("appearance").is_null())origin->appearance=decode_creator_appearance(value.at("appearance"),models);
    if(!value.at("material").is_null())origin->material=MToonMaterial::decode(value.at("material"));
    metadata+=metadata_bytes(origin->material);
    if(metadata>16*1024*1024)throw std::length_error("Scene resolved material metadata exceeds budget");
    origin->roles=decode_roles(value.at("roles"),images);
    if(extended){if(!value.at("deformation").is_null())origin->deformation=std::make_shared<const DeformationState>(DeformationState::decode(value.at("deformation"),origin->paint->mesh()));
      if(!value.at("creator").is_null()){const auto &c=value.at("creator");origin->creator_kind=c.at("kind").get<std::string>();if(!c.is_object() || c.size()!=3 || !c.at("version").is_number_integer() || c.at("version")!=1)throw std::runtime_error("Unsupported creator frame version");origin->creator_version=1;origin->creator_parameters=c.at("parameters").get<std::string>();}}
    output.origins_.push_back(std::move(origin));
  }
  std::vector<std::shared_ptr<const SourceColorPreparation>> preparations;size_t color_uv_layout_bytes=0;
  if(prepared)preparations.resize(input.at("color_preparations").size());
  std::vector<std::shared_ptr<const SourceNormalPreparation>> normal_preparations;
  if(normal_prepared)normal_preparations.resize(input.at("normal_preparations").size());
  std::vector<std::shared_ptr<const detail::CanonicalNormalConversion>> canonical_conversions;
  if(detail::canonical_material_archive(input)){
    canonical_conversions.resize(input.at("canonical_conversions").size());
    for(const auto &record:input.at("canonical_conversions")){
      if(!record.is_object()||record.size()!=3||record.at("version")!=1||!record.at("receipt").is_object())
        throw std::invalid_argument("Unsupported canonical conversion owning anchor");
      index(record.at("anchor"),normal_preparations.size());
    }
  }
  std::vector<std::shared_ptr<const detail::SourceKeyDerivation>> key_derivations;if(keyed)key_derivations.resize(input.at("key_derivations").size());
  std::vector<std::shared_ptr<const detail::NativePaintSampling>> native_paints;if(native_paint)native_paints.resize(input.at("native_paints").size());
  for(const auto &value:input.at("bindings")) {
    const auto &frame=value.at("normal_frame");
    if(!frame.is_string() || (frame.get_ref<const std::string&>()!="uv-derivative" && (!extended || frame.get_ref<const std::string&>()!="original-attributes")))throw std::runtime_error("Unsupported derived material normal frame");
    auto binding=std::make_shared<SourceMaterialBindings>();
    if(value.contains("physical_emission")){if(!physical)throw std::invalid_argument("Absolute background emission requires material11");binding->physical_emission=value.at("physical_emission").get<std::array<float,3>>();}
    if(frame.get_ref<const std::string&>()=="original-attributes")binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;
    if(extended && !value.at("frames").is_null()&&!(keyed&&!value.at("key_derivation").is_null()))binding->normal_frames=frame_at(value.at("frames"));
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
    binding->sampling_reparameterized=extended?value.at("sampling_reparameterized").get<bool>():bool(binding->uv_origin);
    if(layered&&!value.at("canonical_attributes").is_null()){
      const auto &reference=value.at("canonical_attributes");
      if((!reference.is_number_unsigned()&&(!reference.is_number_integer()||reference.get<int64_t>()<0))||reference.get<uint64_t>()>UINT32_MAX)throw std::invalid_argument("Invalid canonical attribute origin reference");
      binding->canonical_attributes=detail::BakedNormalArchive::reader().origin_at(reference.get<uint32_t>());
      if(!(keyed&&!value.at("key_derivation").is_null())){
        NormalFrameArchive a,b;a.add(binding->normal_frames);b.add(binding->canonical_attributes->frames());
        if(a.encode()!=b.encode())throw std::invalid_argument("Canonical attribute frames differ from actual source replay");
        binding->normal_frames=binding->canonical_attributes->frames();
      }
    }
    if(canonical_uv&&!value.at("canonical_uv").is_null()){
      const auto &reference=value.at("canonical_uv");
      if((!reference.is_number_unsigned()&&(!reference.is_number_integer()||reference.get<int64_t>()<0))||reference.get<uint64_t>()>UINT32_MAX)
        throw std::invalid_argument("Invalid canonical UV pool identity");
      binding->canonical_uv=detail::BakedNormalArchive::reader().uv_at(reference.get<uint32_t>());
    }
    if(keyed&&!value.at("key_derivation").is_null()){const auto id=index(value.at("key_derivation"),key_derivations.size());const auto &record=input.at("key_derivations").at(id);if(record.at("frames")!=value.at("frames"))throw std::invalid_argument("Material key frame reference differs from its owning node");if(!key_derivations[id])key_derivations[id]=detail::KeyDerivationCodec::decode(record,uint32_t(output.bindings_.size()),output.bindings_,[&](uint32_t p,const auto &b){if(p>=paints.size())throw std::out_of_range("Owning key Paint reference outside pool");return bind_paint(p,b);},*key_frames,models,{},detail::explicit_mirror_material_archive(input));binding->key_derivation=key_derivations[id];binding->normal_frames=binding->key_derivation->frames();}
    if(native_paint&&!value.at("native_paint_sampling").is_null()){
      const auto id=index(value.at("native_paint_sampling"),native_paints.size());const auto &n=input.at("native_paints").at(id);
      if(!native_paints[id]){
        const auto before=n.at("before_binding").is_null()?std::shared_ptr<const SourceMaterialBindings>{}:output.bindings_.at(index(n.at("before_binding"),output.bindings_.size()));
        const auto prior=bind_paint(index(n.at("paint"),paints.size()),before);
        auto replay=detail::NativePaintSamplingCodec::replay(prior,detail::decode_native_material(n.at("native_material")),before,SourceNormalUvLayout::decode(n.at("layout")),paint_domains->at(uint32_t(index(n.at("domain"),paint_domains->size()))));
        if(replay.owner->history_depth()!=n.at("depth").get<uint32_t>())throw std::invalid_argument("Native paint depth differs from owning replay");native_paints[id]=std::move(replay.owner);
      }
      binding->native_paint_sampling=native_paints[id];
    }
    if(prepared && !value.at("color_preparation").is_null()) {
      const auto id=index(value.at("color_preparation"),preparations.size());
      if(!preparations[id]) {
        const auto &v=input.at("color_preparations").at(id);
        const bool current=v.contains("polygon_colors"),uv=v.contains("uv_reprojection"),retained=v.contains("paint_sampling");
        if(!v.is_object() || v.size()!=(retained?(uv?11u:10u):uv?10u:current?9u:8u) || !v.at("version").is_number_integer() || v.at("version")!=(retained?4:uv?3:current?2:1) || (current&&!corners) || (uv&&(!current||(detail::material_archive_base_version(input)!=8&&detail::material_archive_base_version(input)!=9 && detail::material_archive_base_version(input)!=10))) || (retained&&(!current||detail::material_archive_base_version(input)!=9 && detail::material_archive_base_version(input)!=10)))throw std::runtime_error("Unsupported retained color preparation");
        auto p=std::make_shared<SourceColorPreparation>();
        p->projection=decode_color_projection(v.at("projection"),images);p->initial_projection=decode_color_projection(v.at("initial_projection"),images);
        p->receipt=ColorPreparationReceipt::decode(v.at("receipt"));p->original_paint=paints.at(index(v.at("paint"),paints.size()));
        if(current){if(!colors)throw std::invalid_argument("Current corner color preparation requires its shared field pool");const auto &record=v.at("polygon_colors");if(!record.is_object()||record.size()!=2)throw std::invalid_argument("Invalid corner color projection source");auto source=std::make_shared<PolygonColorSurface>();source->topology=std::make_shared<const PolygonMesh>(PolygonMesh::decode(record.at("topology")));source->field=colors->at(uint32_t(index(record.at("field"),colors->fields().size())));p->polygon_colors=std::move(source);}
        if(!v.at("appearance").is_null())p->original_appearance=decode_creator_appearance(v.at("appearance"),models);
        if(!v.at("deformation").is_null())p->original_deformation=std::make_shared<const DeformationState>(DeformationState::decode(v.at("deformation"),p->original_paint->mesh()));
        if(!v.at("before_binding").is_null())p->original_material=output.bindings_.at(index(v.at("before_binding"),output.bindings_.size()));
        if(uv&&!retained){const auto &u=v.at("uv_reprojection");auto node=std::make_shared<SourceColorUvReprojection>();
          node->receipt=ColorUvReprojectionReceipt::decode(u.at("receipt"));node->layout=SourceColorUvLayout::decode(u.at("layout"));node->projection=p->projection;
          node->before_paint=paints.at(index(u.at("paint"),paints.size()));node->before_material=output.bindings_.at(index(u.at("before_binding"),output.bindings_.size()));
          if(!u.at("appearance").is_null())node->before_appearance=decode_creator_appearance(u.at("appearance"),models);
          if(!u.at("deformation").is_null())node->before_deformation=std::make_shared<const DeformationState>(DeformationState::decode(u.at("deformation"),node->before_paint->mesh()));
          const auto &source=u.at("polygon_colors");if(!source.is_object()||source.size()!=2)throw std::invalid_argument("Invalid current colour UV descriptor");
          auto descriptor=std::make_shared<PolygonColorSurface>();descriptor->topology=std::make_shared<const PolygonMesh>(PolygonMesh::decode(source.at("topology")));descriptor->field=colors->at(uint32_t(index(source.at("field"),colors->fields().size())));node->polygon_colors=std::move(descriptor);
          const auto prior=node->before_material->color_preparation;if(!prior)throw std::invalid_argument("Colour UV predecessor lost its own preparation");
          const auto previous=std::find(preparations.begin(),preparations.end(),prior);if(previous==preparations.end())throw std::invalid_argument("Colour UV predecessor is not an earlier archived recipe");
          const auto &record=input.at("color_preparations").at(size_t(previous-preparations.begin()));
          for(const char *key:{"initial_projection","receipt","paint","appearance","deformation","before_binding","polygon_colors"})
            if(v.at(key)!=record.at(key))throw std::invalid_argument("Colour UV initial serialized authority differs from its immutable predecessor");
          p->initial_projection=prior->initial_projection;p->receipt=prior->receipt;p->original_paint=prior->original_paint;p->original_appearance=prior->original_appearance;
          p->original_deformation=prior->original_deformation;p->original_material=prior->original_material;p->polygon_colors=prior->polygon_colors;p->uv_reprojection=std::move(node);
        }
        if(retained){const auto &u=v.at("paint_sampling");
          const auto before=output.bindings_.at(index(u.at("before_binding"),output.bindings_.size()));const auto prior=before->color_preparation;
          if(!prior)throw std::invalid_argument("Retained paint predecessor lacks its own colour preparation");
          auto previous=std::find(preparations.begin(),preparations.end(),prior);if(previous==preparations.end())throw std::invalid_argument("Retained colour predecessor is not an earlier recipe");
          const auto &record=input.at("color_preparations").at(size_t(previous-preparations.begin()));
          for(const char *key:{"projection","initial_projection","receipt","paint","appearance","deformation","before_binding","polygon_colors"})
            if(v.at(key)!=record.at(key))throw std::invalid_argument("Retained colour changed its immutable predecessor authority");
          if(v.contains("uv_reprojection")!=record.contains("uv_reprojection")||(v.contains("uv_reprojection")&&v.at("uv_reprojection")!=record.at("uv_reprojection")))throw std::invalid_argument("Retained paint changed old UV provenance");
          const auto paint_id=index(u.at("paint"),paints.size());auto paint=bind_paint(paint_id,before);
          if(!u.at("appearance").is_null())throw std::invalid_argument("Current corner retained paint cannot borrow raw source appearance");
          auto layout=SourceColorUvLayout::decode(u.at("layout"));std::shared_ptr<const DeformationState> deform;
          if(!u.at("deformation").is_null())deform=std::make_shared<const DeformationState>(DeformationState::decode(u.at("deformation"),paint->mesh()));
          const auto &d=u.at("polygon_colors");if(!d.is_object()||d.size()!=2)throw std::invalid_argument("Invalid retained current colour descriptor");
          auto descriptor=std::make_shared<const PolygonColorSurface>(PolygonColorSurface{std::make_shared<const PolygonMesh>(PolygonMesh::decode(d.at("topology"))),colors->at(uint32_t(index(d.at("field"),colors->fields().size())))});
          const auto domain=paint_domains->at(uint32_t(index(u.at("domain"),paint_domains->size())));auto target=layout->target_mesh(paint->mesh());
          auto replay=retain_source_corner_paint(paint,before,deform,std::move(descriptor),target,layout->source_vertices,domain);
          if(replay.sampling->receipt().encode()!=u.at("receipt"))throw std::invalid_argument("Retained colour receipt differs from actual owning replay");
          p=std::make_shared<SourceColorPreparation>(*replay.colors);
        }
        p->validate();preparations[id]=std::move(p);
      }
      binding->color_preparation=preparations[id];
    }
    if(normal_prepared&&!value.at("normal_preparation").is_null()){
      const auto id=index(value.at("normal_preparation"),normal_preparations.size());
      if(!normal_preparations[id]){
        const auto &v=input.at("normal_preparations").at(id);
        const bool uv=v.contains("uv_reprojection"),domain=v.contains("sampling_domain"),key=v.contains("key_derivation");
        const auto &entry_version=detail::normal_preparation_base_version(v,input);
        if((uv&&domain)||!v.is_object()||v.size()!=size_t(((uv||domain)?10:9)+(key?1:0)+(v.contains("canonical_conversion")?2:0))||
           entry_version!=(key?(binding->key_derivation&&binding->key_derivation->uses_explicit_mirror()?6:5):domain?(entry_version==4?4:3):uv?2:1)||(key&&!keyed)||(uv&&!normal_uv)||(domain&&!sampling))
          throw std::invalid_argument("Unsupported retained normal preparation");
        auto p=std::make_shared<SourceNormalPreparation>();p->receipt=NormalPreparationReceipt::decode(v.at("receipt"));
        p->projection=decode_normal_projection(v.at("projection"),images,domains);p->initial_projection=decode_normal_projection(v.at("initial_projection"),images,domains);
        p->target_frames=frame_at(v.at("frames"));p->original_paint=paints.at(index(v.at("paint"),paints.size()));
        if(!v.at("appearance").is_null())p->original_appearance=decode_creator_appearance(v.at("appearance"),models);
        if(!v.at("deformation").is_null())p->original_deformation=std::make_shared<const DeformationState>(DeformationState::decode(v.at("deformation"),p->original_paint->mesh()));
        if(!v.at("before_binding").is_null())p->original_material=output.bindings_.at(index(v.at("before_binding"),output.bindings_.size()));
        if(uv){const auto &u=v.at("uv_reprojection");auto projection=std::make_shared<SourceNormalUvReprojection>();projection->receipt=NormalUvReprojectionReceipt::decode(u.at("receipt"));projection->layout=SourceNormalUvLayout::decode(u.at("layout"));projection->projection=decode_normal_projection(u.at("projection"),images,domains);projection->before_paint=paints.at(index(u.at("paint"),paints.size()));projection->before_material=output.bindings_.at(index(u.at("before_binding"),output.bindings_.size()));if(!u.at("appearance").is_null())projection->before_appearance=decode_creator_appearance(u.at("appearance"),models);if(!u.at("deformation").is_null())projection->before_deformation=std::make_shared<const DeformationState>(DeformationState::decode(u.at("deformation"),projection->before_paint->mesh()));
          const auto prior=projection->before_material->normal_preparation;if(!prior)throw std::invalid_argument("Normal UV prior binding lost its own normal interpretation");const auto found=std::find(normal_preparations.begin(),normal_preparations.end(),prior);if(found==normal_preparations.end())throw std::invalid_argument("Normal UV prior recipe is not an earlier archived authority");const auto &record=input.at("normal_preparations").at(size_t(found-normal_preparations.begin()));if(v.at("appearance")!=record.at("appearance")||v.at("deformation")!=record.at("deformation"))throw std::invalid_argument("Normal UV original serialized authority differs from its immutable prior recipe");p->original_appearance=prior->original_appearance;p->original_deformation=prior->original_deformation;p->uv_reprojection=std::move(projection);}
        if(domain){const auto &u=v.at("sampling_domain");
          auto proof=std::make_shared<SourceNormalSampling>();if(u.contains("metadata_sampling_domain")){if((detail::material_archive_base_version(input)!=9&&detail::material_archive_base_version(input)!=10)||!paint_domains)throw std::invalid_argument("Normal metadata entry4 requires owning material9");proof->metadata_sampling_domain=paint_domains->at(uint32_t(index(u.at("metadata_sampling_domain"),paint_domains->size())));}
          proof->receipt=NormalSamplingReceipt::decode(u.at("receipt"));
          proof->layout=SourceNormalUvLayout::decode(u.at("layout"));proof->projection=decode_normal_projection(u.at("projection"),images,domains);
          const auto prior_paint_id=index(u.at("paint"),paints.size());proof->before_paint=paints.at(prior_paint_id);
          proof->before_material=output.bindings_.at(index(u.at("before_binding"),output.bindings_.size()));
          proof->sampling_material=output.bindings_.at(index(u.at("sampling_binding"),output.bindings_.size()));
          proof->before_paint=bind_paint(prior_paint_id,proof->before_material);
          if(!u.at("appearance").is_null())proof->before_appearance=decode_creator_appearance(u.at("appearance"),models);
          if(!u.at("deformation").is_null())proof->before_deformation=std::make_shared<const DeformationState>(DeformationState::decode(u.at("deformation"),proof->before_paint->mesh()));
          if(!u.at("current_sampling_reference").is_null())proof->current_sampling_reference=MToonMaterial::decode(u.at("current_sampling_reference"));
          metadata+=metadata_bytes(proof->current_sampling_reference);
          if(metadata>16*1024*1024)throw std::length_error("Retained normal sampling metadata exceeds budget");
          const auto prior=proof->before_material->normal_preparation;
          if(!prior)throw std::invalid_argument("Retained normal source binding lost its own preparation");
          const auto found=std::find(normal_preparations.begin(),normal_preparations.end(),prior);
          if(found==normal_preparations.end())throw std::invalid_argument("Retained normal source recipe is not an earlier authority");
          const auto &record=input.at("normal_preparations").at(size_t(found-normal_preparations.begin()));
          if(v.at("appearance")!=record.at("appearance")||v.at("deformation")!=record.at("deformation"))
            throw std::invalid_argument("Retained normal original serialized authority differs from its immutable source");
          p->original_appearance=prior->original_appearance;p->original_deformation=prior->original_deformation;
          if(proof->metadata_sampling_domain)detail::authenticate_normal_sampling_metadata(proof);
          p->sampling_domain=std::move(proof);
        }
        if(key){const auto key_id=index(v.at("key_derivation"),key_derivations.size());if(!key_derivations[key_id]||key_derivations[key_id]!=binding->key_derivation)throw std::invalid_argument("Normal key entry5 lacks its actual material owning node");p->key_derivation=key_derivations[key_id];if(p->target_frames!=p->key_derivation->frames())throw std::invalid_argument("Normal key entry5 borrows another target frame owner");
          const auto anchor=p->key_derivation->before_material()->normal_preparation;if(!anchor)throw std::invalid_argument("Paired normal key entry5 needs an actual prepared anchor");
          const auto found=std::find(normal_preparations.begin(),normal_preparations.end(),anchor);if(found==normal_preparations.end())throw std::invalid_argument("Paired key anchor is not an earlier complete normal recipe");const auto &original=input.at("normal_preparations").at(size_t(found-normal_preparations.begin()));
          bool unchanged=true;for(const char *field:{"projection","initial_projection","receipt","paint","appearance","deformation","before_binding","uv_reprojection","sampling_domain","canonical_conversion"})if(v.contains(field)!=original.contains(field)||(v.contains(field)&&v.at(field)!=original.at(field)))unchanged=false;
          if(unchanged){p=std::make_shared<SourceNormalPreparation>(*anchor);p->target_frames=binding->key_derivation->frames();p->key_derivation=binding->key_derivation;}
        }
        if(v.contains("canonical_conversion")){
          const auto cid=index(v.at("canonical_conversion"),canonical_conversions.size());
          const auto &record=input.at("canonical_conversions").at(cid);
          if(!canonical_conversions[cid]){
            if(index(record.at("anchor"),normal_preparations.size())!=id||uv||domain||key)
              throw std::invalid_argument("Canonical conversion descendants need their earlier genuine root anchor");
            canonical_conversions[cid]=detail::CanonicalNormalConversion::restore(record.at("receipt"),
              {p->original_paint,p->original_appearance,p->original_material,p->original_deformation,{},{}},p->target_frames,p->initial_projection);
          }
          const auto &owner=canonical_conversions[cid];
          const auto deformation=owner->before_deformation()?owner->before_deformation()->encode(owner->before_paint()->mesh()):nlohmann::json(nullptr);
          if(p->original_paint!=owner->before_paint()||p->original_material!=owner->before_material()||p->original_appearance||v.at("deformation")!=deformation)
            throw std::invalid_argument("Canonical conversion recipe borrowed a different serialized input");
          p->original_deformation=owner->before_deformation();p->canonical_conversion=owner;
        }
        p->validate();normal_preparations[id]=std::move(p);
      }
      binding->normal_preparation=normal_preparations[id];
    }
    binding->validate();if(native_paint)output.bindings_.push_back(std::make_shared<const SourceMaterialBindings>(*binding));else output.bindings_.push_back(std::move(binding));
  }
  if(detail::material_archive_base_version(input)==9&&!std::any_of(preparations.begin(),preparations.end(),[](const auto &p){return p&&p->paint_sampling;}))throw std::invalid_argument("Material9 requires actual retained current-colour paint authority");
  if(std::any_of(preparations.begin(),preparations.end(),[](const auto &p){return !p;}))throw std::runtime_error("Unreferenced color preparation record");
  if(detail::material_archive_base_version(input)==6&&!std::any_of(preparations.begin(),preparations.end(),[](const auto &p){return p&&p->polygon_colors;}))throw std::invalid_argument("Material archive6 requires current corner color authority");
  if(detail::material_archive_base_version(input)==7&&!std::any_of(normal_preparations.begin(),normal_preparations.end(),[](const auto &p){return p&&p->sampling_domain;}))
    throw std::invalid_argument("Material archive7 requires its actual retained normal feature");
  if(std::any_of(normal_preparations.begin(),normal_preparations.end(),[](const auto &p){return !p;}))throw std::invalid_argument("Unreferenced normal preparation record");
  if(std::any_of(canonical_conversions.begin(),canonical_conversions.end(),[](const auto &p){return !p;}))throw std::invalid_argument("Unreferenced canonical conversion lease");
  if(physical||detail::explicit_mirror_material_archive(input)||detail::canonical_material_archive(input)||layered||native_paint){
    if(native_paint&&input.at("explicit_mirror").get<bool>()!=std::any_of(output.bindings_.begin(),output.bindings_.end(),[](const auto &b){return b->key_derivation&&b->key_derivation->uses_explicit_mirror();}))throw std::invalid_argument("Native paint explicit-Mirror capability differs from actual graph");
    SourceMaterialArchive reachable;
    for(const auto &binding:output.bindings_)reachable.add(binding,false);
    if(reachable.origins_.size()!=output.origins_.size())throw std::invalid_argument("HDR archive has unreferenced source origins");
    const auto owners=material_paint_owners(reachable.bindings_,reachable.origins_);
    if(owners.size()!=paints.size())throw std::invalid_argument("HDR archive has unreferenced paint owners");
    if(physical_material_state(owners,reachable.bindings_)!=physical)throw std::invalid_argument("Material physical capability differs from actual reachable emission state");
    if(material_shape_version(reachable.bindings_,reachable.origins_)!=detail::material_archive_base_version(input).get<unsigned>())
      throw std::invalid_argument("HDR material base layout differs from its actual owning features");
  }
  if(keyed){if(std::any_of(key_derivations.begin(),key_derivations.end(),[](const auto &k){return !k;}))throw std::invalid_argument("Unreferenced material10 key record");key_frames->require_complete();}
  if(canonical_uv&&!std::any_of(output.bindings_.begin(),output.bindings_.end(),[](const auto &b){return bool(b->canonical_uv);}))
    throw std::invalid_argument("Material16 requires actual retained canonical UV correspondence");
  if(layered){
    if(!std::any_of(output.bindings_.begin(),output.bindings_.end(),[](const auto &b){return bool(b->canonical_attributes);}))throw std::invalid_argument("Material15 requires genuine canonical attribute provenance");
    detail::BakedNormalArchive::reader().paint_resolver({});
  }
  return output;
}
} // namespace dome
