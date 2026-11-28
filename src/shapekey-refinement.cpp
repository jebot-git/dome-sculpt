// SPDX-License-Identifier: GPL-3.0-or-later
#include "shapekey-refinement.hpp"
#include "dome/material-bindings.hpp"
#include "key-mirror-refinement.hpp"
#include "normal-bake-frames.hpp"
#include "normal-frame-runtime.hpp"
#include "polygon-conversion.hpp"
#include "polygon-paint.hpp"
#include "shapekey-attributes.hpp"

namespace dome::detail {
namespace {
void stop(const std::function<bool()> &cancelled){
  if(cancelled&&cancelled())throw std::runtime_error("Mirror refinement cancelled");
}

template<class Value,class Read>
Value interpolate(const MirrorSurfacePoint &at,Read read) {
  for(size_t k=0;k<3;++k)if(at.barycentric[k]==1)return read(at.triangle*3+k);
  std::array<double,std::tuple_size_v<Value>> sum{};
  for(size_t k=0;k<3;++k){
    const auto value=read(at.triangle*3+k);
    for(size_t channel=0;channel<sum.size();++channel)sum[channel]+=value[channel]*at.barycentric[k];
  }
  Value result{};
  for(size_t channel=0;channel<sum.size();++channel)result[channel]=float(sum[channel]);
  return result;
}

template<class Read>
Vec3 vector_at(const MirrorSurfacePoint &at,Read read){
  const auto v=interpolate<std::array<float,3>>(at,[&](size_t c){
    const auto value=read(c);return std::array{value.x,value.y,value.z};
  });
  return {v[0],v[1],v[2]};
}

void refine_uv_flags(const Mesh &source,const KeyMirrorRefinement &refined,Mesh &mesh,
    const std::function<bool()> &cancelled){
  if(!source.uv_pins.empty()){
    mesh.uv_pins.resize(refined.corners.size());
    for(size_t c=0;c<refined.corners.size();++c){
      if((c&1023)==0)stop(cancelled);
      const auto &sample=refined.corners[c].current;
      for(size_t k=0;k<3;++k)if(sample.barycentric[k]==1)
        mesh.uv_pins[c]=source.uv_pins[source.indices[sample.triangle*3+k]];
    }
  }
  if(!source.uv_seams.empty()){
    mesh.uv_seams.resize(refined.corners.size());
    for(size_t c=0;c<refined.corners.size();++c){
      if((c&1023)==0)stop(cancelled);
      const auto &a=refined.corners[c].current;
      const auto &b=refined.corners[c-c%3+(c+1)%3].current;
      for(size_t k=0;k<3;++k){
        const auto opposite=(k+2)%3;
        if(a.barycentric[opposite]==0&&b.barycentric[opposite]==0)
          mesh.uv_seams[c]=source.uv_seams[a.triangle*3+k];
      }
    }
  }
}

void admit(const SculptObject &source){
  const auto &paint=*source.painted;
  const auto binding=source.material_bindings;
  if(source.appearance||source.polygon||source.texture_sets||source.retopology||source.bake_recipe||source.rig_source||
     paint.in_stroke()||paint.source_maps()||paint.baked()||paint.paint_sampling_domain()||
     paint.normal_projection()||paint.color_projection()||paint.surface_diagnostics().policy!=PaintUvPolicy::Strict||
     source.deformation->skin||source.deformation->source_skin_instance||
     (binding&&(binding->has_mtoon||binding->sampling_reparameterized||binding->key_derivation||binding->canonical_attributes||binding->canonical_uv||
       binding->normal_preparation||binding->color_preparation||binding->native_paint_sampling||
       binding->native_paint_frames||binding->canonical_paint_sampling||binding->polygon_surface_transfer||
       (binding->uv_origin&&(!binding->uv_origin->paint||binding->uv_origin->appearance||!binding->uv_origin->creator_kind.empty()||
                            binding->uv_origin->paint->source_maps())))))
    throw std::invalid_argument("Refined surface Mirror currently needs a native unskinned mesh with writable UVs and plain paint layers");
}

std::shared_ptr<NormalFrames> refine_frame(const NormalFrames &before,const KeyMirrorRefinement &refined,
    const Mesh &mesh,const std::function<bool()> &cancelled){
  if(before.source_vertex_count||before.source_corner_count||before.transport==NormalFrameTransport::CanonicalMikk)
    throw std::invalid_argument("Refined surface Mirror requires native artist-frame coordinates");
  auto after=std::make_shared<NormalFrames>();
  after->transport=before.transport;
  after->generated_tangents=before.generated_tangents;
  after->generated_uv=before.generated_uv;
  after->origin_vertex_count=uint32_t(mesh.positions.size());
  after->origin_vertices=mesh.indices;
  auto sample=[&](const std::vector<Vec3> &field){
    std::vector<Vec3> result;
    if(field.empty())return result;
    result.reserve(refined.corners.size());
    for(const auto &corner:refined.corners){
      if((result.size()&1023)==0)stop(cancelled);
      result.push_back(vector_at(corner.current,[&](size_t c){return field.at(c);}));
    }
    return result;
  };
  after->normals=sample(before.normals);
  if(!before.tangents.empty()){
    after->tangents.reserve(refined.corners.size());
    for(const auto &corner:refined.corners){
      if((after->tangents.size()&1023)==0)stop(cancelled);
      auto t=interpolate<std::array<float,4>>(corner.current,[&](size_t c){return before.tangents.at(c);});
      t[3]=before.tangents.at(corner.current.triangle*3)[3];
      after->tangents.push_back(t);
    }
  }
  for(const auto &target:before.targets)after->targets.push_back({target.id,sample(target.normals),sample(target.tangents)});
  after->validate();
  return after;
}
}

ShapeKeyResult author_refined_shapekey(Scene &scene,uint64_t object,const ShapeKeyEdit &edit,
    std::string name,const std::function<bool()> &cancelled){
  const auto &source=scene.object(object);
  admit(source);
  const auto &rest=source.painted->mesh();
  const auto &before=*source.deformation->morphs;
  const auto slot=before.targets.size();
  const auto old=source.material_bindings?source.material_bindings->normal_frames:nullptr;
  if(old)normal_frame_current_authority(*source.material_bindings,rest,source.deformation.get(),cancelled);
  else source.deformation->validate(rest);
  const auto refined=refine_reflected_key(source.painted->mesh_owner(),{edit.mirror_plane},
      edit.mirror_tolerance,{128*1024*1024,64*1024*1024,cancelled});
  const auto corners=refined.corners.size();
  if(corners>(128*1024*1024)/(sizeof(MirrorRefinementCorner)+64+(slot+1)*sizeof(Vec3)*10))
    throw std::length_error("Refined Mirror mesh and key data exceed the authoring quota");

  Mesh mesh;
  std::vector<uint32_t> parents;
  mesh.positions.reserve(corners);mesh.normals.reserve(corners);mesh.uv.reserve(corners);
  mesh.indices.reserve(corners);parents.reserve(corners/3);
  for(const auto &corner:refined.corners){
    const auto at=mesh.positions.size();
    if((at&1023)==0)stop(cancelled);
    if(at%3==0)parents.push_back(corner.current.triangle);
    mesh.positions.push_back(corner.position);
    mesh.indices.push_back(uint32_t(at));
    mesh.normals.push_back(vector_at(corner.current,[&](size_t c){return rest.normals[rest.indices[c]];}));
    mesh.uv.push_back(interpolate<std::array<float,2>>(corner.current,[&](size_t c){return rest.uv[rest.indices[c]];}));
  }
  refine_uv_flags(rest,refined,mesh,cancelled);
  PaintSurfaceOptions paint_options;paint_options.cancelled=cancelled;
  auto paint=refined_paint_geometry(*source.painted,std::move(mesh),parents,paint_options);
  const auto &basis=paint->mesh();

  auto deformation=std::make_shared<DeformationState>(*source.deformation);
  deformation->morphs=MorphSet(basis);
  deformation->source=deformation->morphs->topology;
  auto transfer=[&](const MorphTarget &target,bool mirror,std::string title){
    MorphTarget result{std::move(title),{}};
    result.delta.reserve(corners);
    for(const auto &corner:refined.corners){
      if((result.delta.size()&1023)==0)stop(cancelled);
      auto d=vector_at(mirror?corner.reflected:corner.current,[&](size_t c){return target.delta[rest.indices[c]];});
      if(mirror)d.x=-d.x;
      result.delta.push_back(d);
    }
    return result;
  };
  for(const auto &target:before.targets)deformation->morphs->targets.push_back(transfer(target,false,target.name));
  auto binding=source.material_bindings?std::make_shared<SourceMaterialBindings>(*source.material_bindings):std::make_shared<SourceMaterialBindings>();
  auto origin=std::make_shared<SourceUvOrigin>();
  origin->paint=paint->snapshot();
  origin->deformation=std::make_shared<const DeformationState>(*deformation);
  binding->uv_origin=origin;
  binding->has_mtoon=false;
  binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;
  if(!source.material_bindings){
    binding->native_reference={source.material.color,source.material.emissive,int(source.material.alpha),
                               source.material.alpha_cutoff,source.material.double_sided};
  }
  const auto frames=std::make_shared<NormalFrameSet>(old?*old:*normal_bake_frame_candidate(basis,deformation.get(),nullptr,
      {128*1024*1024,128*1024*1024,cancelled}).frames);
  if(frames->targets.size()!=slot||frames->next_target==UINT64_MAX)
    throw std::invalid_argument("Refined Mirror needs an editable native key registry");
  for(auto &identity:frames->targets){
    const auto inherited=std::ranges::find(frames->current_targets,identity.id)-frames->current_targets.begin();
    identity.positions=deformation->morphs->targets.at(size_t(inherited)).delta;
  }
  const auto id=frames->next_target++;
  auto mirrored=transfer(before.targets.at(edit.target),true,std::move(name));
  std::vector<Vec3> endpoint=basis.positions;
  for(size_t i=0;i<corners;++i){if((i&1023)==0)stop(cancelled);endpoint[i]+=mirrored.delta[i];}
  const auto source_pbr=frames->pbr,source_mtoon=frames->mtoon,source_canonical=frames->canonical;
  auto role=[&](const std::shared_ptr<const NormalFrames> &source_frame)->std::shared_ptr<const NormalFrames>{
    if(!source_frame)return {};
    auto frame=old?refine_frame(*source_frame,refined,basis,cancelled):std::make_shared<NormalFrames>(*source_frame);
    frame->targets.push_back(author_key_attributes(basis,*frame,nullptr,basis.positions,endpoint,id,cancelled));
    frame->validate();
    return frame;
  };
  frames->pbr=role(source_pbr);
  frames->mtoon=source_mtoon==source_pbr?frames->pbr:role(source_mtoon);
  frames->canonical=source_canonical==source_pbr?frames->pbr:source_canonical==source_mtoon?frames->mtoon:role(source_canonical);
  frames->current_targets.push_back(id);
  frames->targets.push_back({id,{},{},mirrored.delta});
  binding->normal_frames=frames;
  deformation->morphs->targets.push_back(std::move(mirrored));
  if(!deformation->morph_weights.empty())deformation->morph_weights.push_back(0);

  normal_frame_current_authority(*binding,basis,deformation.get(),cancelled);
  Scene next=scene;
  auto &output=next.object(object);
  output.shape_source=capture_polygon_origin(scene,object);
  output.painted=std::move(paint);
  output.material_bindings=std::move(binding);
  output.deformation=std::move(deformation);
  next.validate();
  stop(cancelled);
  scene=std::move(next);
  return {slot,true};
}
}
