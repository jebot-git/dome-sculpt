// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/skin-authoring.hpp"
#include <nlohmann/json.hpp>
namespace dome {
namespace {
void stop(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled()) throw std::runtime_error("Skin authoring cancelled");
}
Vec3 point(const CanvasTransform &t, Vec3 p) {
  auto result=ob::transform(t,{p.x,p.y,p.z}); return {result.x,result.y,result.z};
}
Vec3 direction(const CanvasTransform &t, Vec3 p) {
  auto result=ob::rotate(t.orientation,{p.x,p.y,p.z}); return {result.x,result.y,result.z};
}
JointTransform root_transform(JointTransform joint,const CanvasTransform &t) {
  joint.translation=point(t,joint.translation);
  const auto &q=t.orientation;
  joint.rotation=(Quat{q.w,q.x,q.y,q.z}*joint.rotation).unit();
  joint.scale=joint.scale*t.scale;
  return joint;
}
}
void SkinAuthoringOptions::validate() const {
  if(mode!=SkinAuthoringMode::Automatic && mode!=SkinAuthoringMode::Transfer)
    throw std::invalid_argument("Unknown skin authoring operation");
  if(!reference || !std::isfinite(seed_distance) || seed_distance<0 || seed_distance>10000 ||
     !std::isfinite(seed_margin) || seed_margin<0 || seed_margin>10000 ||
     !std::isfinite(transfer_distance) || transfer_distance<0 || transfer_distance>10000 ||
     !std::isfinite(falloff) || falloff<1 || falloff>8 ||
     !maximum_work || maximum_work>500'000'000 ||
     !limits.max_influences || limits.max_influences>32 ||
     !std::isfinite(limits.prune) || limits.prune<0 || limits.prune>=1 || locks.size()>256 ||
     std::any_of(locks.begin(),locks.end(),[](uint8_t v){return v>1;}))
    throw std::invalid_argument("Invalid skin distance, falloff, influence, lock or work bounds");
  if((mode==SkinAuthoringMode::Automatic && (seed_distance<=0 || seed_margin>seed_distance)) ||
     (mode==SkinAuthoringMode::Transfer && transfer_distance<=0))
    throw std::invalid_argument("Automatic seeds need a positive radius and contained margin; transfer needs a positive radius");
}
float skin_distance_metres(float t) {
  if(!std::isfinite(t) || t<0 || t>1) throw std::invalid_argument("Invalid skin distance slider");
  if(t==1)return 10000;
  return float(std::expm1(double(t)*std::log1p(1e9))*1e-5);
}
float skin_distance_slider(float metres) {
  if(!std::isfinite(metres) || metres<0 || metres>10000) throw std::invalid_argument("Invalid world skin distance");
  return float(std::log1p(double(metres)*1e5)/std::log1p(1e9));
}
bool author_skin(Scene &scene,uint64_t target_id,const SkinAuthoringOptions &options,
                 const std::function<bool()> &cancelled) {
  stop(cancelled);
  options.validate();
  const auto &target=scene.object(target_id), &reference=scene.object(options.reference);
  if(!target.visible || target.locked || !target.mesh_active || !target.painted)
    throw std::invalid_argument("Select a visible, unlocked mesh to bind");
  if(!reference.mesh_active || !reference.painted || !reference.deformation ||
     !reference.deformation->skeleton ||
     (options.mode==SkinAuthoringMode::Transfer && !reference.deformation->skin))
    throw std::invalid_argument("Choose a fitted reference skeleton; transfer also requires source skin weights");
  if(options.mode==SkinAuthoringMode::Transfer && reference.id==target.id)
    throw std::invalid_argument("Choose another mesh as the weight-transfer source");
  if(options.mode!=SkinAuthoringMode::Automatic && options.mode!=SkinAuthoringMode::Transfer)
    throw std::invalid_argument("Unknown skin authoring operation");
  const auto &source=*reference.deformation;
  source.validate(reference.painted->mesh());
  if(target.id!=reference.id && target.deformation) target.deformation->validate(target.painted->mesh());
  stop(cancelled);
  auto reference_world=scene.world_transform(reference.id), target_world=scene.world_transform(target.id);
  auto source_to_target=ob::compose(ob::inverse(target_world),reference_world);
  auto target_to_source=ob::compose(ob::inverse(reference_world),target_world);
  auto skeleton=*source.skeleton;
  auto pose=source.pose;
  for(size_t i=0;i<skeleton.bones.size();++i) if(skeleton.bones[i].parent<0) {
    skeleton.bones[i].rest=root_transform(skeleton.bones[i].rest,source_to_target);
    if(!pose.empty()) pose[i]=root_transform(pose[i],source_to_target);
  }
  skeleton.validate();
  bool existing=target.deformation && target.deformation->skeleton;
  // Identity placement keeps the original exact encoding, avoiding quaternion
  // renormalization noise during an ordinary rebind to the selected mesh.
  auto equal_placement=[](const CanvasTransform &a,const CanvasTransform &b) {
    bool rotation=(a.orientation.x==b.orientation.x && a.orientation.y==b.orientation.y &&
                   a.orientation.z==b.orientation.z && a.orientation.w==b.orientation.w) ||
                  (a.orientation.x==-b.orientation.x && a.orientation.y==-b.orientation.y &&
                   a.orientation.z==-b.orientation.z && a.orientation.w==-b.orientation.w);
    return a.position.x==b.position.x && a.position.y==b.position.y && a.position.z==b.position.z &&
      rotation && a.scale==b.scale;
  };
  bool same_placement=equal_placement(reference_world,target_world);
  if(same_placement) { skeleton=*source.skeleton; pose=source.pose; }
  if(existing && target.deformation->skeleton->encode()!=skeleton.encode())
    throw std::invalid_argument("Target has a different rest rig; use an explicit joint-mapping workflow");
  if((!options.locks.empty() && (!existing || !target.deformation->skin ||
       options.locks.size()!=skeleton.bones.size())) ||
     std::any_of(options.locks.begin(),options.locks.end(),[](uint8_t v){return v>1;}))
    throw std::invalid_argument("Skin locks require the target's exact existing joint identities");
  if(existing && target.deformation->skin && !options.locks.empty()) {
    bool every_joint_locked=std::all_of(options.locks.begin(),options.locks.end(),[](uint8_t value){return value==1;});
    bool closed=true;
    size_t vertex=0;
    for(const auto &row:target.deformation->skin->vertices) {
      if(!(vertex++ &1023))stop(cancelled);
      double fixed=0;size_t count=0;
      for(auto influence:row) if(options.locks[influence.joint]) {fixed+=influence.weight;++count;}
      if(count>options.limits.max_influences)
        throw std::invalid_argument("Locked influences exceed the chosen influence cap");
      if(count!=row.size() || (!every_joint_locked && fixed<1-1e-6))closed=false;
    }
    if(closed){stop(cancelled);return false;}
  }
  auto rest=target.painted->mesh();
  for(size_t i=0;!same_placement && i<rest.positions.size();++i) {
    if(!(i%1024)) stop(cancelled);
    rest.positions[i]=point(target_to_source,rest.positions[i]);
    rest.normals[i]=direction(target_to_source,rest.normals[i]);
  }
  SkinWeights weights;
  if(options.mode==SkinAuthoringMode::Automatic) {
    BindOptions bind; bind.limits=options.limits; bind.maximum_work=options.maximum_work;
    bind.maximum_seed_distance=options.seed_distance/reference_world.scale;
    bind.seed_margin=options.seed_margin/reference_world.scale; bind.falloff=options.falloff;
    weights=automatic_bind(rest,*source.skeleton,bind,cancelled);
  } else {
    weights=transfer_weights(reference.painted->mesh(),*source.skin,rest,skeleton.bones.size(),
                             options.transfer_distance/reference_world.scale,options.limits,cancelled,options.maximum_work);
  }
  if(!options.locks.empty()) for(size_t vertex=0;vertex<weights.vertices.size();++vertex) {
    if(!(vertex%1024)) stop(cancelled);
    auto &candidate=weights.vertices[vertex];
    std::erase_if(candidate,[&](const Influence &i){return options.locks.at(i.joint);});
    for(auto influence:target.deformation->skin->vertices.at(vertex))
      if(options.locks.at(influence.joint)) candidate.push_back(influence);
    candidate=normalize_weights(std::move(candidate),skeleton.bones.size(),options.locks,options.limits);
  }
  if(existing && target.deformation->skin) {
    bool same=true;
    for(size_t vertex=0;vertex<weights.vertices.size();++vertex) {
      if(!(vertex%1024)) stop(cancelled);
      auto &row=weights.vertices[vertex];const auto &old=target.deformation->skin->vertices[vertex];
      bool equal=row.size()==old.size();
      for(const auto &i:row) equal &= std::any_of(old.begin(),old.end(),[&](const auto &o){return i==o;});
      if(equal)row=old;else same=false;
    }
    if(same){stop(cancelled);return false;}
  }
  auto next=target.deformation ? *target.deformation : DeformationState(target.painted->mesh());
  next.skeleton=std::move(skeleton); next.skin=std::move(weights);
  if(!existing) {
    next.pose=std::move(pose); next.humanoid=source.humanoid;
    if(source.landmarks) {
      next.landmarks=*source.landmarks;
      if(!same_placement) {
        for(auto &[name,p]:next.landmarks->points) p=point(source_to_target,p);
        next.landmarks->left=direction(source_to_target,next.landmarks->left);
        next.landmarks->up=direction(source_to_target,next.landmarks->up);
        next.landmarks->front=direction(source_to_target,next.landmarks->front);
      }
    }
  }
  next.validate(target.painted->mesh());
  Scene result=scene;
  result.object(target.id).deformation=std::make_shared<const DeformationState>(std::move(next));
  result.validate(false); stop(cancelled); scene=std::move(result);return true;
}
} // namespace dome
