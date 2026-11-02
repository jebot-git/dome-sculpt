// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig-fit.hpp"
#include <limits>
namespace dome {
namespace {
void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Humanoid fitting cancelled");}
Vec3 point(const AffineTransform &m,Vec3 p){auto v=m.point({p.x,p.y,p.z});return {float(v[0]),float(v[1]),float(v[2])};}
float segment_distance(Vec3 p,Vec3 a,Vec3 b){auto d=b-a;auto n=dot(d,d);return length(p-(a+d*(n>0?std::clamp(dot(p-a,d)/n,0.f,1.f):0)));}
float triangle_distance(Vec3 p,Vec3 a,Vec3 b,Vec3 c){
 auto ab=b-a,ac=c-a,ap=p-a;float d1=dot(ab,ap),d2=dot(ac,ap);
 if(d1<=0&&d2<=0)return length(ap);
 auto bp=p-b;float d3=dot(ab,bp),d4=dot(ac,bp);
 if(d3>=0&&d4<=d3)return length(bp);
 float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0&&d1!=d3)return length(p-(a+ab*(d1/(d1-d3))));
 auto cp=p-c;float d5=dot(ab,cp),d6=dot(ac,cp);
 if(d6>=0&&d5<=d6)return length(cp);
 float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0&&d2!=d6)return length(p-(a+ac*(d2/(d2-d6))));
 float va=d3*d6-d5*d4;if(va<=0&&d4-d3>=0&&d5-d6>=0&&d4-d3+d5-d6!=0)return segment_distance(p,b,c);
 const float sum=va+vb+vc;
 if(sum<=0||length(cross(ab,ac))==0)return std::min({segment_distance(p,a,b),segment_distance(p,a,c),segment_distance(p,b,c)});
 return length(p-(a+ab*(vb/sum)+ac*(vc/sum)));
}
Mesh world_mesh(const Mesh &rest,const AffineTransform &world,const std::function<bool()> &cancelled){
 Mesh mesh=rest;for(size_t v=0;v<mesh.positions.size();++v){if((v&1023)==0)stop(cancelled);mesh.positions[v]=point(world,rest.positions[v]);
  auto n=world.normal({rest.normals[v].x,rest.normals[v].y,rest.normals[v].z});mesh.normals[v]=normalized(Vec3{float(n[0]),float(n[1]),float(n[2])});}
 if(world.orientation()==AffineOrientation::Reversing)for(size_t i=0;i<mesh.indices.size();i+=3)std::swap(mesh.indices[i+1],mesh.indices[i+2]);
 mesh.validate();return mesh;
}
}
void RigFitOptions::validate() const {
 if(!std::isfinite(fit.maximum_surface_distance)||fit.maximum_surface_distance<=0||fit.maximum_surface_distance>10||
    !std::isfinite(fit.minimum_segment_length)||fit.minimum_segment_length<=0||fit.minimum_segment_length>10||
    !std::isfinite(fit.terminal_length)||fit.terminal_length<fit.minimum_segment_length||fit.terminal_length>10||
    !fit.maximum_work||fit.maximum_work>500'000'000||!binding.maximum_work||binding.maximum_work>500'000'000||
    !std::isfinite(binding.maximum_seed_distance)||binding.maximum_seed_distance<=0||binding.maximum_seed_distance>10000||
    !std::isfinite(binding.seed_margin)||binding.seed_margin<0||binding.seed_margin>binding.maximum_seed_distance||
    !std::isfinite(binding.falloff)||binding.falloff<1||binding.falloff>8||
    !binding.limits.max_influences||binding.limits.max_influences>32||!std::isfinite(binding.limits.prune)||binding.limits.prune<0||binding.limits.prune>=1)
  throw std::invalid_argument("Invalid humanoid fit distance, length, binding or work bounds");
}
HumanLandmarks rig_fit_guides(const SculptObject &o){
 if(!o.mesh_active||!o.painted||o.painted->mesh().positions.empty())throw std::invalid_argument("Select an original painting mesh for humanoid fitting");
 if(o.deformation&&o.deformation->landmarks)return *o.deformation->landmarks;
 HumanLandmarks l;
 if(o.deformation&&o.deformation->skeleton&&o.deformation->humanoid){
  const auto segments=o.deformation->skeleton->segments();
  for(const auto &[role,joint]:o.deformation->humanoid->joints)l.points.emplace(role,segments.at(joint)[0]);
  return l;
 }
 const auto &mesh=o.painted->mesh();Vec3 lo=mesh.positions[0],hi=lo;
 for(auto p:mesh.positions){lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
 const auto center=(lo+hi)*.5f;const float height=hi.y-lo.y,width=hi.x-lo.x;
 if(height<=0||width<=0)throw std::invalid_argument("Landmark guides require a nonempty height and width; supply explicit points for this orientation");
 auto p=[&](float x,float y){return Vec3{center.x+x*width,lo.y+y*height,center.z};};
 l.points={{"hips",p(0,.52f)},{"spine",p(0,.65f)},{"head",p(0,.91f)}};
 for(std::string side:{"left","right"}){const float sign=side=="left"?1:-1;
  l.points[side+"UpperLeg"]=p(sign*.1f,.51f);l.points[side+"LowerLeg"]=p(sign*.1f,.28f);l.points[side+"Foot"]=p(sign*.1f,.04f);
  l.points[side+"UpperArm"]=p(sign*.17f,.78f);l.points[side+"LowerArm"]=p(sign*.32f,.69f);l.points[side+"Hand"]=p(sign*.46f,.61f);}
 return l;
}
RigFitResult author_humanoid_fit(Scene &scene,uint64_t id,const HumanLandmarks &landmarks,const RigFitOptions &options,const std::function<bool()> &cancelled){
 stop(cancelled);options.validate();const auto &object=scene.object(id);
 if(!object.visible||object.locked||!object.mesh_active||!object.painted)throw std::invalid_argument("Select a visible, unlocked original painting mesh");
 if(object.deformation&&object.deformation->skeleton)throw std::invalid_argument("Existing rigs require the source-preserving rest-fit workflow");
 const auto &rest=object.painted->mesh();if(object.deformation)object.deformation->validate(rest);
 const auto world=scene.world_matrix(id),inverse=world.inverse();auto mesh=world_mesh(rest,world,cancelled);
 if(landmarks.points.size()>options.fit.maximum_work/std::max(size_t(1),mesh.indices.size()/3))throw std::length_error("World landmark distance exceeds work budget");
 for(const auto &[role,p]:landmarks.points){stop(cancelled);auto q=point(world,p);float distance=std::numeric_limits<float>::infinity();
  for(size_t i=0;i<mesh.indices.size();i+=3){if((i&3071)==0)stop(cancelled);distance=std::min(distance,triangle_distance(q,mesh.positions[mesh.indices[i]],mesh.positions[mesh.indices[i+1]],mesh.positions[mesh.indices[i+2]]));}
  if(distance>options.fit.maximum_surface_distance)throw std::invalid_argument("Landmark too far from world surface: "+role);
 }
 // The anatomical constraints remain model-space; only the independently
 // measured proximity and binding metric are world-space. The local bound is
 // conservative under shear. It never replaces the exact world check above.
 auto local_options=options.fit;
 auto norm=[](const AffineTransform &a){double sum=0;for(int row=0;row<3;++row)for(int column=0;column<3;++column){auto v=a.matrix()[column*4+row];sum+=v*v;}return std::sqrt(sum);};
 local_options.maximum_surface_distance=float(std::min(10.,norm(inverse)*options.fit.maximum_surface_distance));
 local_options.minimum_segment_length=float(options.fit.minimum_segment_length/norm(world));
 auto up=world.direction({landmarks.up.x,landmarks.up.y,landmarks.up.z});
 const double up_length=std::sqrt(up[0]*up[0]+up[1]*up[1]+up[2]*up[2]);
 local_options.terminal_length=float(options.fit.terminal_length/up_length);
 auto fit=fit_humanoid(rest,landmarks,local_options,cancelled);
 const auto segments=fit.skeleton.segments();
 for(size_t b=0;b<segments.size();++b){
  if(length(point(world,segments[b][1])-point(world,segments[b][0]))<options.fit.minimum_segment_length)
   throw std::invalid_argument("World humanoid segment is shorter than the minimum: "+fit.skeleton.bones[b].name);
  const auto parent=fit.skeleton.bones[b].parent;
  if(parent>=0&&length(point(world,segments[b][0])-point(world,segments[size_t(parent)][0]))<options.fit.minimum_segment_length)
   throw std::invalid_argument("World parent landmarks are too close: "+fit.skeleton.bones[b].name);
 }
 std::optional<SkinWeights> skin;
 if(options.bind){auto world_skeleton=fit.skeleton;world_skeleton.space_matrix=world.float_matrix();skin=automatic_bind(mesh,world_skeleton,options.binding,cancelled);}
 auto next=object.deformation?*object.deformation:DeformationState(rest);next.skeleton=std::move(fit.skeleton);next.skin=std::move(skin);next.humanoid=std::move(fit.mapping);next.landmarks=landmarks;next.pose.clear();next.validate(rest);
 RigFitResult report{next.skeleton->bones.size(),bool(next.skin),std::move(fit.notes)};
 auto owner=std::make_shared<const DeformationState>(std::move(next));Scene result=scene;result.object(id).deformation=std::move(owner);result.validate(false);stop(cancelled);scene=std::move(result);return report;
}
} // namespace dome
