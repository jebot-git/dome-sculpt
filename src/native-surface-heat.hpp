// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-anatomical-weights.hpp"
#include "visibility-heat-bind.hpp"
#include "humanoid-heat-sources.hpp"
#include "humanoid-heat-domains.hpp"
#include "current-humanoid-heat-domains.hpp"
#include "normal-frame-runtime.hpp"
namespace dome::detail {
// Private owning source-informed SURFACE heat route. No public method/default
// redirection. Existing retention and lease primitives keep their exact meaning.
struct NativeSurfaceHeatOptions:NativeAnatomicalOptions {
  WeightLimits limits{};
  float seed_distance=.3f;
};
struct NativeSurfaceHeatReport {
  struct Part {uint64_t object=0;size_t work=0,workspace=0,closed=0,nonembedded=0;double residual=0,partition=0;};
  size_t work=0,workspace=0,retained_input=0,total_admission=0,changed=0;
  std::vector<Part>parts;
};
class NativeSurfaceHeatPrepared;
class NativeSurfaceHeatRequest {
  Scene input_;
  std::vector<uint64_t>targets_,support_;
  uint64_t reference_=0;
  bool source_head_=false;
  NativeSurfaceHeatOptions options_;
  NativeWeightRetention retained_;
  size_t capture_bytes_=0;
  NativeAnatomicalLease lease_;
  std::shared_ptr<const uint64_t>identity_;
  friend NativeSurfaceHeatRequest capture_native_surface_heat(const Scene&,std::span<const uint64_t>,uint64_t,const NativeSurfaceHeatOptions&);
  friend NativeSurfaceHeatPrepared prepare_native_surface_heat(const NativeSurfaceHeatRequest&,const std::function<bool()>&);
  friend bool adopt_native_surface_heat(Scene&,const NativeSurfaceHeatRequest&,NativeSurfaceHeatPrepared&&);
public:
  bool matches(const Scene&s)const{return lease_.matches(s);}
};
class NativeSurfaceHeatPrepared {
  struct Change {uint64_t object;std::shared_ptr<const DeformationState>deformation;std::shared_ptr<const PolygonOrigin>origin;};
  std::vector<Change>changes_;
  NativeSurfaceHeatReport report_;
  std::shared_ptr<const uint64_t>identity_;
  friend NativeSurfaceHeatPrepared prepare_native_surface_heat(const NativeSurfaceHeatRequest&,const std::function<bool()>&);
  friend bool adopt_native_surface_heat(Scene&,const NativeSurfaceHeatRequest&,NativeSurfaceHeatPrepared&&);
public:
  const NativeSurfaceHeatReport&report()const noexcept{return report_;}
};
inline NativeSurfaceHeatRequest capture_native_surface_heat(const Scene&source,
 std::span<const uint64_t>selected,uint64_t reference,const NativeSurfaceHeatOptions&options={}){
  if(!std::isfinite(options.seed_distance)||options.seed_distance<=0||options.seed_distance>10000||!options.limits.max_influences||options.limits.max_influences>32||!std::isfinite(options.limits.prune)||options.limits.prune<0||options.limits.prune>=1)throw std::invalid_argument("Surface heat seed/influence/prune bounds");
  if(selected.empty()||selected.size()>64||source.objects().size()>128||source.groups().size()>128||!options.maximum_work||options.maximum_work>500'000'000||options.maximum_workspace<65536||options.maximum_workspace>256u*1024u*1024u||options.maximum_total<options.maximum_workspace||options.maximum_total>512u*1024u*1024u||options.locks.size()>256||std::any_of(options.locks.begin(),options.locks.end(),[](auto x){return x>1;}))throw std::invalid_argument("Native surface heat owning options/domain");
  // Pure bounded scan; no user callback and no Scene/JSON snapshot allocation
  // before the exact retained closure plus full working cap has been admitted.
  const auto retained=native_weight_retention(source,options.maximum_total-options.maximum_workspace,options.maximum_work);
  geometry_weight_authoring::Bytes capture{65536,options.maximum_workspace};
  weight_surface::Budget capture_work{retained.work,options.maximum_work,{}};
  capture.add(source.objects().size(),2048);capture.add(source.groups().size(),4096);capture.add(options.locks.size()+selected.size()*sizeof(uint64_t));
  for(const auto&o:source.objects()){
    if(!o.volume||o.volume->in_stroke()||(o.mesh_active&&!o.painted)||(o.painted&&o.painted->in_stroke()))throw std::invalid_argument("Native surface heat capture requires complete stable objects");
    capture.text(o.name);capture.text(o.material.name);capture_work.step(512+4*(o.name.size()+o.material.name.size()));
    if(o.painted)for(const auto&l:o.painted->layers()){capture_work.step_items(l.tiles.size()+l.pbr_tiles.size()+l.emission_tiles.size(),4);capture_work.step(l.name.size()*4+16);}
    capture_work.step_items(o.volume->layers().size(),64);
  }
  for(const auto&g:source.groups()){capture.text(g.name);capture_work.step(64+g.name.size()*4);if(g.humanoid){capture.text(g.humanoid->baseline);capture_work.step(g.humanoid->baseline.size()*4);}}
  if(capture_work.used>=capture_work.maximum)throw std::length_error("Native surface heat capture exhausted work before staging");
  NativeSurfaceHeatRequest request;request.capture_bytes_=capture.used;
  request.retained_=retained;request.retained_.work=capture_work.used;request.options_=options;request.reference_=reference;request.identity_=std::make_shared<const uint64_t>(0);
  std::set<uint64_t> unique;
  for(auto id:selected){const auto&o=source.object(id);if(!unique.insert(id).second||!o.visible||o.locked||!o.mesh_active||!o.painted||!o.volume||o.painted->in_stroke()||o.volume->in_stroke())throw std::invalid_argument("Choose explicit editable anatomical targets");request.targets_.push_back(id);}
  const auto&ref=source.object(reference);
  if(!ref.mesh_active||!ref.painted||!ref.deformation||!ref.deformation->skeleton)throw std::invalid_argument("Choose the current native humanoid reference");
  if(!options.locks.empty()&&options.locks.size()!=ref.deformation->skeleton->bones.size())throw std::invalid_argument("Native surface heat lock/joint identity");
  // Select the actual current source instance from explicit targets; foreign
  // instances/objects stay in the full lease and owning resource admission.
  // Prospective input policy, never a retry after a failed domain or solve.
  // A complete authenticated humanoid source uses its existing geometry-only
  // head/ocular domain; ordinary native geometry uses each actual requested
  // one-owner surface and the same exact visibility/screened energy.
  bool any_source=false;for(auto id:selected){capture_work.step(16);any_source|=bool(source.object(id).appearance);}
  request.source_head_=bool(ref.deformation->humanoid)&&any_source&&
    ref.deformation->humanoid->joints.contains("head")&&ref.deformation->humanoid->joints.contains("leftEye")&&ref.deformation->humanoid->joints.contains("rightEye");
  if(request.source_head_){auto domain=native_anatomical_domain(source,selected,capture_work);request.support_=std::move(domain.objects);}
  else request.support_=request.targets_;
  request.retained_.work=capture_work.used;
  for(auto id:request.targets_)if(std::find(request.support_.begin(),request.support_.end(),id)==request.support_.end())throw std::invalid_argument("Native surface heat target lacks current source correspondence");
  if(!options.locks.empty())for(auto id:request.targets_){const auto&o=source.object(id);if(!o.deformation||!o.deformation->skin||!o.deformation->skeleton)throw std::invalid_argument("Native surface heat locks require actual current skin authority");}
  request.lease_.active=source.active().id;request.lease_.groups=source.groups();
  for(auto&g:request.lease_.groups)if(g.humanoid)g.humanoid=std::make_shared<HumanoidControls>(*g.humanoid);
  request.input_=source;
  for(const auto&o:source.objects()){
    NativeAnatomicalLease::Object lease;
    lease.metadata=o;lease.paint=o.painted;lease.volume=o.volume;lease.world=source.world_matrix(o.id).matrix();
    if(o.painted){lease.mesh=o.painted->mesh_owner();lease.paint_revision=o.painted->revision();}
    if(o.volume){lease.geometry=o.volume->geometry_owner();lease.volume_revision=o.volume->revision();}
    auto&detached=request.input_.object(o.id);
    if(o.painted)detached.painted=NativeWeightPaintAccess::snapshot(*o.painted);
    if(o.volume)detached.volume=std::make_shared<Volume>(o.volume->geometry_snapshot());
    lease.metadata.painted=detached.painted;lease.metadata.volume=detached.volume;
    request.lease_.objects.push_back(std::move(lease));
  }
  if(!request.lease_.matches(source))throw std::runtime_error("Native surface heat source changed during capture");
  return request;
}
inline NativeSurfaceHeatPrepared prepare_native_surface_heat(const NativeSurfaceHeatRequest&request,
 const std::function<bool()>&cancelled={}){
  using geometry_weight_authoring::Bytes;
  const auto&input=request.input_;const auto&options=request.options_;
  weight_surface::Budget work{request.retained_.work,options.maximum_work,cancelled};work.stop();
  Bytes bytes{request.capture_bytes_,options.maximum_workspace};
  bytes.add(input.objects().size(),sizeof(SculptObject)+512);bytes.add(input.groups().size(),sizeof(SceneGroup)+512);
  bytes.add(request.targets_.size(),sizeof(NativeSurfaceHeatReport::Part)+256);
  const auto&reference=input.object(request.reference_);const auto&ref=*reference.deformation;
  weight_surface::validate_rest_deformation(ref,reference.painted->mesh(),work);
  // Plan all staged current rig/morph/output storage before any copies. No
  // solved skin values from the reference are consulted by field construction.
  for(auto id:request.support_){const auto&o=input.object(id);work.step(256);
    if(o.deformation){geometry_weight_authoring::deformation_copy_bytes(*o.deformation,bytes,work);weight_surface::validate_rest_deformation(*o.deformation,o.painted->mesh(),work);}
    else bytes.add(sizeof(DeformationState));
    geometry_weight_authoring::deformation_copy_bytes(ref,bytes,work);
    bytes.add(o.painted->mesh().positions.size(),sizeof(VertexWeights)+options.limits.max_influences*sizeof(Influence)+64);
    if(!o.rig_source){bytes.add(sizeof(PolygonOrigin)+sizeof(PaintedMesh)+sizeof(Volume)+4096);bytes.add(o.painted->storage_bytes());}
  }
  const size_t fixed_bytes=bytes.used;
  if(fixed_bytes>=options.maximum_workspace-65536)throw std::length_error("Native surface heat output staging leaves no field workspace");
  Scene support=input;
  const auto reference_world=input.world_matrix(reference.id);
  auto shift=AffineTransform{}.matrix();for(size_t k=0;k<3;++k)shift[12+k]=-reference_world.matrix()[12+k];
  const AffineTransform origin(shift),world_reference=origin*reference_world;
  // Reframe the actual native rest/pose/IBM authority, never evaluated vertices.
  auto rig_at=[&](const AffineTransform&change){SkinGeometryWork geometry(work.maximum-work.used,cancelled);auto before=geometry.remaining();auto rig=skin_reframe(*ref.skeleton,change,geometry);work.step(before-geometry.remaining());return rig;};
  const auto world_rig=world_reference.matrix()==AffineTransform{}.matrix()?*ref.skeleton:rig_at(world_reference);
  for(auto id:request.support_){const auto&o=input.object(id);work.stop();
    auto d=o.deformation?std::make_shared<DeformationState>(*o.deformation):std::make_shared<DeformationState>(o.painted->mesh());
    const auto target_world=input.world_matrix(id);const bool same=target_world.matrix()==reference_world.matrix();
    auto local_rig=same?*ref.skeleton:rig_at(target_world.inverse()*reference_world);
    if(d->skeleton){work.step_items(local_rig.bones.size(),2048);if(d->skeleton->encode()!=local_rig.encode())throw std::invalid_argument("Surface heat target has different actual native rest/joint identity");}
    else{
      d->skeleton=std::move(local_rig);d->pose=ref.pose;
      if(!d->humanoid)d->humanoid=ref.humanoid;
      if(!d->landmarks&&ref.landmarks){if(same)d->landmarks=ref.landmarks;else{SkinGeometryWork geometry(work.maximum-work.used,cancelled);auto before=geometry.remaining();d->landmarks=skin_reframe_landmarks(*ref.landmarks,target_world.inverse()*reference_world,geometry);work.step(before-geometry.remaining());}}
    }
    if(request.source_head_&&(!d->humanoid||d->humanoid->joints!=ref.humanoid->joints))throw std::invalid_argument("Surface heat current humanoid joint identities differ");
    support.object(id).deformation=std::move(d);
  }
  HumanoidHeatDomains anatomy;
  if(request.source_head_)anatomy=source_humanoid_heat_domains(support,request.support_,request.reference_,work,std::min(size_t(128u*1024u*1024u),options.maximum_workspace-fixed_bytes));
  else if(ref.humanoid&&ref.humanoid->joints.contains("head")&&ref.humanoid->joints.contains("leftEye")&&ref.humanoid->joints.contains("rightEye"))anatomy=current_humanoid_heat_domains(support,request.support_,request.reference_,work,std::min(size_t(128u*1024u*1024u),options.maximum_workspace-fixed_bytes));
  else{for(auto id:request.support_){work.step(32);anatomy.parts.push_back({id,input.object(id).painted->mesh_owner(),{},false,false});}anatomy.forecast_bytes=65536+anatomy.parts.capacity()*sizeof(HumanoidHeatDomains::Part);}
  // Analysis temporary storage is released by its factory; retained per-row
  // boundary data stays charged alongside output and one active field.
  const size_t boundary_bytes=anatomy.forecast_bytes;
  bytes.add(boundary_bytes);
  std::vector<HeatSourceSegment>profile;
  if(ref.humanoid)profile=humanoid_heat_sources(world_rig,*ref.humanoid,work);
  else{const auto segments=world_rig.segments();profile.reserve(world_rig.bones.size());for(uint32_t j=0;j<world_rig.bones.size();++j)if(world_rig.bones[j].deform){work.step(8);const auto&a=segments[j][0],&b=segments[j][1];profile.push_back({{a.x,a.y,a.z},{b.x,b.y,b.z},j});}}
  bytes.add(profile.size(),sizeof(HeatSourceSegment)+64);
  NativeSurfaceHeatPrepared result;result.identity_=request.identity_;
  result.report_.retained_input=request.retained_.bytes;
  result.report_.total_admission=request.retained_.bytes+options.maximum_workspace;
  result.report_.workspace=std::max(bytes.used,fixed_bytes+anatomy.analysis_bytes);
  for(auto id:request.targets_){work.stop();const auto&before=input.object(id);const auto&current=support.object(id);const auto&mesh=before.painted->mesh();
    const auto found=std::find_if(anatomy.parts.begin(),anatomy.parts.end(),[&](const auto&p){return p.object==id;});
    if(found==anatomy.parts.end()||found->mesh!=before.painted->mesh_owner())throw std::runtime_error("Surface heat lost its captured current boundary owner");
    // A completely locked current field is a true no-op; no source capsule or
    // optimizer invocation is manufactured. Partial locks retain exact values.
    if(!options.locks.empty()&&std::all_of(options.locks.begin(),options.locks.end(),[](auto x){return x!=0;})){
      for(const auto&row:before.deformation->skin->vertices){work.step(64);if(row.size()>options.limits.max_influences)throw std::invalid_argument("Locked influences exceed the surface heat cap");}continue;
    }
    VisibilityGeodesicResult field;
    if(!found->fixed.empty()){
      if(found->fixed.size()!=mesh.positions.size())throw std::runtime_error("Surface heat fixed boundary dimensions");
      field.weights.vertices.reserve(found->fixed.size());for(auto joint:found->fixed){work.step(16);if(joint>=world_rig.bones.size()||!world_rig.bones[joint].deform)throw std::invalid_argument("Surface heat boundary references a nondeform joint");field.weights.vertices.push_back({{joint,1}});}
    }else{
      BindOptions bind;bind.maximum_work=work.maximum-work.used;bind.limits=options.limits;bind.maximum_seed_distance=options.seed_distance;
      const auto metric=origin*input.world_matrix(id);
      // Sew only the genuine raw one-owner/all-morph geometry. Quantized world
      // coordinates must never invent an additional raw seam correspondence.
      field=bind_visibility_object(before,world_rig,bind,cancelled,options.maximum_workspace-bytes.used,
       [&](const Mesh&raw,const Skeleton&rig,BindOptions o,const std::function<bool()>&cancel,size_t available){
        visibility_geodesic::Bytes staging{0,available};staging.add(65536);staging.add(raw.positions.size(),2*sizeof(Vec3)+32);staging.add(raw.indices.size(),sizeof(uint32_t)+1);
        SkinGeometryWork geometry(o.maximum_work,cancel);const auto initial=geometry.remaining();auto placed=skin_world_mesh(raw,metric,geometry);const auto used=initial-geometry.remaining();
        if(used>=o.maximum_work)throw std::length_error("Surface heat metric exhausts actual work before solve");o.maximum_work-=used;
        auto value=visibility_heat_bind(placed,rig,o,cancel,available-staging.used,profile,HeatDomainPolicy::ComponentHandleSupport);value.work+=used;value.workspace+=staging.used;return value;
       });work.step(field.work);
    }
    auto d=std::make_shared<DeformationState>(*current.deformation);
    if(!options.locks.empty())for(size_t row=0;row<field.weights.vertices.size();++row){work.step(256);auto&next=field.weights.vertices[row];std::erase_if(next,[&](auto x){return options.locks.at(x.joint)!=0;});for(auto x:before.deformation->skin->vertices.at(row))if(options.locks.at(x.joint))next.push_back(x);next=normalize_weights(std::move(next),d->skeleton->bones.size(),options.locks,options.limits);}
    bool changed=!before.deformation||!before.deformation->skin;
    if(!changed)for(size_t row=0;row<field.weights.vertices.size();++row){work.step(128);if(!geometry_weight_authoring::equal_row(field.weights.vertices[row],before.deformation->skin->vertices.at(row))){changed=true;break;}}
    result.report_.parts.push_back({id,field.work,field.workspace,field.closed_components,field.nonembedded_components,field.heat_residual,field.partition_error});
    result.report_.workspace=std::max(result.report_.workspace,bytes.used+field.workspace);
    if(changed){d->skin=std::move(field.weights);weight_surface::validate_rest_deformation(*d,mesh,work);if(before.material_bindings&&before.material_bindings->normal_frames)normal_frame_current_authority(*before.material_bindings,mesh,d.get(),cancelled);auto capsule=before.rig_source;if(!capsule)capsule=capture_polygon_origin(input,id);result.changes_.push_back({id,std::move(d),std::move(capsule)});}
  }
  work.stop();result.report_.work=work.used;result.report_.changed=result.changes_.size();return result;
}
inline bool adopt_native_surface_heat(Scene&scene,const NativeSurfaceHeatRequest&request,NativeSurfaceHeatPrepared&&result){
  if(!result.identity_||result.identity_!=request.identity_||result.identity_.owner_before(request.identity_)||request.identity_.owner_before(result.identity_))throw std::invalid_argument("Surface heat result belongs to a different owning request");
  if(!request.lease_.matches(scene))throw std::runtime_error("Surface heat source lease changed before publication");
  if(result.changes_.size()>request.targets_.size())throw std::invalid_argument("Surface heat prepared recipients exceed captured request");
  if(result.changes_.empty())return false;
  Scene staged=scene;size_t cursor=0;
  for(const auto&change:result.changes_){while(cursor<request.targets_.size()&&request.targets_[cursor]!=change.object)++cursor;if(cursor==request.targets_.size()||!change.deformation||!change.origin)throw std::invalid_argument("Surface heat prepared target/capsule identity");++cursor;auto&o=staged.object(change.object);o.deformation=change.deformation;o.rig_source=change.origin;}
  staged.validate();if(!request.lease_.matches(scene))throw std::runtime_error("Surface heat source changed during final validation");
  static_assert(std::is_nothrow_move_assignable_v<Scene>);scene=std::move(staged);return true;
}
}
