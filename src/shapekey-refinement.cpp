// SPDX-License-Identifier: GPL-3.0-or-later
#include "shapekey-refinement.hpp"
#include "bake-canonical-frame.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/material-bindings.hpp"
#include "refined-mirror-topology.hpp"
#include "normal-bake-frames.hpp"
#include "normal-frame-runtime.hpp"
#include "polygon-conversion.hpp"
#include "polygon-paint.hpp"
#include "shapekey-attributes.hpp"
#include <bit>
#include <map>

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

struct VertexSupport {
  uint32_t vertex=UINT32_MAX;
  double weight=0;
  auto operator<=>(const VertexSupport &)const=default;
};
using SurfaceSupport=std::array<VertexSupport,3>;

SurfaceSupport support(const Mesh &source,const MirrorSurfacePoint &point){
  SurfaceSupport result;
  for(size_t k=0;k<3;++k)if(point.barycentric[k]!=0)
    result[k]={source.indices[point.triangle*3+k],point.barycentric[k]};
  std::ranges::sort(result,{},&VertexSupport::vertex);
  return result;
}

struct RefinedVertex {
  std::array<uint32_t,3> position;
  // Two sides may share a geometric edge but carry independent POSITION
  // fields. The original vertex IDs preserve both sides of such a seam.
  SurfaceSupport current,reflected;
  auto operator<=>(const RefinedVertex &)const=default;
};

