// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/normal-preparation.hpp"
#include "dome/humanoid-creator.hpp"
#include "normal-bake-frames.hpp"
#include "normal-frame-export.hpp"
#include "paint-normal-access.hpp"
#include "source-paint-pairing.hpp"
#include "normal-preparation-cache.hpp"
#include <mutex>
namespace dome {
namespace {
uint64_t integer(const nlohmann::json &j,uint64_t cap){
  if((!j.is_number_unsigned()&&(!j.is_number_integer()||j.get<int64_t>()<0))||j.get<uint64_t>()>cap)
    throw std::invalid_argument("Normal preparation integer is outside its declared range");
  return j.get<uint64_t>();
}
bool vector_equal(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
template<class T>struct WeakOwner {
  std::weak_ptr<const T> owner;bool present=false;
  explicit WeakOwner(const std::shared_ptr<const T> &p):owner(p),present(bool(p)){}
  bool matches(const std::shared_ptr<const T> &p)const{
    return present==bool(p)&&(!present||(owner.lock()==p&&!owner.owner_before(p)&&!p.owner_before(owner)));
  }
};
struct InitialProof {
  WeakOwner<PaintedMesh> paint;
  WeakOwner<CreatorAppearance> appearance;
  WeakOwner<DeformationState> deformation;
  WeakOwner<SourceMaterialBindings> material;
  WeakOwner<NormalFrameSet> frames;
  WeakOwner<PaintNormalProjection> initial;
  nlohmann::json receipt;
};
std::mutex proof_mutex;std::deque<InitialProof> proofs;
detail::NormalPreparationCacheStats proof_stats;
}
detail::NormalPreparationCacheStats detail::normal_preparation_cache_stats(){std::lock_guard lock(proof_mutex);return proof_stats;}
void NormalPreparationReceipt::validate() const {
  if(version!=1||resolution<64||resolution>2048||(resolution&(resolution-1))||
     pbr!=PreparedPbrNormalEvaluation::SampleRenderer2026||size_t(mtoon)>2||
     !std::isfinite(accepted_angular_error)||accepted_angular_error<0||accepted_angular_error>.25||
     !std::isfinite(observed_initial_error)||observed_initial_error<0||observed_initial_error>accepted_angular_error||
     !sampled_texels||sampled_texels>size_t(resolution)*resolution)
    throw std::invalid_argument("Invalid normal preparation interpretation/quality receipt");
}
nlohmann::json NormalPreparationReceipt::encode() const {
  validate();return {{"version",version},{"resolution",resolution},{"pbr",int(pbr)},{"mtoon",int(mtoon)},
    {"authored_native_normals",authored_native_normals},{"authored_native_tangents",authored_native_tangents},
    {"accepted_angular_error",accepted_angular_error},{"observed_initial_error",observed_initial_error},{"sampled_texels",sampled_texels}};
}
NormalPreparationReceipt NormalPreparationReceipt::decode(const nlohmann::json &j){
  if(!j.is_object()||j.size()!=9||integer(j.at("version"),1)!=1)throw std::invalid_argument("Unsupported normal preparation receipt");
  NormalPreparationReceipt out;out.resolution=uint32_t(integer(j.at("resolution"),2048));out.pbr=PreparedPbrNormalEvaluation(integer(j.at("pbr"),1));out.mtoon=PreparedMToonNormalEvaluation(integer(j.at("mtoon"),2));
  out.authored_native_normals=j.at("authored_native_normals").get<bool>();out.authored_native_tangents=j.at("authored_native_tangents").get<bool>();
  out.accepted_angular_error=j.at("accepted_angular_error").get<double>();out.observed_initial_error=j.at("observed_initial_error").get<double>();out.sampled_texels=integer(j.at("sampled_texels"),4'194'304);out.validate();return out;
}
nlohmann::json encode_normal_projection(const PaintNormalProjection &p,PaintImageArchive &images){
  if(!p.pbr)throw std::invalid_argument("Missing prepared PBR normal plane");
  p.validate(p.pbr->width);
  return {{"version",1},{"pbr",images.add(p.pbr)},{"mtoon",images.add(p.mtoon)}};
}
std::shared_ptr<const PaintNormalProjection> decode_normal_projection(const nlohmann::json &j,const PaintImageArchive &images){
  if(!j.is_object()||j.size()!=3||integer(j.at("version"),1)!=1)throw std::invalid_argument("Unsupported prepared normal projection");
  auto out=std::make_shared<PaintNormalProjection>();out->pbr=images.at(uint32_t(integer(j.at("pbr"),UINT32_MAX)));out->mtoon=images.at(uint32_t(integer(j.at("mtoon"),UINT32_MAX)));out->validate(out->pbr->width);return out;
}
void SourceNormalPreparation::validate(bool deep) const {
  receipt.validate();
  if(!projection||!initial_projection||!target_frames||!original_paint||original_paint->in_stroke()||original_paint->normal_projection())
    throw std::invalid_argument("Normal preparation requires immutable unprepared original authority");
  if(original_paint->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored)
    throw std::invalid_argument("Normal preparation original paint requires its own strict conversion, not an inferred polygon witness");
  if(original_material&&original_material->normal_preparation)
    throw std::invalid_argument("Normal preparation cannot recursively retain another normal interpretation");
  if(original_material)original_material->validate(deep);
  detail::source_paint_pairing(original_paint.get(),original_material.get(),original_appearance.get(),deep);
  if(original_paint->resolution()!=receipt.resolution)throw std::invalid_argument("Normal preparation original resolution differs");
  if(original_paint->baked()&&!original_paint->baked()->maps.normal.rgba.empty())throw std::invalid_argument("Historical normal payload cannot be silently reinterpreted");
  projection->validate(receipt.resolution);initial_projection->validate(receipt.resolution);target_frames->validate(false);
  if(!detail::normal_projection_identity(*projection,*initial_projection))
    throw std::invalid_argument("Derived prepared-normal UV planes require explicit source-aware reprojection authority");
  if(!target_frames->pbr||target_frames->pbr->tangents.empty()||target_frames->canonical||
     (receipt.mtoon!=PreparedMToonNormalEvaluation::Absent&&target_frames->mtoon!=target_frames->pbr))
    throw std::invalid_argument("Prepared normal target has no explicit compatible complete attribute model");
  if(original_appearance&&original_appearance->source_vertices.size()!=original_paint->mesh().positions.size())throw std::invalid_argument("Normal preparation original appearance remap differs");
  if(!deep)return;
  const auto receipt_data=receipt.encode();
  {
    std::lock_guard lock(proof_mutex);
    for(const auto &proof:proofs)if(proof.paint.matches(original_paint)&&proof.appearance.matches(original_appearance)&&proof.deformation.matches(original_deformation)&&proof.material.matches(original_material)&&proof.frames.matches(target_frames)&&proof.initial.matches(initial_projection)&&proof.receipt==receipt_data){++proof_stats.hits;return;}
  }
  if(original_material&&original_material->has_mtoon){const auto &b=*original_material;
    if((!b.normal_frames&&b.material.parameters.normal_enabled&&b.roles[size_t(MToonSlot::Normal)].image)||
       (b.normal_frames&&(!b.normal_frames->pbr||!b.normal_frames->mtoon||!detail::normal_frame_attribute_equal(*b.normal_frames->pbr,*b.normal_frames->mtoon,true))))
      throw std::invalid_argument("Retained normal conversion has no qualified shared PBR/MToon source attribute model");
  }
  if(original_deformation)original_deformation->validate(original_paint->mesh());
  detail::NormalBakeFrameOptions frame_options;
  auto expected=detail::normal_bake_frame_candidate(original_paint->mesh(),original_deformation.get(),original_material.get(),frame_options);
  NormalFrameArchive a,b;a.add(expected.frames);b.add(target_frames);
  if(a.encode()!=b.encode()||expected.authored_native_normals!=receipt.authored_native_normals||expected.authored_native_tangents!=receipt.authored_native_tangents)
    throw std::invalid_argument("Prepared normal target attributes differ from their retained native key authority");
  NormalPreparationOptions options;options.max_angular_error=receipt.accepted_angular_error;
  auto initial=SourceNormalPreparationAccess::prepare(*original_paint,original_material.get(),options);
  const auto &actual=*initial.paint->normal_projection();
  if(actual.pbr->rgba!=initial_projection->pbr->rgba||actual.mtoon->rgba!=initial_projection->mtoon->rgba||
     initial.receipt.observed_initial_error!=receipt.observed_initial_error||initial.receipt.sampled_texels!=receipt.sampled_texels||initial.receipt.mtoon!=receipt.mtoon)
    throw std::invalid_argument("Initial normal planes/receipt differ from retained original samples");
  // This is a weak immutable-owner cache, not a mesh/pixel hash. Pose/stereo
  // validation reuses proven original inputs instead of rerasterizing pixels.
  try {std::lock_guard lock(proof_mutex);++proof_stats.builds;
    if(proofs.size()>=64)proofs.pop_front();
    proofs.push_back({WeakOwner(original_paint),WeakOwner(original_appearance),WeakOwner(original_deformation),WeakOwner(original_material),WeakOwner(target_frames),WeakOwner(initial_projection),receipt_data});
  }catch(const std::bad_alloc&){/* Best-effort cache failure cannot fail a proven detached conversion. */}
}
void SourceNormalPreparation::validate_current(const Mesh &mesh,const CreatorAppearance *appearance,
    const std::function<bool()> &cancelled) const {
  const auto &original=original_paint->mesh();mesh.validate();
  if(mesh.indices.size()!=original.indices.size())throw std::invalid_argument("Prepared normal corner authority changed");
  if(bool(appearance)!=bool(original_appearance))throw std::invalid_argument("Prepared normal appearance authority changed");
  if(appearance&&(appearance->asset!=original_appearance->asset||appearance->source_mesh!=original_appearance->source_mesh||appearance->source_primitive!=original_appearance->source_primitive||appearance->source_vertices.size()!=mesh.positions.size()))
    throw std::invalid_argument("Prepared normal original primitive authority changed");
  for(size_t c=0;c<mesh.indices.size();++c){if((c&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Normal preparation authority validation cancelled");
    const auto a=mesh.indices[c],b=original.indices[c];
    if(!vector_equal(mesh.positions[a],original.positions[b])||(appearance&&appearance->source_vertices[a]!=original_appearance->source_vertices[b]))
      throw std::invalid_argument("Prepared normal native rest/source corner identity changed");
    if(mesh.uv[a]!=original.uv[b])
      throw std::invalid_argument("Prepared normal sampling UV changed without explicit source-aware reprojection authority");
  }
}
NormalPreparationResult prepare_source_normals(const ColorPreparationInput &input,const NormalPreparationOptions &options){
  auto check=[&]{if(options.cancelled&&options.cancelled())throw std::runtime_error("Normal preparation cancelled");};check();
  if(!std::isfinite(options.max_angular_error)||options.max_angular_error<0||options.max_angular_error>.25||!options.max_samples||options.max_samples>64'000'000||!options.max_working_bytes||options.max_working_bytes>512*1024*1024)
    throw std::invalid_argument("Invalid explicit normal preparation quality/resource policy");
  if(!input.paint||input.paint->in_stroke())throw std::invalid_argument("Normal preparation requires a finished immutable paint input");
  if(input.material_bindings)input.material_bindings->validate();
  if(input.paint->normal_projection()){
    if(!input.material_bindings||!input.material_bindings->normal_preparation)throw std::invalid_argument("Prepared normal planes lost their owning recipe");
    input.material_bindings->normal_preparation->validate_current(input.paint->mesh(),input.appearance.get(),options.cancelled);
    return {input.paint->snapshot(),input.material_bindings,false};
  }
  if(input.appearance&&(!input.material_bindings||!input.material_bindings->normal_frames))
    throw std::invalid_argument("Imported normal preparation requires retained source attribute identities");
  if(input.paint->source_maps()&&input.paint->source_maps()->normal.image&&(!input.material_bindings||!input.material_bindings->normal_frames||!input.material_bindings->normal_frames->pbr||input.material_bindings->normal_frames->pbr->tangents.empty()))
    throw std::invalid_argument("Original normal mapped material needs a proven explicit source frame before preparation");
  if(input.material_bindings&&input.material_bindings->has_mtoon){const auto &b=*input.material_bindings;
    if(!b.normal_frames&&b.material.parameters.normal_enabled&&b.roles[size_t(MToonSlot::Normal)].image)
      throw std::invalid_argument("Original MToon normal map requires retained source attributes before preparation");
    if(b.normal_frames&&(!b.normal_frames->pbr||!b.normal_frames->mtoon||!detail::normal_frame_attribute_equal(*b.normal_frames->pbr,*b.normal_frames->mtoon,true,options.cancelled)))
      throw std::invalid_argument("Different original PBR/MToon attributes require pose-qualified explicit conversion before sharing a standard normal target");
  }
  detail::NormalBakeFrameOptions frame_options;frame_options.cancelled=options.cancelled;
  auto candidate=detail::normal_bake_frame_candidate(input.paint->mesh(),input.deformation.get(),input.material_bindings.get(),frame_options);
  auto prepared=SourceNormalPreparationAccess::prepare(*input.paint,input.material_bindings.get(),options);
  prepared.receipt.authored_native_normals=candidate.authored_native_normals;prepared.receipt.authored_native_tangents=candidate.authored_native_tangents;
  auto recipe=std::make_shared<SourceNormalPreparation>();recipe->receipt=prepared.receipt;recipe->projection=recipe->initial_projection=prepared.paint->normal_projection();recipe->target_frames=candidate.frames;
  recipe->original_paint=input.paint->snapshot();recipe->original_appearance=input.appearance;recipe->original_deformation=input.deformation;recipe->original_material=input.material_bindings;
  auto binding=input.material_bindings?std::make_shared<SourceMaterialBindings>(*input.material_bindings):std::make_shared<SourceMaterialBindings>();
  if(!input.material_bindings)binding->has_mtoon=false;
  if(!binding->uv_origin){auto origin=std::make_shared<SourceUvOrigin>();origin->paint=recipe->original_paint;origin->appearance=input.appearance;origin->deformation=input.deformation;binding->uv_origin=std::move(origin);}
  binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;binding->normal_frames=std::move(candidate.frames);binding->normal_preparation=std::move(recipe);
  if(binding->has_mtoon){auto &normal=binding->material.textures[size_t(MToonSlot::Normal)];normal.index=std::max(0,normal.index);normal.texcoord=0;normal.transform_texcoord=-1;normal.offset={};normal.scale={1,1};normal.rotation=0;
    binding->material.parameters.normal_enabled=true;binding->material.parameters.normal_scale=1;
    auto &role=binding->roles[size_t(MToonSlot::Normal)];role={};role.image=prepared.paint->normal_projection()->mtoon;role.wrap_s=role.wrap_t=33071;role.min_filter=role.mag_filter=9729;
  }
  binding->validate();check();return {std::move(prepared.paint),std::move(binding),true};
}
} // namespace dome
