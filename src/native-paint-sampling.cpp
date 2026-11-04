// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-paint-sampling.hpp"
#include "paint-sampling-access.hpp"
#include "dome/color-paint-sampling.hpp"
#include <nlohmann/json.hpp>
#include <numeric>
#include <cmath>
namespace dome::detail {namespace {
void stop(const PaintSamplingLimits &limits){if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Native paint retention cancelled");}
bool equal(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
void replay_budget(const PaintedMesh &before,const Mesh &target,const PaintSamplingLimits &limits){
  if(!limits.max_vertices||limits.max_vertices>2'000'000||!limits.max_triangles||limits.max_triangles>2'000'000||
     !limits.max_working_bytes||limits.max_working_bytes>512*1024*1024||!limits.max_work||limits.max_work>256'000'000)
    throw std::invalid_argument("Invalid native paint replay limits");
  const auto &source=before.mesh();
  if(source.positions.size()>limits.max_vertices||target.positions.size()>limits.max_vertices||
     source.indices.size()/3>limits.max_triangles||target.indices.size()/3>limits.max_triangles)
    throw std::length_error("Native paint replay exceeds geometry budget before allocation");
  const uint64_t work=uint64_t(source.positions.size())*8+uint64_t(target.positions.size())*8+uint64_t(target.indices.size())*16;
  if(work>limits.max_work)throw std::length_error("Native paint replay exceeds work budget before allocation");
  const size_t bytes=target.positions.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+target.indices.size()*sizeof(uint32_t)+target.uv_pins.size()+target.uv_seams.size();
  if(bytes>limits.max_working_bytes)throw std::length_error("Native paint replay exceeds scratch budget before allocation");
}
void same_mesh(const Mesh &a,const Mesh &b,const PaintSamplingLimits &limits){
  if(a.positions.size()!=b.positions.size()||a.normals.size()!=b.normals.size()||a.indices!=b.indices||a.uv!=b.uv||a.uv_pins!=b.uv_pins||a.uv_seams!=b.uv_seams)
    throw std::invalid_argument("Native retained paint differs from its exact ordered REST layout");
  for(size_t i=0;i<a.positions.size();++i){if((i&1023)==0)stop(limits);if(!equal(a.positions[i],b.positions[i])||!equal(a.normals[i],b.normals[i]))throw std::invalid_argument("Native retained paint changed REST positions or normals");}
}
}
const std::shared_ptr<const PaintedMesh> &NativePaintSampling::original_paint() const noexcept {
  const auto *node=this;while(node->before_material_&&node->before_material_->native_paint_sampling)node=node->before_material_->native_paint_sampling.get();return node->before_;
}
const Material &NativePaintSampling::original_native_material() const noexcept {
  const auto *node=this;while(node->before_material_&&node->before_material_->native_paint_sampling)node=node->before_material_->native_paint_sampling.get();return node->native_;
}
bool NativePaintSampling::valid_owner(const std::shared_ptr<const NativePaintSampling> &owner) const noexcept {auto actual=self_.lock();return actual&&actual==owner&&!actual.owner_before(owner)&&!owner.owner_before(actual);}
bool NativePaintSampling::matches_current(const PaintedMesh &paint) const noexcept {
  return attachment_&&before_&&paint.resolution()==before_->resolution()&&paint.valid_paint_sampling_attachment()&&attachment_->matches(paint.mesh_owner(),paint.paint_sampling_domain());
}
void NativePaintSampling::validate_mesh(const Mesh &mesh,const PaintSamplingLimits &limits) const {
  stop(limits);if(!attachment_)throw std::invalid_argument("Native retained paint has no sealed current geometry");same_mesh(mesh,*attachment_->authored_geometry(),limits);
}
void NativePaintSampling::validate(const PaintSamplingLimits &limits) const {
  stop(limits);native_.validate();
  if(!before_||!layout_||!domain_||!attachment_||before_->in_stroke()||attachment_->domain()!=domain_||!depth_||depth_>32)
    throw std::invalid_argument("Native retained paint lacks its independently captured owner");
  const auto previous=before_material_?before_material_->native_paint_sampling:nullptr;
  if(bool(previous)!=bool(before_->paint_sampling_domain())||depth_!=(previous?previous->history_depth()+1:1))throw std::invalid_argument("Native paint history lost its exact prior owner");
  if(before_material_){before_material_->validate();if(!previous||!previous->matches_current(*before_))throw std::invalid_argument("Native before-Paint borrowed another material attachment");}
  replay_budget(*before_,*attachment_->authored_geometry(),limits);domain_->storage_bytes(limits);
  if(previous){if(domain_->basis()!=previous->domain()->basis()||domain_->basis().owner_before(previous->domain()->basis())||previous->domain()->basis().owner_before(domain_->basis()))throw std::invalid_argument("Native paint history changed its actual original basis owner");}
  else same_mesh(*domain_->basis(),before_->mesh(),limits);
  const auto prior=before_->paint_sampling_domain();
  if(domain_->source_corners().size()!=before_->mesh().indices.size())throw std::invalid_argument("Native paint corner history differs");
  for(size_t c=0;c<domain_->source_corners().size();++c){if((c&1023)==0)stop(limits);if(domain_->source_corners()[c]!=(prior?prior->source_corners()[c]:uint32_t(c)))throw std::invalid_argument("Native paint changed ordered original sampling corners");}
  const auto target=layout_->target_mesh(before_->mesh(),limits.cancelled);domain_->validate_current(target,limits);same_mesh(target,*attachment_->authored_geometry(),limits);
}
void NativePaintSampling::validate_current(const PaintedMesh &paint,const PaintSamplingLimits &limits) const {
  validate(limits);if(!matches_current(paint))throw std::invalid_argument("Native retained paint borrowed another current attachment or grid");
}
bool native_paint_retention_supported(const SculptObject &source) noexcept {
  if(!source.mesh_active||!source.painted||source.painted->in_stroke()||source.appearance||source.deformation||source.shape_source||source.rig_source||source.polygon||
     source.painted->source_maps()||source.painted->color_projection()||source.painted->normal_projection()||source.painted->baked()||source.painted->source_sampling_prepared()||source.painted->surface_diagnostics().policy!=PaintUvPolicy::Strict)return false;
  const auto &b=source.material_bindings;
  return !b?!source.painted->paint_sampling_domain():!b->has_mtoon&&!b->normal_frames&&!b->key_derivation&&!b->normal_preparation&&!b->color_preparation&&b->native_paint_sampling&&b->native_paint_sampling->valid_owner(b->native_paint_sampling)&&b->native_paint_sampling->history_depth()<32&&b->native_paint_sampling->matches_current(*source.painted);
}
std::shared_ptr<const SourceMaterialBindings> bind_native_paint(const std::shared_ptr<const NativePaintSampling> &owner,std::shared_ptr<const SourceMaterialBindings> previous){
  if(!owner)throw std::invalid_argument("Native paint binding needs its real owner");
  SourceMaterialBindings result=previous?*previous:SourceMaterialBindings{};result.has_mtoon=false;result.native_paint_sampling=owner;
  if(!previous){const auto &m=owner->original_native_material();result.native_reference={m.color,m.emissive,int(m.alpha),m.alpha_cutoff,m.double_sided};SourceUvOrigin origin;origin.paint=owner->original_paint();result.uv_origin=std::make_shared<const SourceUvOrigin>(std::move(origin));}
  result.sampling_reparameterized=true;result.validate();return std::make_shared<const SourceMaterialBindings>(std::move(result));
}
std::shared_ptr<const PaintSamplingAttachment> material_paint_attachment(const SourceMaterialBindings *b){
  if(!b)return {};const auto color=b->color_preparation?b->color_preparation->paint_sampling:nullptr;
  if(color&&b->native_paint_sampling)throw std::invalid_argument("One paint grid cannot borrow two distinct material owners");
  return b->native_paint_sampling?b->native_paint_sampling->attachment():color?color->geometry_attachment():nullptr;
}
struct NativePaintSamplingAccess {
 template<class Capture,class CaptureBinding>static RetainedNativePaint build(const SculptObject &source,Mesh target,
    std::span<const uint32_t> vertices,const PaintSamplingLimits &limits,
    std::shared_ptr<const PaintSamplingDomain> pooled,Capture capture,CaptureBinding capture_binding){
  stop(limits);
  if(!native_paint_retention_supported(source))throw std::invalid_argument("Native paint retention requires its own supported native grid and history");
  if(!limits.max_vertices||limits.max_vertices>2'000'000||!limits.max_triangles||limits.max_triangles>2'000'000||
     !limits.max_basis_bytes||limits.max_basis_bytes>128*1024*1024||!limits.max_working_bytes||limits.max_working_bytes>512*1024*1024||!limits.max_work||limits.max_work>256'000'000)
    throw std::invalid_argument("Invalid native retained paint limits");
  const auto &before=source.painted->mesh();
  if(before.positions.size()>limits.max_vertices||target.positions.size()>limits.max_vertices||
     before.indices.size()/3>limits.max_triangles||target.indices.size()/3>limits.max_triangles||
     vertices.size()!=target.positions.size()||before.indices.size()!=target.indices.size())
    throw std::length_error("Native retained layout exceeds geometry budget");
  const size_t basis_bytes=before.positions.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+before.indices.size()*2*sizeof(uint32_t)+before.uv_pins.size()+before.uv_seams.size();
  if(basis_bytes>limits.max_basis_bytes)throw std::length_error("Native retained basis exceeds budget before allocation");
  const uint64_t pixels=uint64_t(source.painted->resolution())*source.painted->resolution();
  uint64_t work=pixels*16+uint64_t(before.positions.size())*8+uint64_t(before.indices.size())*16;
  if(work>limits.max_work)throw std::length_error("Native retained fixed work exceeds budget before allocation");
  for(size_t c=0;c<before.indices.size();c+=3){
    if((c&1023)==0)stop(limits);
    const auto a=before.uv.at(before.indices[c]),b=before.uv.at(before.indices[c+1]),d=before.uv.at(before.indices[c+2]);
    const int r=int(source.painted->resolution());
    auto extent=[&](size_t axis){const int low=std::max(0,int(std::floor(std::min({a[axis],b[axis],d[axis]})*r))),high=std::min(r-1,int(std::ceil(std::max({a[axis],b[axis],d[axis]})*r)));return high<low?uint64_t(0):uint64_t(high-low+1);};
    const auto samples=extent(0)*extent(1);if(samples>limits.max_work-work)throw std::length_error("Native retained raster work exceeds budget before allocation");work+=samples;
  }
  size_t metadata=sizeof(NativePaintSampling),scratch=0;
  if(metadata>limits.max_working_bytes)throw std::length_error("Native retained owner exceeds working budget before allocation");
  auto charge=[&](size_t n,size_t unit){if(n>limits.max_working_bytes/unit||n*unit>limits.max_working_bytes-metadata)throw std::length_error("Native retained metadata exceeds working budget before allocation");metadata+=n*unit;};
  for(const auto &layer:source.painted->layers()){charge(layer.name.size()+1,3);charge(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size(),3*sizeof(std::shared_ptr<const void>));}
  charge(target.positions.size(),sizeof(uint32_t)+sizeof(std::array<float,2>));charge(target.indices.size(),2*sizeof(uint32_t));charge(target.uv_pins.size()+target.uv_seams.size(),1);
  scratch=metadata;auto budget=limits;if(scratch>=budget.max_working_bytes)throw std::length_error("Native retained metadata exhausts working budget");budget.max_working_bytes-=scratch;budget.max_basis_bytes=std::min(budget.max_basis_bytes,budget.max_working_bytes);
  SourceNormalUvLayout value;value.source_vertices.assign(vertices.begin(),vertices.end());value.indices=target.indices;value.uv=target.uv;value.pins=target.uv_pins;value.seams=target.uv_seams;
  auto layout=std::make_shared<const SourceNormalUvLayout>(std::move(value));
  const auto replay=layout->target_mesh(before,limits.cancelled);same_mesh(target,replay,limits);
  std::vector<uint32_t> corners(target.indices.size());std::iota(corners.begin(),corners.end(),0u);
  auto domain=retain_paint_sampling(before,source.painted->paint_sampling_domain(),target,corners,budget);
  if(pooled){
    pooled->validate_current(target,budget);same_mesh(*domain->basis(),*pooled->basis(),limits);
    if(domain->source_corners().size()!=pooled->source_corners().size()||!std::equal(domain->source_corners().begin(),domain->source_corners().end(),pooled->source_corners().begin()))throw std::invalid_argument("Native paint pool differs from exact source replay");
    if(const auto prior=source.painted->paint_sampling_domain();prior&&(pooled->basis()!=prior->basis()||pooled->basis().owner_before(prior->basis())||prior->basis().owner_before(pooled->basis())))throw std::invalid_argument("Native paint pool lost original basis alias");domain=std::move(pooled);
  }
  // Freeze mutable layer names/visibility/opacity and tile tables. Surface and
  // immutable pixel owners remain shared; current metadata cannot edit anchor.
  auto captured=capture(source.painted);
  auto paint=retain_paint_grid(*captured,std::move(target),domain,budget);
  auto owner=std::shared_ptr<NativePaintSampling>(new NativePaintSampling);owner->before_=std::move(captured);owner->native_=source.material;owner->before_material_=capture_binding(source.material_bindings);owner->depth_=source.material_bindings?source.material_bindings->native_paint_sampling->history_depth()+1:1;owner->layout_=std::move(layout);owner->domain_=std::move(domain);owner->attachment_=paint->paint_sampling_attachment();
  owner->self_=owner;owner->validate_current(*paint,limits);stop(limits);return {std::move(paint),std::move(owner)};
}
};
RetainedNativePaint retain_native_paint(const SculptObject &source,Mesh target,std::span<const uint32_t> vertices,const PaintSamplingLimits &limits){
  return NativePaintSamplingAccess::build(source,std::move(target),vertices,limits,{},[](const auto &paint){return std::make_shared<const PaintedMesh>(*paint->snapshot());},[](const auto &binding){return binding?std::make_shared<const SourceMaterialBindings>(*binding):nullptr;});
}
RetainedNativePaint NativePaintSamplingCodec::replay(std::shared_ptr<const PaintedMesh> before,Material native,
    std::shared_ptr<const SourceMaterialBindings> binding,std::shared_ptr<const SourceNormalUvLayout> layout,
    std::shared_ptr<const PaintSamplingDomain> domain,const PaintSamplingLimits &limits){
  if(!before||!layout||!domain)throw std::invalid_argument("Native paint replay is incomplete");
  SculptObject source;source.mesh_active=true;source.painted=std::const_pointer_cast<PaintedMesh>(before);source.material=std::move(native);source.material_bindings=std::move(binding);
  replay_budget(*before,*domain->basis(),limits);
  auto target=layout->target_mesh(before->mesh(),limits.cancelled);
  return NativePaintSamplingAccess::build(source,std::move(target),layout->source_vertices,limits,std::move(domain),[&](const auto &){return before;},[](const auto &binding){return binding;});
}
nlohmann::json encode_native_material(const Material &m){m.validate();return {{"name",m.name},{"color",m.color},{"metallic",m.metallic},{"roughness",m.roughness},{"emissive",m.emissive},{"toon",m.toon},{"double_sided",m.double_sided},{"alpha",int(m.alpha)},{"alpha_cutoff",m.alpha_cutoff},{"unlit",m.unlit}};}
Material decode_native_material(const nlohmann::json &j){
 if(!j.is_object()||j.size()!=10)throw std::invalid_argument("Native paint material record shape differs");
 Material m;m.name=j.at("name").get<std::string>();m.color=j.at("color").get<std::array<float,4>>();m.metallic=j.at("metallic");m.roughness=j.at("roughness");m.emissive=j.at("emissive").get<std::array<float,3>>();m.toon=j.at("toon");m.double_sided=j.at("double_sided");const auto alpha=j.at("alpha");if(!alpha.is_number_integer()||alpha.get<int64_t>()<0||alpha.get<int64_t>()>3)throw std::invalid_argument("Invalid native paint alpha mode");m.alpha=Material::AlphaMode(alpha.get<int>());m.alpha_cutoff=j.at("alpha_cutoff");m.unlit=j.at("unlit");m.validate();return m;
}
} // namespace dome::detail