void refine_uv_flags(const Mesh &source,const KeyMirrorRefinement &refined,Mesh &mesh,
    const std::function<bool()> &cancelled){
  if(!source.uv_pins.empty()){
    mesh.uv_pins.resize(mesh.positions.size());
    for(size_t c=0;c<refined.corners.size();++c){
      if((c&1023)==0)stop(cancelled);
      const auto &sample=refined.corners[c].current;
      for(size_t k=0;k<3;++k)if(sample.barycentric[k]==1)
        mesh.uv_pins[mesh.indices[c]]=source.uv_pins[source.indices[sample.triangle*3+k]];
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

SkinWeights refine_skin(const DeformationState &before,const KeyMirrorRefinement &refined,
    std::span<const uint32_t> representatives,const std::function<bool()> &cancelled){
  SkinWeights result;
  result.vertices.reserve(representatives.size());
  const auto &rows=before.skin->vertices;
  const auto &indices=refined.source->indices;
  const size_t bones=before.skeleton->bones.size();
  for(const auto corner:representatives){
    if((result.vertices.size()&255)==0)stop(cancelled);
    const auto &point=refined.corners[corner].current;
    const auto face=point.triangle*3;
    const auto exact=std::ranges::find(point.barycentric,1.);
    if(exact!=point.barycentric.end()){
      result.vertices.push_back(rows[indices[face+size_t(exact-point.barycentric.begin())]]);
      continue;
    }
    // Interpolate this side's original face. Closest-point transfer can pick
    // the other face of a seam, and pruning changes the authored weight field.
    std::array<double,256> weights{};
    for(size_t k=0;k<3;++k){
      const auto fraction=point.barycentric[k];
      if(fraction==0)continue;
      for(const auto &entry:rows[indices[face+k]])
        weights[entry.joint]+=fraction*entry.weight;
    }
    std::array<Influence,32> row;
    size_t count=0;
    for(size_t joint=0;joint<bones;++joint){
      const auto weight=float(weights[joint]);
      if(weight<=0)continue;
      if(count==row.size())
        throw std::invalid_argument("Refined surface Mirror would require more than 32 influences at a new vertex");
      row[count++]={uint32_t(joint),weight};
    }
    result.vertices.emplace_back(row.begin(),row.begin()+count);
  }
  return result;
}

void admit(const SculptObject &source){
  const auto &paint=*source.painted;
  const auto binding=source.material_bindings;
  if(source.appearance||source.polygon||source.texture_sets||source.retopology||
     paint.in_stroke()||paint.source_maps()||paint.paint_sampling_domain()||
     paint.normal_projection()||paint.color_projection()||paint.surface_diagnostics().policy!=PaintUvPolicy::Strict||
     source.deformation->source_skin_instance||
     (binding&&(binding->has_mtoon||binding->sampling_reparameterized||binding->key_derivation||binding->canonical_attributes||binding->canonical_uv||
       binding->normal_preparation||binding->color_preparation||binding->native_paint_sampling||
       binding->native_paint_frames||binding->canonical_paint_sampling||binding->polygon_surface_transfer||
       (binding->uv_origin&&(!binding->uv_origin->paint||binding->uv_origin->appearance||!binding->uv_origin->creator_kind.empty()||
                            binding->uv_origin->paint->source_maps())))))
    throw std::invalid_argument("Refined surface Mirror requires a native mesh with writable UVs");
}

std::shared_ptr<NormalFrames> refine_frame(const NormalFrames &before,const KeyMirrorRefinement &refined,
    const Mesh &mesh,const std::function<bool()> &cancelled){
  if(before.source_vertex_count||before.source_corner_count)
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

IndexedMirrorRefinement index_mirror_refinement(const KeyMirrorRefinement &refined,
    const std::function<bool()> &cancelled,size_t max_bytes){
  const auto &rest=*refined.source;
  const auto corners=refined.corners.size();
  const size_t bytes_per_corner=sizeof(RefinedVertex)+4*sizeof(void *)+3*sizeof(uint32_t)+
      2*sizeof(Vec3)+sizeof(std::array<float,2>);
  if(corners>max_bytes/bytes_per_corner)
    throw std::length_error("Indexed Mirror topology exceeds its working budget");
  IndexedMirrorRefinement result;
  auto &[mesh,parents,representatives]=result;
  mesh.positions.reserve(corners);mesh.normals.reserve(corners);mesh.uv.reserve(corners);
  mesh.indices.reserve(corners);parents.reserve(corners/3);representatives.reserve(corners);
  std::map<RefinedVertex,uint32_t> vertices;
  for(size_t at=0;at<corners;++at){
    if((at&1023)==0)stop(cancelled);
    const auto &corner=refined.corners[at];
    const auto p=corner.position;
    if(at%3==0)parents.push_back(corner.current.triangle);
    const RefinedVertex identity{{std::bit_cast<uint32_t>(p.x),std::bit_cast<uint32_t>(p.y),
      std::bit_cast<uint32_t>(p.z)},support(rest,corner.current),support(rest,corner.reflected)};
    const auto [vertex,inserted]=vertices.try_emplace(identity,uint32_t(mesh.positions.size()));
    mesh.indices.push_back(vertex->second);
    if(!inserted)continue;
    representatives.push_back(uint32_t(at));
    mesh.positions.push_back(p);
    mesh.normals.push_back(vector_at(corner.current,[&](size_t c){return rest.normals[rest.indices[c]];}));
    mesh.uv.push_back(interpolate<std::array<float,2>>(corner.current,[&](size_t c){return rest.uv[rest.indices[c]];}));
  }
  return result;
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
  bool baked_normals=false;
  if(const auto baked=source.painted->baked()){
    if(baked->normal_interpretation!=BakedNormalInterpretation::CanonicalMikk||
       baked->color_interpretation!=BakedColorInterpretation::VertexComponent||
       baked->normal_authority||baked->normal_field||!baked->maps.mtoon_normal.rgba.empty())
      throw std::invalid_argument("Refined surface Mirror requires ordinary native Bake maps on the same UV grid");
    baked_normals=!baked->maps.normal.rgba.empty();
    if(baked_normals&&(!old||!old->canonical||
       !constant_bake_frame(*old->canonical,rest,source.deformation.get(),true,cancelled)))
      throw std::invalid_argument("Refined surface Mirror needs a retained constant tangent frame for baked normals");
  }
  const auto refined=refine_authoring_key(source.painted->mesh_owner(),{edit.mirror_plane},
      edit.mirror_tolerance,{128*1024*1024,64*1024*1024,cancelled});
  const auto corners=refined.corners.size();
  const auto &original_deformation=*source.deformation;
  const auto skin_bytes=original_deformation.skin?sizeof(VertexWeights)+
      sizeof(Influence)*std::min(size_t(32),original_deformation.skeleton->bones.size()):0;
  const size_t indexing_bytes=sizeof(RefinedVertex)+sizeof(uint32_t)+4*sizeof(void *);
  if(corners>(128*1024*1024)/(sizeof(MirrorRefinementCorner)+indexing_bytes+64+
                            (slot+1)*sizeof(Vec3)*10+skin_bytes*2))
    throw std::length_error("Refined Mirror mesh and key data exceed the authoring quota");

  auto [mesh,parents,representatives]=index_mirror_refinement(refined,cancelled);
  refine_uv_flags(rest,refined,mesh,cancelled);
  PaintSurfaceOptions paint_options;paint_options.cancelled=cancelled;
  auto paint=refined_paint_geometry(*source.painted,std::move(mesh),parents,paint_options);
  const auto &basis=paint->mesh();

  auto deformation=std::make_shared<DeformationState>(*source.deformation);
  deformation->morphs=MorphSet(basis);
  deformation->source=deformation->morphs->topology;
  if(original_deformation.skin)deformation->skin=refine_skin(original_deformation,refined,representatives,cancelled);
  auto transfer=[&](const MorphTarget &target,bool mirror,std::string title){
    MorphTarget result{std::move(title),{}};
    result.delta.reserve(representatives.size());
    for(const auto c:representatives){
      if((result.delta.size()&1023)==0)stop(cancelled);
      const auto &corner=refined.corners[c];
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
  for(size_t i=0;i<endpoint.size();++i){if((i&1023)==0)stop(cancelled);endpoint[i]+=mirrored.delta[i];}
  if(baked_normals){
    auto geometry=basis;
    geometry.positions=endpoint;
    geometry.validate();
  }
  const auto source_pbr=frames->pbr,source_mtoon=frames->mtoon,source_canonical=frames->canonical;
  auto role=[&](const std::shared_ptr<const NormalFrames> &source_frame)->std::shared_ptr<const NormalFrames>{
    if(!source_frame)return {};
    auto frame=old?refine_frame(*source_frame,refined,basis,cancelled):std::make_shared<NormalFrames>(*source_frame);
    // Captured Bake frames own constant authored directions with zero key
    // deltas. Reflect zero exactly and keep the texture's tangent basis.
    if(baked_normals&&(source_frame==old->canonical||
       constant_bake_frame(*source_frame,rest,source.deformation.get(),false,cancelled)))
      frame->targets.push_back({id,{},{}});
    else frame->targets.push_back(author_key_attributes(basis,*frame,nullptr,basis.positions,endpoint,id,cancelled));
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
  if(source.bake_recipe&&(source.bake_recipe->cage||source.bake_recipe->settings.corresponding_topology)&&
      original_deformation.source!=deformation->source){
    auto recipe=std::make_shared<ObjectBakeRecipe>(*source.bake_recipe);
    recipe->refinement=BakeTopologyRefinement{original_deformation.source,deformation->source,
                                             edit.mirror_plane,edit.mirror_tolerance};
    recipe->validate();
    output.bake_recipe=std::move(recipe);
  }
  output.painted=std::move(paint);
  output.material_bindings=std::move(binding);
  output.deformation=std::move(deformation);
  next.validate();
  stop(cancelled);
  scene=std::move(next);
  return {slot,true};
}
}
