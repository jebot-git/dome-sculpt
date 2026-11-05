// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-paint-sampling.hpp"
#include "canonical-paint-prospect.hpp"
#include "polygon-conversion.hpp"
#include "normal-frame-runtime.hpp"
#include "source-paint-pairing.hpp"
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
void CanonicalPaintSampling::validate_current(const PaintedMesh &paint,const PaintSamplingLimits &limits) const{
 stop(limits);if(!valid_owner(self_.lock())||!before_||!before_->paint||!before_->material_bindings||
  !normal_uv_||!normal_uv_->valid_owner(normal_uv_)||!attachment_||!matches_current(paint)||
  paint.source_maps()||paint.source_sampling_prepared()||paint.color_projection()||paint.normal_projection())
  throw std::invalid_argument("Canonical paint sampling lost its own exact before/current owners");
 const auto &b=*before_->material_bindings;
 if(normal_uv_->origin()!=b.canonical_attributes||b.normal_frames!=b.canonical_attributes->frames())
  throw std::invalid_argument("Canonical paint sampling borrowed a different normal attribute origin");
 source_paint_pairing(before_->paint.get(),before_->material_bindings.get(),before_->appearance.get(),true);
 normal_uv_->validate_current(paint.mesh(),limits.cancelled);
 if(current_sources_.size()!=paint.mesh().positions.size())throw std::invalid_argument("Canonical paint source dimensions differ");
 const auto &old=before_->paint->mesh(),&current=paint.mesh();
 if(old.indices.size()!=current.indices.size())throw std::invalid_argument("Canonical paint triangle identity differs");
 for(size_t c=0;c<current.indices.size();++c){if((c&1023)==0)stop(limits);
  if(current_sources_.at(current.indices[c])!=old.indices[c])throw std::invalid_argument("Canonical paint ordered source corners differ");}
 attachment_->domain()->validate_current(current,limits);
 if(paint.baked()&&paint.baked()->normal_field&&paint.baked()->normal_field->origin()!=normal_uv_->origin())
  throw std::invalid_argument("Canonical paint borrowed another baked normal field origin");
 if(deformation_)deformation_->validate(current);stop(limits);
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
 owner->attachment_=geometry.paint->paint_sampling_attachment();owner->deformation_=std::move(model);
 owner->current_sources_=std::move(geometry.current_sources);owner->self_=owner;
 owner->validate_current(*geometry.paint,limits);stop(limits);
 auto current_deformation=owner->deformation_;
 return {std::move(owner),std::move(geometry.paint),std::move(current_deformation)};
}
}
