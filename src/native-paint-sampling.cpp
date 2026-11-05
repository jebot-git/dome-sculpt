// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-paint-model.hpp"
#include "canonical-paint-sampling.hpp"
#include "paint-sampling-access.hpp"
#include "normal-frame-runtime.hpp"
#include "dome/color-paint-sampling.hpp"
#include <nlohmann/json.hpp>
#include <numeric>
#include <cmath>
namespace dome::detail {namespace {
void stop(const PaintSamplingLimits &limits){if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Native paint retention cancelled");}
bool equal(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool native_grid_state(const PaintedMesh &p) noexcept {return !p.source_maps()&&!p.color_projection()&&!p.normal_projection()&&!p.baked()&&!p.source_sampling_prepared()&&p.surface_diagnostics().policy==PaintUvPolicy::Strict;}
void replay_budget(const PaintedMesh &before,size_t target_vertices,size_t target_indices,size_t pins,size_t seams,const PaintSamplingLimits &limits){
  if(!limits.max_vertices||limits.max_vertices>2'000'000||!limits.max_triangles||limits.max_triangles>2'000'000||
     !limits.max_working_bytes||limits.max_working_bytes>512*1024*1024||!limits.max_work||limits.max_work>256'000'000)
    throw std::invalid_argument("Invalid native paint replay limits");
  const auto &source=before.mesh();
  if(source.positions.size()>limits.max_vertices||target_vertices>limits.max_vertices||
     source.indices.size()/3>limits.max_triangles||target_indices/3>limits.max_triangles)
    throw std::length_error("Native paint replay exceeds geometry budget before allocation");
  const uint64_t work=uint64_t(source.positions.size())*8+uint64_t(target_vertices)*8+uint64_t(target_indices)*16;
  if(work>limits.max_work)throw std::length_error("Native paint replay exceeds work budget before allocation");
  const size_t bytes=target_vertices*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+target_indices*sizeof(uint32_t)+pins+seams;
  if(bytes>limits.max_working_bytes)throw std::length_error("Native paint replay exceeds scratch budget before allocation");
}
void replay_budget(const PaintedMesh &before,const Mesh &target,const PaintSamplingLimits &limits){replay_budget(before,target.positions.size(),target.indices.size(),target.uv_pins.size(),target.uv_seams.size(),limits);}
size_t mesh_storage(const Mesh &m){return m.positions.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+m.indices.size()*sizeof(uint32_t)+m.uv_pins.size()+m.uv_seams.size();}
void layout_current(const Mesh &before,const Mesh &target,const SourceNormalUvLayout &layout,const PaintSamplingLimits &limits){
  stop(limits);layout.storage_bytes();before.validate();target.validate();
  if(layout.source_vertices.size()!=target.positions.size()||layout.indices!=target.indices||layout.uv!=target.uv||layout.pins!=target.uv_pins||layout.seams!=target.uv_seams||target.indices.size()!=before.indices.size())throw std::invalid_argument("Native layout lost exact current dimensions");
  for(size_t v=0;v<target.positions.size();++v){if((v&1023)==0)stop(limits);const auto old=layout.source_vertices[v];if(old>=before.positions.size()||!equal(target.positions[v],before.positions[old])||!equal(target.normals[v],before.normals[old]))throw std::invalid_argument("Native layout changed exact REST source correspondence");for(float q:target.uv[v])if(q<0||q>1)throw std::invalid_argument("Native UV layout is outside its unit atlas");}
  for(size_t c=0;c<target.indices.size();++c){if((c&1023)==0)stop(limits);if(layout.source_vertices[target.indices[c]]!=before.indices[c])throw std::invalid_argument("Native layout changed exact ordered source corners");}stop(limits);
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
  return native_grid_state(paint)&&attachment_&&before_&&paint.resolution()==before_->resolution()&&paint.valid_paint_sampling_attachment()&&attachment_->matches(paint.mesh_owner(),paint.paint_sampling_domain());
}
void NativePaintSampling::validate_mesh(const Mesh &mesh,const PaintSamplingLimits &limits) const {
  stop(limits);if(!attachment_)throw std::invalid_argument("Native retained paint has no sealed current geometry");same_mesh(mesh,*attachment_->authored_geometry(),limits);
}
void NativePaintSampling::validate(const PaintSamplingLimits &limits) const {
  stop(limits);native_.validate();
  if(!before_||!layout_||!domain_||!attachment_||before_->in_stroke()||attachment_->domain()!=domain_||!depth_||depth_>32)
    throw std::invalid_argument("Native retained paint lacks its independently captured owner");
  if(!native_grid_state(*before_))throw std::invalid_argument("Native paint history requires its actual unprepared writable grid");
  const auto previous=before_material_?before_material_->native_paint_sampling:nullptr;
  if(bool(previous)!=bool(before_->paint_sampling_domain())||depth_!=(previous?previous->history_depth()+1:1))throw std::invalid_argument("Native paint history lost its exact prior owner");
  if(before_material_){before_material_->validate();if(!previous||!previous->matches_current(*before_))throw std::invalid_argument("Native before-Paint borrowed another material attachment");}
  replay_budget(*before_,*attachment_->authored_geometry(),limits);domain_->storage_bytes(limits);
  if(previous){if(domain_->basis()!=previous->domain()->basis()||domain_->basis().owner_before(previous->domain()->basis())||previous->domain()->basis().owner_before(domain_->basis()))throw std::invalid_argument("Native paint history changed its actual original basis owner");}
  else same_mesh(*domain_->basis(),before_->mesh(),limits);
  const auto prior=before_->paint_sampling_domain();
  if(domain_->source_corners().size()!=before_->mesh().indices.size())throw std::invalid_argument("Native paint corner history differs");
  for(size_t c=0;c<domain_->source_corners().size();++c){if((c&1023)==0)stop(limits);if(domain_->source_corners()[c]!=(prior?prior->source_corners()[c]:uint32_t(c)))throw std::invalid_argument("Native paint changed ordered original sampling corners");}
  layout_current(before_->mesh(),*attachment_->authored_geometry(),*layout_,limits);domain_->validate_current(*attachment_->authored_geometry(),limits);
}
void NativePaintSampling::validate_current(const PaintedMesh &paint,const PaintSamplingLimits &limits) const {
  validate(limits);if(!native_grid_state(paint))throw std::invalid_argument("Native retained paint needs an explicit baked/projection/source transition");if(!matches_current(paint))throw std::invalid_argument("Native retained paint borrowed another current attachment or grid");
}
bool native_paint_retention_supported(const SculptObject &source) noexcept {
  if(!source.mesh_active||!source.painted||source.painted->in_stroke()||source.appearance||source.shape_source||source.polygon||
     (source.deformation&&source.deformation->morphs&&!source.deformation->morphs->targets.empty())||
     source.painted->source_maps()||source.painted->color_projection()||source.painted->normal_projection()||source.painted->baked()||source.painted->source_sampling_prepared()||source.painted->surface_diagnostics().policy!=PaintUvPolicy::Strict)return false;
  const auto &b=source.material_bindings;
  return !b?!source.painted->paint_sampling_domain():!b->has_mtoon&&b->normal_frame==SourceMaterialNormalFrame::UvDerivative&&!b->normal_frames&&!b->key_derivation&&!b->normal_preparation&&!b->color_preparation&&b->native_paint_sampling&&b->native_paint_sampling->valid_owner(b->native_paint_sampling)&&b->native_paint_sampling->history_depth()<32&&b->native_paint_sampling->matches_current(*source.painted);
}
bool native_paint_source_restorable(const SculptObject &o) noexcept {
  const auto b=o.material_bindings;const auto p=b?b->native_paint_sampling:nullptr;
  return o.visible&&!o.locked&&o.mesh_active&&o.painted&&!o.painted->in_stroke()&&!o.appearance&&!o.deformation&&!o.shape_source&&!o.rig_source&&!o.polygon&&!o.retopology&&!o.affine_edit&&!o.bake_recipe&&b&&!b->has_mtoon&&!b->normal_frames&&!b->key_derivation&&!b->normal_preparation&&!b->color_preparation&&p&&p->valid_owner(p)&&p->matches_current(*o.painted);
}
void restore_native_paint(Scene &scene,uint64_t object,const std::function<bool()> &cancelled){
  PaintSamplingLimits limits;limits.cancelled=cancelled;stop(limits);const auto &source=scene.object(object);
  if(!native_paint_source_restorable(source))throw std::invalid_argument("Restore UV and paint source requires its unchanged supported native geometry and history");
  source.material_bindings->validate();auto first=source.material_bindings->native_paint_sampling;first->validate_current(*source.painted,limits);
  while(first->before_material()&&first->before_material()->native_paint_sampling){stop(limits);first=first->before_material()->native_paint_sampling;}
  auto next=scene;auto &o=next.object(object);o.painted=first->before_paint()->snapshot();o.material=first->before_native_material();o.material_bindings=first->before_material();next.validate();stop(limits);scene=std::move(next);
}
std::shared_ptr<const SourceMaterialBindings> bind_native_paint(const std::shared_ptr<const NativePaintSampling> &owner,std::shared_ptr<const SourceMaterialBindings> previous){
  if(!owner)throw std::invalid_argument("Native paint binding needs its real owner");
  SourceMaterialBindings result=previous?*previous:SourceMaterialBindings{};result.has_mtoon=false;result.native_paint_sampling=owner;
  if(!previous){const auto &m=owner->original_native_material();result.native_reference={m.color,m.emissive,int(m.alpha),m.alpha_cutoff,m.double_sided};SourceUvOrigin origin;origin.paint=owner->original_paint();result.uv_origin=std::make_shared<const SourceUvOrigin>(std::move(origin));}
  result.sampling_reparameterized=true;result.validate();return std::make_shared<const SourceMaterialBindings>(std::move(result));
}
std::shared_ptr<const PaintSamplingAttachment> material_paint_attachment(const SourceMaterialBindings *b){
  if(!b)return {};const auto color=b->color_preparation?b->color_preparation->paint_sampling:nullptr;
  if(size_t(bool(color))+size_t(bool(b->native_paint_sampling))+size_t(bool(b->canonical_paint_sampling))>1)throw std::invalid_argument("One paint grid cannot borrow two distinct material owners");
  if(b->canonical_paint_sampling)return b->canonical_paint_sampling->attachment();
  return b->native_paint_sampling?b->native_paint_sampling->attachment():color?color->geometry_attachment():nullptr;
}
struct NativePaintSamplingAccess {
 template<class Capture,class CaptureBinding>static RetainedNativePaint build(const SculptObject &source,const Mesh &target,
    std::span<const uint32_t> vertices,const PaintSamplingLimits &limits,
    std::shared_ptr<const PaintSamplingDomain> pooled,Capture capture,CaptureBinding capture_binding,Mesh *owned_target=nullptr){
  stop(limits);
  if(owned_target&&owned_target!=&target)throw std::logic_error("Native decoded target lost its actual owned mesh");
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
  uint64_t work=pixels*16+uint64_t(before.positions.size())*8+uint64_t(target.positions.size())*8+uint64_t(before.indices.size())*16;
  if(work>limits.max_work)throw std::length_error("Native retained fixed work exceeds budget before allocation");
  for(size_t c=0;c<before.indices.size();c+=3){
    if((c&1023)==0)stop(limits);
    const auto a=before.uv.at(before.indices[c]),b=before.uv.at(before.indices[c+1]),d=before.uv.at(before.indices[c+2]);
    const int r=int(source.painted->resolution());
    auto extent=[&](size_t axis){const int low=std::max(0,int(std::floor(std::min({a[axis],b[axis],d[axis]})*r))),high=std::min(r-1,int(std::ceil(std::max({a[axis],b[axis],d[axis]})*r)));return high<low?uint64_t(0):uint64_t(high-low+1);};
    const auto samples=extent(0)*extent(1);if(samples>limits.max_work-work)throw std::length_error("Native retained raster work exceeds budget before allocation");work+=samples;
  }
  size_t metadata=sizeof(NativePaintSampling)+2*sizeof(PaintedMesh)+sizeof(SourceNormalUvLayout)+4096,scratch=0;
  if(metadata>limits.max_working_bytes)throw std::length_error("Native retained owner exceeds working budget before allocation");
  auto charge=[&](size_t n,size_t unit){if(n>limits.max_working_bytes/unit||n*unit>limits.max_working_bytes-metadata)throw std::length_error("Native retained metadata exceeds working budget before allocation");metadata+=n*unit;};
  charge(source.material.name.size()+1,3);
  for(const auto &layer:source.painted->layers()){charge(layer.name.size()+1,3);charge(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size(),3*sizeof(std::shared_ptr<const void>));}
  charge(target.positions.size(),sizeof(uint32_t)+sizeof(std::array<float,2>));charge(target.indices.size(),2*sizeof(uint32_t));charge(target.uv_pins.size()+target.uv_seams.size(),1);
  scratch=metadata;auto budget=limits;if(scratch>=budget.max_working_bytes)throw std::length_error("Native retained metadata exhausts working budget");budget.max_working_bytes-=scratch;budget.max_basis_bytes=std::min(budget.max_basis_bytes,budget.max_working_bytes);
  const auto geometry_bytes=mesh_storage(target);if(geometry_bytes>=budget.max_working_bytes)throw std::length_error("Native authored geometry exhausts working budget before allocation");
  SourceNormalUvLayout value;value.source_vertices.assign(vertices.begin(),vertices.end());value.indices=target.indices;value.uv=target.uv;value.pins=target.uv_pins;value.seams=target.uv_seams;
  auto layout=std::make_shared<const SourceNormalUvLayout>(std::move(value));
  layout_current(before,target,*layout,limits);
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
  std::shared_ptr<PaintedMesh> paint;
  if(owned_target){auto grid_budget=budget;grid_budget.max_working_bytes-=geometry_bytes;grid_budget.max_basis_bytes=std::min(grid_budget.max_basis_bytes,grid_budget.max_working_bytes);paint=retain_paint_grid(*captured,std::move(*owned_target),domain,grid_budget);}
  else paint=retain_paint_grid_borrowed(*captured,target,domain,budget);
  auto owner=std::shared_ptr<NativePaintSampling>(new NativePaintSampling);owner->before_=std::move(captured);owner->native_=source.material;owner->before_material_=capture_binding(source.material_bindings);owner->depth_=source.material_bindings?source.material_bindings->native_paint_sampling->history_depth()+1:1;owner->layout_=std::move(layout);owner->domain_=std::move(domain);owner->attachment_=paint->paint_sampling_attachment();
  owner->self_=owner;owner->validate_current(*paint,limits);stop(limits);return {std::move(paint),std::move(owner)};
}
};
RetainedNativePaint retain_native_paint(const SculptObject &source,const Mesh &target,std::span<const uint32_t> vertices,const PaintSamplingLimits &limits){
  return NativePaintSamplingAccess::build(source,target,vertices,limits,{},[](const auto &paint){return std::make_shared<const PaintedMesh>(*paint->snapshot());},[](const auto &binding){return binding?std::make_shared<const SourceMaterialBindings>(*binding):nullptr;});
}
RetainedNativePaintModel retain_native_paint_model(const SculptObject &source,const Mesh &target,
    std::span<const uint32_t> vertices,const PaintSamplingLimits &limits){
  stop(limits);
  if(!native_paint_retention_supported(source))throw std::invalid_argument("Native UV model requires its complete supported current paint and deformation");
  replay_budget(*source.painted,target,limits);
  if(!limits.max_basis_bytes||limits.max_basis_bytes>128*1024*1024)throw std::invalid_argument("Invalid native UV model basis budget");
  auto budget=limits;
  // Charge all newly owned model arrays and metadata before their copies. The
  // retained input and first-source capsule are immutable shared owners. Paint
  // construction receives only the remaining common scratch allowance.
  size_t bytes=sizeof(DeformationState),work=0;
  auto charge=[&](size_t n,size_t stride){if(!stride||bytes>budget.max_working_bytes||n>(budget.max_working_bytes-bytes)/stride)throw std::length_error("Native UV model exceeds combined paint/deformation scratch budget");bytes+=n*stride;};
  auto visit=[&](size_t n){if(work>budget.max_work||n>budget.max_work-work)throw std::length_error("Native UV model exceeds combined remap work budget");work+=n;stop(limits);};
  if(const auto &d=source.deformation){
    d->resource_entries();visit(source.painted->mesh().positions.size()+vertices.size());
    charge(d->pose.size(),sizeof(JointTransform));charge(d->morph_weights.size(),sizeof(float));
    if(d->skeleton){charge(d->skeleton->bones.size(),sizeof(Bone));for(const auto &b:d->skeleton->bones)charge(b.name.size()+1,1);if(d->skeleton->inverse_bind_override)charge(d->skeleton->inverse_bind_override->size(),sizeof(RigMatrix));}
    if(d->humanoid)for(const auto &[name,joint]:d->humanoid->joints)charge(1,sizeof(std::pair<const std::string,uint32_t>)+64+name.size()+1);
    if(d->landmarks)for(const auto &[name,value]:d->landmarks->points)charge(1,sizeof(std::pair<const std::string,Vec3>)+64+name.size()+1);
    if(d->expressions)for(const auto &[name,values]:d->expressions->visemes){charge(1,sizeof(std::pair<const std::string,std::vector<VisemeBinding>>)+64+name.size()+1);charge(values.size(),sizeof(VisemeBinding));for(const auto &value:values)charge(value.shape.size()+1,1);}
    if(d->skin){charge(vertices.size(),sizeof(std::vector<Influence>));for(size_t v=0;v<vertices.size();++v){if((v&1023)==0)stop(limits);const auto old=vertices[v];if(old>=d->skin->vertices.size())throw std::invalid_argument("Native UV model source vertex is outside its actual skin domain");const auto n=d->skin->vertices[old].size();charge(n,sizeof(Influence));visit(n);}}
  }
  if(bytes>=budget.max_working_bytes||work>=budget.max_work)throw std::length_error("Native UV model exhausts common authoring budget");
  budget.max_working_bytes-=bytes;budget.max_basis_bytes=std::min(budget.max_basis_bytes,budget.max_working_bytes);budget.max_work-=work;
  auto deformation=normal_frame_remap_deformation(source.deformation,source.painted->mesh(),target,vertices,limits.cancelled);
  auto sampling=retain_native_paint(source,target,vertices,budget);
  stop(limits);return {std::move(sampling),std::move(deformation)};
}
RetainedNativePaint NativePaintSamplingCodec::replay(std::shared_ptr<const PaintedMesh> before,Material native,
    std::shared_ptr<const SourceMaterialBindings> binding,std::shared_ptr<const SourceNormalUvLayout> layout,
    std::shared_ptr<const PaintSamplingDomain> domain,const PaintSamplingLimits &limits){
  if(!before||!layout||!domain)throw std::invalid_argument("Native paint replay is incomplete");
  SculptObject source;source.mesh_active=true;source.painted=std::const_pointer_cast<PaintedMesh>(before);source.material=std::move(native);source.material_bindings=std::move(binding);
  layout->storage_bytes();
  replay_budget(*before,layout->source_vertices.size(),layout->indices.size(),layout->pins.size(),layout->seams.size(),limits);
  auto target=layout->target_mesh(before->mesh(),limits.cancelled);
  return NativePaintSamplingAccess::build(source,target,layout->source_vertices,limits,std::move(domain),[&](const auto &){return before;},[](const auto &binding){return binding;},&target);
}
nlohmann::json encode_native_material(const Material &m){m.validate();return {{"name",m.name},{"color",m.color},{"metallic",m.metallic},{"roughness",m.roughness},{"emissive",m.emissive},{"toon",m.toon},{"double_sided",m.double_sided},{"alpha",int(m.alpha)},{"alpha_cutoff",m.alpha_cutoff},{"unlit",m.unlit}};}
Material decode_native_material(const nlohmann::json &j){
 if(!j.is_object()||j.size()!=10)throw std::invalid_argument("Native paint material record shape differs");
 Material m;m.name=j.at("name").get<std::string>();m.color=j.at("color").get<std::array<float,4>>();m.metallic=j.at("metallic");m.roughness=j.at("roughness");m.emissive=j.at("emissive").get<std::array<float,3>>();m.toon=j.at("toon");m.double_sided=j.at("double_sided");const auto alpha=j.at("alpha");if(!alpha.is_number_integer()||alpha.get<int64_t>()<0||alpha.get<int64_t>()>3)throw std::invalid_argument("Invalid native paint alpha mode");m.alpha=Material::AlphaMode(alpha.get<int>());m.alpha_cutoff=j.at("alpha_cutoff");m.unlit=j.at("unlit");m.validate();return m;
}
} // namespace dome::detail
