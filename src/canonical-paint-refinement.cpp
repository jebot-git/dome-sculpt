// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-paint-sampling.hpp"
#include "refinement-geometry.hpp"
#include "polygon-conversion.hpp"
#include "paint-sampling-access.hpp"
#include "source-paint-pairing.hpp"
namespace dome::detail {namespace {
size_t refinement_model_copy_bytes(const DeformationState &d){
  // Resident/copy buffers plus bounded map-node overhead. No skin interpolation
  // is hidden here: exact dynamic skin refinement is a separate required model.
  size_t n=2*sizeof(DeformationState)+(d.pose.capacity()+d.pose.size())*sizeof(JointTransform)+(d.morph_weights.capacity()+d.morph_weights.size())*sizeof(float);
  if(d.skin){n+=(d.skin->vertices.capacity()+d.skin->vertices.size())*sizeof(VertexWeights);for(const auto &row:d.skin->vertices)n+=(row.capacity()+row.size())*sizeof(VertexWeights::value_type);}
  if(d.skeleton){n+=(d.skeleton->bones.capacity()+d.skeleton->bones.size())*sizeof(Bone);for(const auto&b:d.skeleton->bones)n+=b.name.capacity()+b.name.size()+2;if(d.skeleton->inverse_bind_override)n+=(d.skeleton->inverse_bind_override->capacity()+d.skeleton->inverse_bind_override->size())*sizeof(RigMatrix);}
  if(d.morphs){n+=(d.morphs->targets.capacity()+d.morphs->targets.size())*sizeof(MorphTarget);for(const auto&t:d.morphs->targets)n+=(t.delta.capacity()+t.delta.size())*sizeof(Vec3)+t.name.capacity()+t.name.size()+2;}
  if(d.humanoid)for(const auto &[s,v]:d.humanoid->joints)n+=2*(sizeof(std::pair<const std::string,uint32_t>)+4*sizeof(void*))+s.capacity()+s.size()+2;
  if(d.landmarks)for(const auto &[s,v]:d.landmarks->points)n+=2*(sizeof(std::pair<const std::string,Vec3>)+4*sizeof(void*))+s.capacity()+s.size()+2;
  if(d.expressions)for(const auto &[s,v]:d.expressions->visemes){n+=2*(sizeof(std::pair<const std::string,std::vector<VisemeBinding>>)+4*sizeof(void*))+s.capacity()+s.size()+2+(v.capacity()+v.size())*sizeof(VisemeBinding);for(const auto &b:v)n+=b.shape.capacity()+b.shape.size()+2;}
  return n;
}
}
CanonicalPaintGeometryTransition CanonicalPaintSampling::stage_refinement(
 const Scene &scene,uint64_t id,std::shared_ptr<const RefinementGeometry> geometry,const PaintSamplingLimits &limits){
 auto stop=[&]{if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Independent paint refinement cancelled");};stop();
 if(!limits.max_working_bytes||limits.max_working_bytes>512*1024*1024)throw std::invalid_argument("Invalid independent paint capture working policy");
 const auto &source=scene.object(id);const auto &paint=source.painted;
 if(!source.mesh_active||!paint||paint->in_stroke()||!source.volume||source.volume->in_stroke()||source.appearance||source.polygon||paint->source_maps()||paint->source_sampling_prepared()||paint->color_projection()||paint->normal_projection()||paint->surface_diagnostics().policy!=PaintUvPolicy::Strict||(source.material_bindings&&source.material_bindings->has_mtoon))
  throw std::invalid_argument("Independent refined paint needs its actual strict finalized writable control");
 if(!geometry||!geometry->valid_owner(geometry)||!geometry->matches_source(paint->mesh_owner()))throw std::invalid_argument("Independent paint capture borrowed a foreign geometry owner");
 size_t reserved=0;auto charge=[&](size_t count,size_t width){stop();if(reserved>limits.max_working_bytes||count>(limits.max_working_bytes-reserved)/width)throw std::length_error("Independent paint full capture exceeds aggregate working quota before allocation");reserved+=count*width;};
 charge(1,sizeof(CanonicalPaintSampling)+2*sizeof(PolygonOrigin)+sizeof(Volume)+sizeof(DeformationState));
 charge(paint->storage_bytes(),1);charge(geometry->working_bytes(),1);
 for(size_t copy=0;copy<2;++copy){charge(1,sizeof(PaintedMesh)+64);charge(paint->layers().size(),sizeof(PaintLayer));for(const auto &layer:paint->layers()){charge(layer.name.size()+1,1);charge(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size(),sizeof(std::shared_ptr<const PaintTile>));}}
 if(source.material_bindings){charge(1,sizeof(SourceMaterialBindings)+64);charge(source.material_bindings->material.copy_working_bytes(limits.max_working_bytes-reserved,limits.cancelled),1);}
 if(source.deformation){source.deformation->resource_entries();charge(refinement_model_copy_bytes(*source.deformation),1);}
 charge(source.name.size()+source.material.name.size()+2,1);charge(source.volume->layers().size(),sizeof(Volume::Layer));for(const auto &layer:source.volume->layers())charge(layer.name.size()+1,1);
 charge(scene.groups().size(),sizeof(SceneGroup));for(const auto &group:scene.groups())charge(group.name.size()+1,1);
 scene.validate();stop();
 auto budget=limits;budget.max_working_bytes-=reserved;budget.max_basis_bytes=std::min(budget.max_basis_bytes,budget.max_working_bytes);
 auto domain=retain_refined_paint_sampling(paint->paint_sampling_domain(),geometry,budget);
 auto before=std::make_shared<PolygonOrigin>(*capture_polygon_origin(scene,id));
 if(source.material_bindings)before->material_bindings=std::make_shared<const SourceMaterialBindings>(*source.material_bindings);
 if(source.deformation)before->deformation=std::make_shared<const DeformationState>(*source.deformation);
 auto current=retain_paint_grid_borrowed(*before->paint,*geometry->mesh(),std::move(domain),budget);
 auto owner=std::shared_ptr<CanonicalPaintSampling>(new CanonicalPaintSampling);owner->kind_=CanonicalPaintKind::IndependentGeometry;owner->self_=owner;owner->before_=std::move(before);owner->attachment_=current->paint_sampling_attachment();
 owner->validate_current(*current,limits);stop();return {std::move(owner),std::move(current)};
}
}
