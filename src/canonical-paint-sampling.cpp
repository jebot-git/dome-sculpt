// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-paint-sampling.hpp"
#include "canonical-paint-prospect.hpp"
#include "polygon-conversion.hpp"
#include "normal-frame-runtime.hpp"
#include "source-paint-pairing.hpp"
#include "canonical-key-uv.hpp"
#include "normal-key-derivation.hpp"
#include "paint-sampling-access.hpp"
#include <cstring>
namespace dome::detail {
namespace {
void stop(const PaintSamplingLimits &limits){if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Canonical paint sampling cancelled");}
}
bool CanonicalPaintSampling::valid_owner(const std::shared_ptr<const CanonicalPaintSampling> &p) const noexcept{
 return p.get()==this&&!self_.expired()&&!self_.owner_before(p)&&!p.owner_before(self_);
}
bool CanonicalPaintSampling::matches_current(const PaintedMesh &paint) const noexcept{
 return attachment_&&paint.paint_sampling_attachment()==attachment_&&
   !paint.paint_sampling_attachment().owner_before(attachment_)&&
   !attachment_.owner_before(paint.paint_sampling_attachment())&&paint.valid_paint_sampling_attachment();
}
void CanonicalPaintSampling::validate(const PaintSamplingLimits &limits,bool deep) const{
 stop(limits);if(!valid_owner(self_.lock())||!before_||!before_->paint||!before_->material_bindings||
  !normal_uv_||!normal_uv_->valid_owner(normal_uv_)||!attachment_||!attachment_->authored_geometry()||!attachment_->domain())
  throw std::invalid_argument("Canonical paint sampling lost its own exact before/current owners");
 const auto &b=*before_->material_bindings;
 if(normal_uv_->origin()!=b.canonical_attributes||(!b.key_derivation&&b.normal_frames!=b.canonical_attributes->frames()))
  throw std::invalid_argument("Canonical paint sampling borrowed a different normal attribute origin");
 // A geometry seal does not certify the caller's legally mutable material DTO.
 // Revalidate that actual before holder on each call, including its complete
 // before-binding graph; no pointer/scalar validity survives this invocation.
 b.validate(deep);
 if(b.key_derivation)canonical_key_uv_authority(*before_->paint,b,before_->deformation.get(),64'000'000,limits.max_working_bytes,limits.cancelled);
 if(b.canonical_paint_sampling){
  // The holder validation above already descended through this exact older
  // owner. Pair the immutable before Paint without recursively validating that
  // same chain a second time at every UV step.
  if(!b.canonical_paint_sampling->matches_current(*before_->paint)||before_->paint->source_maps()||
     before_->paint->source_sampling_prepared()||before_->paint->normal_projection()||before_->paint->color_projection())
   throw std::invalid_argument("Canonical paint before-state lost its exact older sampling attachment");
  if(before_->paint->baked()&&before_->paint->baked()->normal_field&&before_->paint->baked()->normal_field->origin()!=b.canonical_attributes)
   throw std::invalid_argument("Canonical paint before-state borrowed another independent normal field");
 }else source_paint_pairing(before_->paint.get(),before_->material_bindings.get(),before_->appearance.get(),deep);
 const auto &current=*attachment_->authored_geometry();normal_uv_->validate_current(current,limits.cancelled);
 if(current_sources_.size()!=current.positions.size())throw std::invalid_argument("Canonical paint source dimensions differ");
 const auto &old=before_->paint->mesh();
 if(old.indices.size()!=current.indices.size())throw std::invalid_argument("Canonical paint triangle identity differs");
 for(size_t c=0;c<current.indices.size();++c){if((c&1023)==0)stop(limits);
  if(current_sources_.at(current.indices[c])!=old.indices[c])throw std::invalid_argument("Canonical paint ordered source corners differ");}
 attachment_->domain()->validate_current(current,limits);
 stop(limits);
}
void CanonicalPaintSampling::validate_binding(const SourceMaterialBindings &binding,bool deep) const{
 auto own=[](const auto &a,const auto &b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);};
 if(!before_||!before_->material_bindings||!attachment_||!valid_owner(binding.canonical_paint_sampling)||
    binding.has_mtoon||binding.native_paint_sampling||binding.native_paint_frames||binding.color_preparation||binding.normal_preparation||
    !binding.sampling_reparameterized||!own(binding.canonical_uv,normal_uv_)||!own(binding.canonical_attributes,normal_uv_->origin())||
    !own(binding.uv_origin,before_->material_bindings->uv_origin)||binding.normal_frame!=before_->material_bindings->normal_frame)
  throw std::invalid_argument("Canonical writable paint binding lost its actual independent normal/paint source owners");
 const auto &before=*before_->material_bindings;
 const bool original=own(binding.key_derivation,before.key_derivation)&&own(binding.normal_frames,before.normal_frames);
 if(!original){
  // A later key owns CURRENT attribute authoring. It does not replace this
  // immutable writable-UV transition or inherit its earlier normal frames.
  const auto &key=binding.key_derivation;
  if(!key||!key->valid_owner(key)||!key->before_material()||!key->before_paint()||!key->position_domain()||
     !own(binding.normal_frames,key->frames())||!valid_owner(key->before_material()->canonical_paint_sampling)||
     !own(key->before_material()->canonical_uv,normal_uv_)||!own(key->before_material()->canonical_attributes,normal_uv_->origin())||
     !own(key->before_material()->uv_origin,binding.uv_origin)||!matches_current(*key->before_paint())||
     !own(key->before_paint()->mesh_owner(),attachment_->authored_geometry())||
     !own(key->position_domain()->current_geometry(),attachment_->authored_geometry()))
   throw std::invalid_argument("Canonical current key lacks its exact captured writable and normal source context");
  // Contextual proof is always real replay, including during a shallow caller
  // validation. An equal address/FrameSet or a prior successful call is not a
  // certificate for legally mutable target arrays or another owner context.
  key->validate(true);
  validate_current(*key->before_paint(),{},true);
  normal_frame_current_authority(*key->before_material(),key->before_paint()->mesh(),key->before_deformation().get());
 }
 validate({},deep);
}
void CanonicalPaintSampling::validate_current(const PaintedMesh &paint,const PaintSamplingLimits &limits,bool deep) const{
 validate(limits,deep);
 if(!matches_current(paint)||paint.source_maps()||paint.source_sampling_prepared()||paint.color_projection()||paint.normal_projection())
  throw std::invalid_argument("Canonical paint sampling lost its exact current geometry attachment");
 if(paint.baked()&&paint.baked()->normal_field&&paint.baked()->normal_field->origin()!=normal_uv_->origin())
  throw std::invalid_argument("Canonical paint borrowed another baked normal field origin");
 stop(limits);
}
CanonicalPaintTransition CanonicalPaintSampling::create(const Scene &scene,uint64_t id,
 const Mesh &target,std::span<const uint32_t> map,const PaintSamplingLimits &limits){
 stop(limits);if(!limits.max_working_bytes||limits.max_working_bytes>512*1024*1024)throw std::invalid_argument("Invalid canonical paint capture memory policy");const auto &source=scene.object(id);
 if(!source.painted||!source.volume||source.volume->in_stroke()||!source.material_bindings)
  throw std::invalid_argument("Canonical paint capture needs complete finalized source control");
 // Count decoded inline model slots before source capture or remapping arrays.
 // Equal immutable values in different tables are not a pooling witness.
 const auto entries=canonical_paint_model_entries(source.deformation.get(),source.painted->mesh(),target,map,limits.cancelled);
 preflight_canonical_paint_models(scene,id,entries,limits.cancelled);
 scene.validate();stop(limits);
 size_t reserved=0;auto charge=[&](size_t n,size_t unit){if(reserved>limits.max_working_bytes||n>(limits.max_working_bytes-reserved)/unit)throw std::length_error("Canonical paint full capture/remap exceeds aggregate working quota");reserved+=n*unit;};
 charge(1,sizeof(CanonicalPaintSampling)+2*sizeof(PolygonOrigin)+sizeof(Volume)+sizeof(DeformationState)+sizeof(PaintedMesh));
 charge(1,sizeof(SourceMaterialBindings)+64);
 // has_mtoon=false is not proof that inactive retained JSON is empty. Count
 // its actual value-copy footprint without allocating an encoded clone.
 charge(source.material_bindings->material.copy_working_bytes(limits.max_working_bytes-reserved,limits.cancelled),1);
 charge(entries,sizeof(Vec3));charge(target.positions.size(),sizeof(VertexWeights));
 charge(source.volume->layers().size(),sizeof(Volume::Layer));
 for(const auto &layer:source.volume->layers())charge(layer.name.size()+1,1);
 charge(scene.groups().size(),sizeof(SceneGroup));for(const auto &group:scene.groups())charge(group.name.size()+1,1);
 charge(source.name.size()+source.material.name.size()+2,1);
 if(source.deformation){
  if(source.deformation->skeleton)for(const auto &bone:source.deformation->skeleton->bones)charge(bone.name.size()+1,sizeof(char));
  if(source.deformation->morphs)for(const auto &t:source.deformation->morphs->targets)charge(t.name.size()+1,sizeof(char));
 }
 auto geometry_limits=limits;geometry_limits.max_working_bytes-=reserved;
 geometry_limits.max_basis_bytes=std::min(geometry_limits.max_basis_bytes,geometry_limits.max_working_bytes);
 auto geometry=stage_canonical_paint_uv(source,target,map,geometry_limits);
 auto before=std::make_shared<PolygonOrigin>(*capture_polygon_origin(scene,id));
 // The geometry factory's actual metadata snapshot is the captured before
 // Paint. Keep all other original control/rig/key/placement roots intact.
 before->paint=geometry.before_paint;
 auto model=normal_frame_remap_deformation(before->deformation,before->paint->mesh(),geometry.paint->mesh(),map,limits.cancelled);
 auto owner=std::shared_ptr<CanonicalPaintSampling>(new CanonicalPaintSampling);
 owner->before_=std::move(before);owner->normal_uv_=std::move(geometry.normal_uv);
 owner->attachment_=geometry.paint->paint_sampling_attachment();
 owner->current_sources_=std::move(geometry.current_sources);owner->self_=owner;
 owner->validate_current(*geometry.paint,limits);stop(limits);
 auto binding=std::make_shared<SourceMaterialBindings>(*source.material_bindings);
 binding->canonical_uv=owner->normal_uv_;binding->canonical_paint_sampling=owner;binding->sampling_reparameterized=true;
 binding->validate();source_paint_pairing(geometry.paint.get(),binding.get(),nullptr,true);stop(limits);
 return {std::move(owner),std::move(geometry.paint),std::move(model),std::move(binding)};
}
std::shared_ptr<const CanonicalPaintSampling> CanonicalPaintSamplingCodec::replay(
 std::shared_ptr<const PolygonOrigin> before,std::shared_ptr<const CanonicalNormalUv> uv,
 std::shared_ptr<const PaintSamplingDomain> domain,std::span<const uint32_t> map,const PaintSamplingLimits &limits){
 stop(limits);if(!before||!before->paint||!before->volume||!before->material_bindings||!uv||!uv->valid_owner(uv)||!domain)
  throw std::invalid_argument("Canonical paint replay requires complete actual before/normal/paint pools");
 SculptObject source;source.volume.reset();source.id=before->source_object;source.mesh_active=before->mesh_active;
 source.painted=before->paint->snapshot();source.material=before->material;source.deformation=before->deformation;
 source.appearance=before->appearance;source.material_bindings=before->material_bindings;source.polygon=before->polygon;
 source.retopology=before->retopology;source.shape_source=before->shape_source;source.rig_source=before->rig_source;source.bake_recipe=before->bake_recipe;
 auto actual=stage_canonical_paint_uv(source,*uv->mesh(),map,limits);
 const auto &a=*actual.normal_uv->layout(),&b=*uv->layout();
 auto own=[](const auto &x,const auto &y){return x==y&&!x.owner_before(y)&&!y.owner_before(x);};
 if(!own(actual.normal_uv->origin(),uv->origin())||a.source_vertices!=b.source_vertices||a.indices!=b.indices||a.pins!=b.pins||a.seams!=b.seams)
  throw std::invalid_argument("Canonical paint replay changed its genuine current-to-normal root correspondence");
 uv->validate_current(actual.paint->mesh(),limits.cancelled);domain->validate_current(actual.paint->mesh(),limits);
 auto bytes=[](const auto &x,const auto &y){return x.size()==y.size()&&(x.empty()||std::memcmp(x.data(),y.data(),x.size()*sizeof(x[0]))==0);};
 const auto &basis=*actual.domain->basis(),&expected=*domain->basis();
 if(actual.domain->source_corners().size()!=domain->source_corners().size()||
    !std::equal(actual.domain->source_corners().begin(),actual.domain->source_corners().end(),domain->source_corners().begin())||
    !bytes(basis.positions,expected.positions)||!bytes(basis.normals,expected.normals)||!bytes(basis.uv,expected.uv)||
    basis.indices!=expected.indices||basis.uv_pins!=expected.uv_pins||basis.uv_seams!=expected.uv_seams)
  throw std::invalid_argument("Canonical paint replay changed its actual retained writable basis/corners");
 // Drop the temporary replay grid before attaching the independently decoded
 // domain. The old basis must still pass the normal retain helper's strong
 // previous-basis checks; equal dimensions never authenticate another field.
 actual.paint.reset();actual.domain.reset();actual.before_paint.reset();
 auto paint=retain_paint_grid_borrowed(*before->paint,*uv->mesh(),domain,limits);
 auto owner=std::shared_ptr<CanonicalPaintSampling>(new CanonicalPaintSampling);
 owner->before_=std::move(before);owner->normal_uv_=std::move(uv);owner->attachment_=paint->paint_sampling_attachment();
 owner->current_sources_=std::move(actual.current_sources);owner->self_=owner;
 owner->validate_current(*paint,limits);stop(limits);return owner;
}
}
