// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-weight-retention.hpp"
#include "native-head-geometry.hpp"
#include "rotary-cap-geometry.hpp"
#include "weight-boundary-sewing.hpp"
#include "native-carrier-continuation.hpp"
namespace dome::detail {
// Explicit source-informed anatomical surface/contact construction. Neither a
// volume PDE nor a geometric guarantee for arbitrary edited/native meshes.
// Original artist influence values are never an input to this method.
struct NativeAnatomicalOptions {
  size_t maximum_work=500'000'000;
  size_t maximum_workspace=256u*1024u*1024u;
  size_t maximum_total=512u*1024u*1024u;
  std::vector<uint8_t> locks;
};
struct NativeAnatomicalReport {
  size_t work=0,workspace=0,retained_input=0,total_admission=0,changed=0;
  GeometryWeightBindReport base;
};
struct NativeAnatomicalLease {
  struct Object {
    SculptObject metadata;
    std::weak_ptr<PaintedMesh> paint;
    std::weak_ptr<Volume> volume;
    std::shared_ptr<const Mesh> mesh;
    Volume::GeometryOwner geometry;
    uint64_t paint_revision=0,volume_revision=0;
    std::array<double,16> world;
  };
  std::vector<Object> objects;
  std::vector<SceneGroup> groups;
  uint64_t active=0;
  bool matches(const Scene&scene)const {
    if(scene.objects().size()!=objects.size()||scene.groups().size()!=groups.size()||scene.active().id!=active)return false;
    auto same=[](const auto&a,const auto&b){return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);};
    for(size_t i=0;i<groups.size();++i){const auto&a=groups[i];const auto&b=scene.groups()[i];if(a.id!=b.id||a.parent!=b.parent||a.name!=b.name||!native_weight_state::transform(a.transform,b.transform)||!same_humanoid_controls(a.humanoid,b.humanoid)||!same(a.source_placement,b.source_placement)||!same(a.affine_edit,b.affine_edit))return false;}
    for(size_t i=0;i<objects.size();++i){const auto&x=objects[i];const auto&a=x.metadata;const auto&b=scene.objects()[i];auto p=x.paint.lock();auto v=x.volume.lock();
      if(a.id!=b.id||a.name!=b.name||a.group!=b.group||a.visible!=b.visible||a.locked!=b.locked||a.mesh_active!=b.mesh_active||!native_weight_state::transform(a.transform,b.transform)||!native_weight_state::material(a.material,b.material)||!same(p,b.painted)||!same(v,b.volume)||!v||!same(a.deformation,b.deformation)||!same(a.appearance,b.appearance)||!same(a.material_bindings,b.material_bindings)||!same(a.retopology,b.retopology)||!same(a.polygon,b.polygon)||!same(a.shape_source,b.shape_source)||!same(a.rig_source,b.rig_source)||!same(a.bake_recipe,b.bake_recipe)||!same(a.affine_edit,b.affine_edit)||b.volume->revision()!=x.volume_revision||!same(x.geometry,b.volume->geometry_owner())||!NativeWeightVolumeAccess::same_current(*a.volume,*b.volume)||(p&&(!a.painted||!NativeWeightPaintAccess::same_current(*a.painted,*p)))||(p&&(p->revision()!=x.paint_revision||!same(x.mesh,p->mesh_owner())))||!native_weight_state::world(scene.world_matrix(b.id).matrix(),x.world))return false;
    }
    return true;
  }
};
class NativeAnatomicalPrepared;
class NativeAnatomicalRequest {
  Scene input_;
  std::vector<uint64_t> targets_,support_;
  uint64_t reference_=0;
  NativeAnatomicalOptions options_;
  NativeWeightRetention retained_;
  size_t capture_bytes_=0;
  NativeAnatomicalLease lease_;
  std::shared_ptr<const uint64_t> identity_;
public:
  bool matches(const Scene&scene)const{return lease_.matches(scene);}
private:
  friend NativeAnatomicalRequest capture_native_anatomical(const Scene&,std::span<const uint64_t>,uint64_t,const NativeAnatomicalOptions&);
  friend NativeAnatomicalPrepared prepare_native_anatomical(const NativeAnatomicalRequest&,const std::function<bool()>&);
  friend bool adopt_native_anatomical(Scene&,const NativeAnatomicalRequest&,NativeAnatomicalPrepared&&);
};
class NativeAnatomicalPrepared {
  struct Change {uint64_t object;std::shared_ptr<const DeformationState> deformation;std::shared_ptr<const PolygonOrigin> origin;};
  std::vector<Change> changes;
  NativeAnatomicalReport report_;
  std::shared_ptr<const uint64_t> identity_;
  friend NativeAnatomicalPrepared prepare_native_anatomical(const NativeAnatomicalRequest&,const std::function<bool()>&);
  friend bool adopt_native_anatomical(Scene&,const NativeAnatomicalRequest&,NativeAnatomicalPrepared&&);
public:
  const NativeAnatomicalReport& report()const noexcept{return report_;}
};
inline NativeAnatomicalRequest capture_native_anatomical(const Scene&source,
 std::span<const uint64_t>selected,uint64_t reference,const NativeAnatomicalOptions&options={}){
  if(selected.empty()||selected.size()>64||source.objects().size()>128||source.groups().size()>128||!options.maximum_work||options.maximum_work>500'000'000||options.maximum_workspace<65536||options.maximum_workspace>256u*1024u*1024u||options.maximum_total<options.maximum_workspace||options.maximum_total>512u*1024u*1024u||options.locks.size()>256||std::any_of(options.locks.begin(),options.locks.end(),[](auto x){return x>1;}))throw std::invalid_argument("Native anatomical owning options/domain");
  // Pure bounded scan; no user callback and no Scene/JSON snapshot allocation
  // before the exact retained closure plus full working cap has been admitted.
  const auto retained=native_weight_retention(source,options.maximum_total-options.maximum_workspace);
  geometry_weight_authoring::Bytes capture{65536,options.maximum_workspace};
  weight_surface::Budget capture_work{retained.work,options.maximum_work,{}};
  capture.add(source.objects().size(),2048);capture.add(source.groups().size(),4096);capture.add(options.locks.size()+selected.size()*sizeof(uint64_t));
  for(const auto&o:source.objects()){
    if(!o.volume||o.volume->in_stroke()||(o.mesh_active&&!o.painted)||(o.painted&&o.painted->in_stroke()))throw std::invalid_argument("Native anatomical capture requires complete stable objects");
    capture.text(o.name);capture.text(o.material.name);capture_work.step(512+4*(o.name.size()+o.material.name.size()));
    if(o.painted)for(const auto&l:o.painted->layers()){capture_work.step_items(l.tiles.size()+l.pbr_tiles.size()+l.emission_tiles.size(),4);capture_work.step(l.name.size()*4+16);}
    capture_work.step_items(o.volume->layers().size(),64);
  }
  if(capture_work.used>=capture_work.maximum)throw std::length_error("Native anatomical capture exhausted work before staging");
  NativeAnatomicalRequest request;request.capture_bytes_=capture.used;
  request.retained_=retained;request.retained_.work=capture_work.used;request.options_=options;request.reference_=reference;request.identity_=std::make_shared<const uint64_t>(0);
  std::set<uint64_t> unique;
  for(auto id:selected){const auto&o=source.object(id);if(!unique.insert(id).second||!o.visible||o.locked||!o.mesh_active||!o.painted||!o.volume||o.painted->in_stroke()||o.volume->in_stroke())throw std::invalid_argument("Choose explicit editable anatomical targets");request.targets_.push_back(id);}
  const auto&ref=source.object(reference);
  if(!ref.deformation||!ref.deformation->skeleton||!ref.deformation->humanoid)throw std::invalid_argument("Choose the current native humanoid reference");
  if(!options.locks.empty()&&options.locks.size()!=ref.deformation->skeleton->bones.size())throw std::invalid_argument("Native anatomical lock/joint identity");
  const CreatorAppearance*first=nullptr;uint64_t current_root=0;
  std::optional<uint64_t> source_instance;std::shared_ptr<const CreatorSourceModel> model;
  std::set<std::pair<int,int>> primitives;
  auto same=[](const auto&a,const auto&b){return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);};
  for(const auto&o:source.objects())if(o.mesh_active&&o.painted&&o.appearance){
    const auto&a=*o.appearance;
    if(!o.group)throw std::invalid_argument("Native anatomical support lacks an explicit current group");
    const auto path=source.group_path(o.group);
    if(path.empty())throw std::invalid_argument("Native anatomical current group path missing");
    if(!first){first=&a;current_root=path.front().id;}
    if(!a.asset||!a.original_bytes||!same(a.asset,first->asset)||!same(a.original_bytes,first->original_bytes)||path.front().id!=current_root||!primitives.emplace(a.source_mesh,a.source_primitive).second)throw std::invalid_argument("Native anatomical support requires one complete unambiguous current source group");
    // Legacy similarity imports have genuine primitive provenance but no
    // Source13 node capsule. The CURRENT group is request scope, never claimed
    // original placement authority; all metric transforms are current world.
    const auto&node=source.group(o.group).source_placement;
    if(node){
      if(!node->model||!node->source_instance||a.source_node<0||node->source_node!=uint32_t(a.source_node)||!same(node->model->asset,a.asset)||!same(node->model->original_bytes,a.original_bytes))throw std::invalid_argument("Native anatomical primitive contradicts its source node/model placement");
      if(source_instance&&(*source_instance!=node->source_instance||!same(model,node->model)))throw std::invalid_argument("Native anatomical support crosses original instances");
      model=node->model;source_instance=node->source_instance;
    }
    request.support_.push_back(o.id);
  }
  if(!first||request.support_.size()>64||request.support_.size()!=first->asset->primitives.size())throw std::invalid_argument("Native anatomical support needs every authenticated current source primitive");
  for(auto id:request.targets_)if(std::find(request.support_.begin(),request.support_.end(),id)==request.support_.end())throw std::invalid_argument("Native anatomical target lacks current source correspondence");
  if(!options.locks.empty())for(auto id:request.targets_){const auto&o=source.object(id);if(!o.deformation||!o.deformation->skin||!o.deformation->skeleton)throw std::invalid_argument("Native anatomical locks require actual current skin authority");}
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
  if(!request.lease_.matches(source))throw std::runtime_error("Native anatomical source changed during capture");
  return request;
}
inline NativeAnatomicalPrepared prepare_native_anatomical(const NativeAnatomicalRequest&request,
 const std::function<bool()>&cancelled={}){
  Scene prepared=request.input_;
  const auto&scene=request.input_;
  const auto&options=request.options_;
  const auto&retained=request.retained_;
  const auto&targets=request.support_;
  const auto reference=request.reference_;
  if(cancelled&&cancelled())throw std::runtime_error("Native anatomical preparation cancelled");
  DeclaredWeightSupport support;
  support.policy = DeclaredSupportPolicy::UniqueSourceWitnesses;
  support.world_metric = true;
  support.reference = reference;
  support.row_anchors = DeclaredRowAnchors::NearestPermitted;
  support.source_requirements =
      DeclaredSourceRequirements::GeometryEvidence;
  support.source_span = DeclaredSourceSpan::AuthoredFinite;
  support.origin_boundary =
      DeclaredOriginBoundary::NativeFiniteLineIntegral;
  support.contact_support = DeclaredContactSupport::CommonMask;
  support.rig_geometry = DeclaredRigGeometry::NativeBoneAxes;
  const auto analysis = analyze_native_head_geometry(scene, reference,
      std::min(size_t(100'000'000),options.maximum_work-retained.work),
      std::min(size_t(128u*1024u*1024u),options.maximum_workspace),cancelled);
  // Research candidate only: the same exact boundary algebra applies these
  // GEOMETRY-GENERATED constraints, with no manual target IDs or role palette.
  // They are not represented as user provenance or original-source authority.
  const auto &reference_object = scene.object(reference);
  const auto &rig = *reference_object.deformation->skeleton;
  const auto &roles = reference_object.deformation->humanoid->joints;
  const auto head_joint = roles.at("head");
  HeatJointMask eyes{};
  for (auto role : {"leftEye","rightEye"}) {auto j=roles.at(role);eyes[j/64]|=uint64_t(1)<<(j%64);}
  const auto &never = cancelled;
  weight_surface::Budget construction{0, options.maximum_work-analysis.work-retained.work, never};
  auto shift=AffineTransform{}.matrix();auto refworld=scene.world_matrix(reference);
  for(size_t k=0;k<3;++k)shift[12+k]=-refworld.matrix()[12+k];
  const AffineTransform origin(shift);
  std::vector<HeatSourceSegment> eye_points;
  const auto segments=rig.segments();
  for(auto role:{"leftEye","rightEye"}){
    auto j=roles.at(role);auto p=segments[j][0];auto q=(origin*refworld).point({p.x,p.y,p.z});
    const std::array<double,3> point{float(q[0]),float(q[1]),float(q[2])};
    if(rig.bones[j].deform)eye_points.push_back({point,point,j});
  }
  NearestWeightSources nearest_eye(eye_points,construction);
  size_t temporary_peak=analysis.bytes;
  for(auto id:targets){
    DeclaredWeightPart part{id,{}};
    const auto at=std::find_if(analysis.parts.begin(),analysis.parts.end(),[&](const auto&p){return p.object==id;});
    if(at==analysis.parts.end())throw std::runtime_error("Automatic source part lost its analysis");
    bool ocular=!at->head_region&&at->geometric_controls!=HeatJointMask{};
    for(size_t k=0;k<4;++k)ocular&=(at->geometric_controls[k]&~eyes[k])==0;
    if(at->head_region||ocular){
      const auto&o=scene.object(id);const auto&m=o.painted->mesh();size_t local=0;
      auto components=heat_intent_components(m,{},local,construction.maximum-construction.used,never);construction.step(local);
      std::map<uint32_t,std::vector<uint32_t>>vertices;
      for(uint32_t i=0;i<components.size();++i){construction.step(16);vertices[components[i]].push_back(i);}
      std::optional<Mesh> metric;
      if(ocular){SkinGeometryWork geom(construction.maximum-construction.used,never);auto available=geom.remaining();metric=skin_world_mesh(m,origin*scene.world_matrix(id),geom);construction.step(available-geom.remaining());}
      for(const auto&[root,rows]:vertices){
        uint32_t joint=head_joint;bool convex=false;
        if(ocular){
          Mesh subset;std::vector<uint32_t>map(m.positions.size(),UINT32_MAX);
          for(auto i:rows){construction.step(8);map[i]=subset.positions.size();subset.positions.push_back(metric->positions[i]);}
          for(size_t c=0;c<m.indices.size();c+=3){construction.step(8);if(components[m.indices[c]]!=root)continue;for(size_t k=0;k<3;++k)subset.indices.push_back(map[m.indices[c+k]]);}
          auto patch=analyze_projected_ocular_patch(subset,128u*1024u*1024u,construction);
          convex=patch.disk&&patch.convex_boundary;
          temporary_peak=std::max(temporary_peak,subset.positions.size()*112+subset.indices.size()*160+32768+m.positions.size()*32);
          if(convex){
            uint32_t selected=UINT32_MAX;
            for(auto p:subset.positions){
              const auto mask=nearest_eye.nearest({p.x,p.y,p.z},construction);size_t count=0;uint32_t candidate=0;
              for(uint32_t j=0;j<rig.bones.size();++j)if(mask[j/64]&(uint64_t(1)<<(j%64))){++count;candidate=j;}
              if(count!=1||(selected!=UINT32_MAX&&selected!=candidate))throw std::runtime_error("Convex ocular patch has ambiguous/cross-eye control support");
              selected=candidate;
            }
            joint=selected;
          }
        }
        part.declarations.push_back({root,{joint}});
      }
      // Same exact algebraic primitive as the explicit-boundary oracle; this
      // research request is generated by the declared geometry rule ABOVE.
      part.user_singleton_boundary=true;
    }else part.user_excluded_joints.push_back(head_joint);
    const auto &own=scene.object(id);
    std::vector<std::vector<Vec3>> deltas;
    if(own.deformation&&own.deformation->morphs)
      for(const auto&t:own.deformation->morphs->targets)deltas.push_back(t.delta);
    const auto sewn=geometric_boundary_sewing(own.painted->mesh(),deltas,
                          128u*1024u*1024u,construction);
    temporary_peak=std::max(temporary_peak,sewn.bytes+analysis.bytes);
    part.geometric_row_groups=sewn.groups;
    support.parts.push_back(std::move(part));
  }
  support.hierarchy_joint=head_joint;
  auto head=[&](uint64_t id){for(const auto&p:analysis.parts)if(p.object==id)return p.head_region;throw std::logic_error("Unknown captured part");};
  for(const auto&e:analysis.seam_endpoints) {
    if(head(e.first)!=head(e.second)) {
      const auto other=head(e.first)?e.second:e.first;
      const auto vertex=head(e.first)?e.second_vertex:e.first_vertex;
      if(std::find(support.hierarchy_transition_parts.begin(),support.hierarchy_transition_parts.end(),other)==support.hierarchy_transition_parts.end())
        support.hierarchy_transition_parts.push_back(other);
      support.hierarchy_one_boundary.push_back({other,vertex});
    }else support.geometric_equalities.push_back({e.first,e.second,e.first_vertex,e.second_vertex});
  }
  if(support.hierarchy_transition_parts.empty())support.hierarchy_joint=UINT32_MAX;
  size_t declaration_bytes = support.geometric_equalities.capacity()*sizeof(DeclaredWeightSupport::Equality)+
    support.hierarchy_transition_parts.capacity()*sizeof(uint64_t)+support.hierarchy_one_boundary.capacity()*sizeof(DeclaredWeightSupport::Boundary);
  for (const auto &part : support.parts) {
    declaration_bytes += part.user_excluded_joints.capacity() * sizeof(uint32_t);
    declaration_bytes += part.geometric_row_groups.capacity()*sizeof(uint32_t);
    declaration_bytes += part.declarations.capacity() * sizeof(HeatComponentDeclaration);
    for (const auto &entry : part.declarations)
      declaration_bytes += entry.joints.capacity() * sizeof(uint32_t) + 64;
  }
  const auto carrier=native_carrier_plan(scene,analysis,support,rig,*reference_object.deformation->humanoid,construction);
  const size_t continuation_allowance=native_carrier_work_plan(scene,carrier,reference,construction);
  geometry_weight_authoring::Bytes locks_bytes{0,options.maximum_workspace};
  if(!options.locks.empty())for(auto id:request.targets_){const auto&o=scene.object(id);geometry_weight_authoring::deformation_copy_bytes(*o.deformation,locks_bytes,construction);locks_bytes.add(o.painted->mesh().positions.size(),4*sizeof(Influence)+96);}
  const size_t wrapper_work = retained.work + analysis.work + construction.used + 4096 + targets.size()*32;
  const size_t wrapper_bytes = locks_bytes.used+request.capture_bytes_+sizeof(support)+sizeof(analysis)+analysis.parts.capacity()*sizeof(NativeHeadGeometryReport::Part)+analysis.contacts.capacity()*sizeof(NativeHeadGeometryReport::Contact)+analysis.seam_endpoints.capacity()*sizeof(NativeHeadGeometryReport::Seam)+declaration_bytes+targets.size()*sizeof(DeclaredWeightPart)+scene.objects().size()*(sizeof(SculptObject)+512)+scene.groups().size()*(sizeof(SceneGroup)+512)+65536;
  if(wrapper_work>=options.maximum_work||wrapper_bytes>=options.maximum_workspace||continuation_allowance>options.maximum_work-wrapper_work)throw std::length_error("Native anatomical complete request forecast exceeds original resources");
  GeometryWeightBindOptions bind;
  bind.limits={4,1e-5f};bind.maximum_work=options.maximum_work-wrapper_work-continuation_allowance;bind.maximum_workspace=options.maximum_workspace-wrapper_bytes;
  GeometryWeightBindReport report;
  for(auto id:targets){prepared.object(id).visible=true;prepared.object(id).locked=false;}
  author_geometry_weight_skin(prepared,targets,support,bind,cancelled,&report);
  const auto continuation_bytes=carrier.targets.empty()?0:continue_native_carrier(prepared,carrier,reference,continuation_allowance,options.maximum_workspace-wrapper_bytes,cancelled);
  NativeAnatomicalPrepared result;result.identity_=request.identity_;
  result.report_={report.work+wrapper_work+continuation_allowance,std::max({report.workspace+wrapper_bytes,temporary_peak,continuation_bytes+wrapper_bytes}),retained.bytes,retained.bytes+options.maximum_workspace,0,report};
  // Adopt ONLY requested recipients. Every other solved support surface stays
  // private and read-only to the Document; its temporary weights are discarded.
  weight_surface::Budget output{result.report_.work,options.maximum_work,cancelled};
  for(auto id:request.targets_){const auto&before=scene.object(id);const auto&after=prepared.object(id);
    auto deform=after.deformation;
    if(!options.locks.empty()){
      auto d=std::make_shared<DeformationState>(*deform);
      for(size_t row=0;row<d->skin->vertices.size();++row){output.step(256);VertexWeights values;for(auto x:d->skin->vertices[row])if(!options.locks.at(x.joint))values.push_back(x);for(auto x:before.deformation->skin->vertices.at(row))if(options.locks.at(x.joint))values.push_back(x);d->skin->vertices[row]=normalize_weights(std::move(values),d->skeleton->bones.size(),options.locks,{4,1e-5f});}
      d->validate(before.painted->mesh());deform=std::move(d);
    }
    bool differs=!before.deformation||!before.deformation->skeleton||!before.deformation->skin;
    if(!differs){const auto&a=before.deformation->skin->vertices;const auto&b=deform->skin->vertices;differs=a.size()!=b.size();
      if(!differs)for(size_t i=0;i<a.size();++i){output.step(8+4*a[i].size()*b[i].size());if(!geometry_weight_authoring::equal_row(a[i],b[i])){differs=true;break;}}}
    if(differs)result.changes.push_back({id,std::move(deform),after.rig_source});
  }
  output.stop();result.report_.work=output.used;result.report_.changed=result.changes.size();return result;
}
inline bool adopt_native_anatomical(Scene&scene,const NativeAnatomicalRequest&request,NativeAnatomicalPrepared&&result){
  if(!result.identity_||result.identity_!=request.identity_||result.identity_.owner_before(request.identity_)||request.identity_.owner_before(result.identity_))throw std::invalid_argument("Native anatomical prepared result belongs to another owning request");
  if(!request.lease_.matches(scene))throw std::runtime_error("Native anatomical source lease changed before publication");
  if(result.changes.size()>request.targets_.size())throw std::invalid_argument("Native anatomical prepared recipients exceed request");
  if(result.changes.empty())return false;
  Scene staged=scene;size_t cursor=0;
  for(const auto&c:result.changes){while(cursor<request.targets_.size()&&request.targets_[cursor]!=c.object)++cursor;
    if(cursor==request.targets_.size()||!c.deformation||!c.origin)throw std::invalid_argument("Native anatomical prepared recipient authority");++cursor;
    auto&o=staged.object(c.object);o.deformation=c.deformation;o.rig_source=c.origin;}
  staged.validate();
  if(!request.lease_.matches(scene))throw std::runtime_error("Native anatomical source changed during final validation");
  static_assert(std::is_nothrow_move_assignable_v<Scene>);scene=std::move(staged);return true;
}
}
