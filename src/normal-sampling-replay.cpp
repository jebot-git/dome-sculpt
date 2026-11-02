// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/normal-preparation.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/humanoid-creator.hpp"
#include "normal-frame-runtime.hpp"
#include "normal-sampling-access.hpp"
#include "normal-projection-identity.hpp"
#include "paint-normal-access.hpp"
#include "source-paint-pairing.hpp"
#include <bit>

namespace dome {
namespace {
void stop(const std::function<bool()> &cancelled) {
  if(cancelled&&cancelled())throw std::runtime_error("Retained normal sampling replay cancelled");
}
uint64_t count(const nlohmann::json &j,uint64_t maximum) {
  if((!j.is_number_unsigned()&&(!j.is_number_integer()||j.get<int64_t>()<0))||j.get<uint64_t>()>maximum)
    throw std::invalid_argument("Normal sampling receipt integer exceeds its bound");
  return j.get<uint64_t>();
}
bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool same_image(const Image &a,const Image &b) {
  return a.width==b.width&&a.height==b.height&&a.rgba==b.rgba;
}
bool same_owner(const std::shared_ptr<const Image> &a,const std::shared_ptr<const Image> &b) {
  return a==b&&!a.owner_before(b)&&!b.owner_before(a);
}
bool same_uv(std::array<float,2> a,std::array<float,2> b) {
  return std::bit_cast<uint32_t>(a[0])==std::bit_cast<uint32_t>(b[0])&&
         std::bit_cast<uint32_t>(a[1])==std::bit_cast<uint32_t>(b[1]);
}
bool same_sampling(const MToonMaterial &a,const MToonMaterial &b) {
  if(a.version()!=b.version()||a.parameters.scroll!=b.parameters.scroll||
     a.parameters.rotation_speed!=b.parameters.rotation_speed)return false;
  const auto role=size_t(MToonSlot::Normal);
  const auto transform=a.version()==MToonVersion::V0?size_t(MToonSlot::Base):role;
  const auto &x=a.textures[transform],&y=b.textures[transform];
  const auto &cx=a.textures[role],&cy=b.textures[role];
  return cx.texcoord==cy.texcoord&&cx.transform_texcoord==cy.transform_texcoord&&
    x.offset==y.offset&&x.scale==y.scale&&x.rotation==y.rotation;
}
std::vector<std::array<float,2>> source_mtoon_coordinates(const SourceNormalSampling &proof,
    const std::function<bool()> &cancelled) {
  stop(cancelled);
  const auto previous=proof.before_paint->normal_projection();
  if(previous->version==2)return previous->sampling_domain->mtoon_coordinates;
  if(!proof.before_material->has_mtoon)return {};
  const auto &normal=proof.before_material->material.textures[size_t(MToonSlot::Normal)];
  const auto coordinate=normal.transform_texcoord>=0?normal.transform_texcoord:int(normal.texcoord);
  if(coordinate==0)return {};
  if(coordinate!=1)throw std::invalid_argument("Retained normal source coordinate set is unsupported");
  const auto &mesh=proof.before_paint->mesh();
  const auto maps=proof.before_paint->source_maps();
  std::vector<std::array<float,2>> result;
  if(maps&&maps->uv1){maps->validate_surface(mesh);result=*maps->uv1;}
  if(proof.before_appearance){
    const auto &appearance=*proof.before_appearance;
    if(!appearance.asset||appearance.source_vertices.size()!=mesh.positions.size())
      throw std::invalid_argument("Retained normal UV1 source appearance is incomplete");
    const ImportedPrimitive *primitive=nullptr;
    for(const auto &candidate:appearance.asset->primitives)
      if(candidate.source_mesh==appearance.source_mesh&&candidate.source_primitive==appearance.source_primitive){
        if(primitive)throw std::invalid_argument("Retained normal source primitive is ambiguous");
        primitive=&candidate;
      }
    if(!primitive||primitive->uv1.size()!=primitive->mesh.positions.size())
      throw std::invalid_argument("Retained normal UV1 source accessor is unavailable");
    std::vector<std::array<float,2>> imported;imported.reserve(mesh.positions.size());
    for(size_t v=0;v<appearance.source_vertices.size();++v){
      if((v&1023)==0)stop(cancelled);const auto vertex=appearance.source_vertices[v];
      if(vertex>=primitive->uv1.size())throw std::out_of_range("Retained normal UV1 source vertex is unavailable");
      imported.push_back(primitive->uv1[vertex]);
    }
    if(!result.empty()&&result!=imported)
      throw std::invalid_argument("Retained normal UV1 source authorities disagree");
    result=std::move(imported);
  }
  if(result.size()!=mesh.positions.size())
    throw std::invalid_argument("Retained normal UV1 source needs an owning coordinate table");
  return result;
}
}
void NormalSamplingReceipt::validate() const {
  if(version!=1||resolution<64||resolution>2048||(resolution&(resolution-1))||
     !vertices||vertices>2'000'000||!indices||indices>6'000'000||indices%3||
     !correspondence_bytes||correspondence_bytes>128*1024*1024||
     !max_working_bytes||max_working_bytes>512*1024*1024)
    throw std::invalid_argument("Invalid exact normal sampling resource/correspondence receipt");
}
nlohmann::json NormalSamplingReceipt::encode() const {
  validate();return {{"version",version},{"resolution",resolution},{"vertices",vertices},
    {"indices",indices},{"correspondence_bytes",correspondence_bytes},{"max_working_bytes",max_working_bytes}};
}
NormalSamplingReceipt NormalSamplingReceipt::decode(const nlohmann::json &j) {
  if(!j.is_object()||j.size()!=6||count(j.at("version"),1)!=1)
    throw std::invalid_argument("Unsupported exact normal sampling receipt");
  NormalSamplingReceipt result;result.resolution=uint32_t(count(j.at("resolution"),2048));
  result.vertices=count(j.at("vertices"),2'000'000);result.indices=count(j.at("indices"),6'000'000);
  result.correspondence_bytes=count(j.at("correspondence_bytes"),128*1024*1024);
  result.max_working_bytes=count(j.at("max_working_bytes"),512*1024*1024);
  result.validate();return result;
}
namespace detail {
std::shared_ptr<const SourceNormalSampling> retain_normal_sampling(const SourceUvBakeInput &input,
    const SourceUvBakeResult &result,const SourceUvBakeOptions &options) {
  stop(options.cancelled);
  if(!input.paint||!input.paint->normal_projection()||!input.current_material||
     !input.current_material->normal_preparation)
    throw std::invalid_argument("Exact normal UV retention requires its own prepared source");
  const auto r=input.paint->resolution();const size_t pixels=size_t(r)*r;
  // Bound the simultaneously live correspondence/layout, replay target mesh,
  // retained resolved/detail output and both independent replay scratch planes.
  // This new interpretation is deliberately conservative; legacy v1 budgets
  // and quality counters retain their original meaning.
  const size_t geometry=input.target.positions.size()*60+input.target.indices.size()*12+
    2*(input.target.uv_pins.size()+input.target.uv_seams.size());
  if(geometry>options.max_working_bytes||pixels>(options.max_working_bytes-geometry)/81)
    throw std::length_error("Exact normal domain construction/replay exceeds its cumulative scratch budget");
  auto proof=std::make_shared<SourceNormalSampling>();
  proof->before_paint=input.paint;proof->before_material=input.current_material;
  proof->before_appearance=input.appearance;proof->before_deformation=input.current_deformation;
  const auto previous=input.paint->normal_projection();
  proof->sampling_material=input.current_material->normal_preparation->sampling_domain
    ?input.current_material->normal_preparation->sampling_domain->sampling_material:input.current_material;
  proof->current_sampling_reference=result.mtoon;
  const auto &old_coordinates=previous->version==2?previous->sampling_domain->coordinates:input.paint->mesh().uv;
  const auto toon_coordinates=source_mtoon_coordinates(*proof,options.cancelled);
  auto domain=std::make_shared<NormalSamplingDomain>();domain->resolution=r;
  domain->indices=input.target.indices;domain->coordinates.reserve(input.source_vertices.size());
  if(!toon_coordinates.empty())domain->mtoon_coordinates.reserve(input.source_vertices.size());
  for(size_t v=0;v<input.source_vertices.size();++v){
    if((v&1023)==0)stop(options.cancelled);const auto old=input.source_vertices[v];
    domain->coordinates.push_back(old_coordinates.at(old));
    if(!toon_coordinates.empty())domain->mtoon_coordinates.push_back(toon_coordinates.at(old));
  }
  const auto resolved=SourceNormalPreparationAccess::resolved(*input.paint,options.cancelled);
  domain->resolved_pbr=resolved->pbr;domain->resolved_mtoon=resolved->mtoon;
  domain->detail=transfer_normal_detail(*input.paint,input.target,options);
  auto projection=std::make_shared<PaintNormalProjection>();projection->version=2;
  projection->pbr=previous->pbr;projection->mtoon=previous->mtoon;projection->sampling_domain=domain;
  auto layout=std::make_shared<SourceNormalUvLayout>();layout->source_vertices=input.source_vertices;
  layout->indices=input.target.indices;layout->uv=input.target.uv;layout->pins=input.target.uv_pins;layout->seams=input.target.uv_seams;
  proof->projection=projection;proof->layout=layout;
  proof->receipt.resolution=r;proof->receipt.vertices=input.target.positions.size();proof->receipt.indices=input.target.indices.size();
  proof->receipt.correspondence_bytes=layout->storage_bytes()+domain->storage_bytes();
  proof->receipt.max_working_bytes=options.max_working_bytes;
  proof->validate(true,options.cancelled);proof->validate_current(input.target,result.appearance.get(),options.cancelled);
  return proof;
}
} // namespace detail
void SourceNormalSampling::validate(bool deep,const std::function<bool()> &cancelled) const {
  stop(cancelled);receipt.validate();
  if(!layout||!projection||projection->version!=2||!projection->sampling_domain||
     !before_paint||before_paint->in_stroke()||!before_paint->normal_projection()||
     !before_material||!before_material->normal_preparation||!sampling_material||
     before_paint->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored)
    throw std::invalid_argument("Retained normal field needs its complete own prepared before-state");
  const auto &domain=*projection->sampling_domain;
  const auto &previous=*before_paint->normal_projection();
  projection->validate(receipt.resolution,deep);layout->storage_bytes();
  const size_t replay_mesh=layout->uv.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+
    layout->indices.size()*4+layout->pins.size()+layout->seams.size();
  const size_t owned=layout->storage_bytes()+domain.storage_bytes();
  const size_t pixels=size_t(receipt.resolution)*receipt.resolution;
  if(owned>receipt.max_working_bytes||replay_mesh>receipt.max_working_bytes-owned||
     pixels>(receipt.max_working_bytes-owned-replay_mesh)/80)
    throw std::length_error("Retained normal source/replay planes exceed their cumulative scratch receipt");
  if(before_paint->resolution()!=receipt.resolution||receipt.vertices!=layout->source_vertices.size()||
     receipt.indices!=layout->indices.size()||domain.indices!=layout->indices||
     domain.coordinates.size()!=layout->source_vertices.size()||
     receipt.correspondence_bytes!=layout->storage_bytes()+domain.storage_bytes())
    throw std::invalid_argument("Retained normal field correspondence dimensions differ");
  const auto expected_sampler=before_material->normal_preparation->sampling_domain
    ?before_material->normal_preparation->sampling_domain->sampling_material:before_material;
  if(sampling_material!=expected_sampler||
     !same_owner(projection->pbr,previous.pbr)||!same_owner(projection->mtoon,previous.mtoon))
    throw std::invalid_argument("Retained normal field changed its intrinsic owners or captured sampler source");
  if(before_material->has_mtoon!=bool(current_sampling_reference)||
     sampling_material->has_mtoon!=before_material->has_mtoon)
    throw std::invalid_argument("Retained normal material interpretation differs");
  if(sampling_material->has_mtoon){
    const auto &material=sampling_material->material;
    if(material.parameters.scroll!=std::array<float,2>{}||material.parameters.rotation_speed!=0)
      throw std::invalid_argument("Animated normal sampling requires an explicit retained-source animation adapter");
    current_sampling_reference->validate();
    auto expected=before_material->material;
    for(auto &texture:expected.textures){texture.texcoord=0;texture.transform_texcoord=-1;
      texture.offset={};texture.scale={1,1};texture.rotation=0;}
    if(!same_sampling(expected,*current_sampling_reference))
      throw std::invalid_argument("Retained normal current UV reference differs from its source-aware atlas operation");
  }
  before_material->validate(deep);
  detail::source_paint_pairing(before_paint.get(),before_material.get(),before_appearance.get(),deep);
  if(!deep)return;
  detail::normal_frame_current_authority(*before_material,before_paint->mesh(),before_deformation.get(),cancelled);
  if(before_paint->mesh().positions.size()>receipt.max_working_bytes/(2*sizeof(std::array<float,2>)))
    throw std::length_error("Retained normal role-coordinate replay exceeds scratch budget");
  {
    const auto &source_coordinates=previous.version==2?previous.sampling_domain->coordinates:before_paint->mesh().uv;
    const auto toon_coordinates=source_mtoon_coordinates(*this,cancelled);
    if(domain.mtoon_coordinates.empty()!=toon_coordinates.empty())
      throw std::invalid_argument("Retained normal role-coordinate alias changed");
    for(size_t v=0;v<layout->source_vertices.size();++v){
      if((v&1023)==0)stop(cancelled);const auto old=layout->source_vertices[v];
      if(old>=source_coordinates.size()||!same_uv(domain.coordinates[v],source_coordinates[old])||
         (!toon_coordinates.empty()&&(old>=toon_coordinates.size()||!same_uv(domain.mtoon_coordinates[v],toon_coordinates[old]))))
        throw std::invalid_argument("Retained normal field lost exact source-coordinate correspondence");
    }
  }
  const size_t mesh_bytes=layout->uv.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+
    layout->indices.size()*4+layout->pins.size()+layout->seams.size();
  if(mesh_bytes>=receipt.max_working_bytes)
    throw std::length_error("Retained normal replay geometry exceeds working budget");
  auto target=layout->target_mesh(before_paint->mesh(),cancelled);
  {
    const auto resolved=SourceNormalPreparationAccess::resolved(*before_paint,cancelled);
    if(!same_image(*resolved->pbr,*domain.resolved_pbr)||!same_image(*resolved->mtoon,*domain.resolved_mtoon))
      throw std::invalid_argument("Retained normal resolved planes differ from exact pre-filter source replay");
  }
  SourceUvBakeOptions options;options.cancelled=cancelled;
  options.max_working_bytes=size_t(receipt.max_working_bytes)-mesh_bytes;
  const auto expected=detail::transfer_normal_detail(*before_paint,target,options);
  if(bool(expected)!=bool(domain.detail))throw std::invalid_argument("Retained normal detail presence differs from owning transfer");
  if(expected&&(expected->coverage!=domain.detail->coverage||
      !same_image(*expected->pbr,*domain.detail->pbr)||!same_image(*expected->mtoon,*domain.detail->mtoon)))
    throw std::invalid_argument("Retained normal detail metadata differs from owning exact transfer");
  stop(cancelled);
}
void SourceNormalSampling::validate_current(const Mesh &mesh,const CreatorAppearance *appearance,
    const std::function<bool()> &cancelled) const {
  if(!layout||!before_paint)throw std::invalid_argument("Retained normal current geometry lost its own before-state");
  layout->storage_bytes();mesh.validate();const auto &before=before_paint->mesh();
  if(mesh.positions.size()!=layout->source_vertices.size()||mesh.indices!=layout->indices||
     mesh.uv!=layout->uv||mesh.uv_pins!=layout->pins||mesh.uv_seams!=layout->seams||
     bool(appearance)!=bool(before_appearance))
    throw std::invalid_argument("Current retained normal topology/atlas differs");
  if(appearance&&(appearance->asset!=before_appearance->asset||
     appearance->source_mesh!=before_appearance->source_mesh||
     appearance->source_primitive!=before_appearance->source_primitive||
     appearance->source_vertices.size()!=mesh.positions.size()))
    throw std::invalid_argument("Current retained normal primitive identity differs");
  for(size_t v=0;v<mesh.positions.size();++v){
    if((v&1023)==0)stop(cancelled);const auto old=layout->source_vertices[v];
    if(old>=before.positions.size()||!same(mesh.positions[v],before.positions[old])||
       !same(mesh.normals[v],before.normals[old])||
       (appearance&&appearance->source_vertices[v]!=before_appearance->source_vertices.at(old)))
      throw std::invalid_argument("Current retained normal REST/corner source differs");
  }
}
void SourceNormalSampling::validate_paint(const PaintedMesh &paint,const SourceMaterialBindings &binding,bool deep,
    const std::function<bool()> &cancelled) const {
  stop(cancelled);
  if(!paint.normal_projection()||!detail::normal_projection_identity(*paint.normal_projection(),*projection))
    throw std::invalid_argument("Current retained normal Paint and its own field differ");
  if(binding.has_mtoon!=bool(current_sampling_reference)||
     (binding.has_mtoon&&!same_sampling(binding.material,*current_sampling_reference)))
    throw std::invalid_argument("Normal UV material edits require an explicit retained-source transition");
  if(const auto baked=paint.baked();baked&&!baked->maps.normal.rgba.empty()){
    if(const auto detail=baked->normal_authority;detail){
      for(const auto &image:{detail->pbr,detail->mtoon})
        if(!image.owner_before(baked)&&!baked.owner_before(image))
          throw std::invalid_argument("Normal detail images cannot own their current baked container");
    }
    if(!detail::same_normal_detail(projection->sampling_domain->detail.get(),baked.get(),paint.resolution(),deep))
      throw std::invalid_argument("Current normal bake is not the captured retained-field event");
  }
  if(binding.has_mtoon){const auto &normal=binding.roles[size_t(MToonSlot::Normal)];
    if(normal.image&&(!same_owner(normal.image,projection->mtoon)||normal.wrap_s!=33071||
       normal.wrap_t!=33071||normal.min_filter!=9729||normal.mag_filter!=9729))
      throw std::invalid_argument("Current normal texture replacement requires a source-owning field transition");
  }
  stop(cancelled);
}
} // namespace dome
