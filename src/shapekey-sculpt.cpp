// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/shapekey-sculpt.hpp"
#include "source-frame-skin.hpp"
namespace dome {
namespace {
RigMatrix inverse_linear(const RigMatrix &m){
 auto t=detail::source_frame_inverse_transpose(m);
 return {t[0],t[4],t[8],0,t[1],t[5],t[9],0,t[2],t[6],t[10],0,0,0,0,1};
}
Vec3 direction(const RigMatrix&m,Vec3 p){return detail::source_frame_direction(m,p);}
Vec3 point(const RigMatrix&m,Vec3 p){return direction(m,p)+Vec3{m[12],m[13],m[14]};}
void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Shapekey sculpt cancelled");}
}
void ShapeSculptDraft::consume(size_t count,const std::function<bool()> &cancelled){
 if(count>options_.maximum_work-work_)throw std::length_error("Shapekey sculpt work budget exceeded; cancel and use a shorter gesture");
 work_+=count;if((work_&1023)==0)stop(cancelled);
}
ShapeSculptDraft::ShapeSculptDraft(std::shared_ptr<const PaintedMesh> paint,
 std::shared_ptr<const DeformationState> deformation,size_t target,const RigMatrix &world,
 ShapeSculptOptions options,const std::function<bool()> &cancelled)
 :paint_(std::move(paint)),deformation_(std::move(deformation)),options_(options){
 stop(cancelled);
 if(!paint_||!deformation_||!options.maximum_vertices||options.maximum_vertices>2'000'000||
    !options.maximum_work||options.maximum_work>100'000'000)
  throw std::invalid_argument("Invalid shapekey sculpt source or resource bounds");
 const auto &rest=paint_->mesh();deformation_->validate(rest);
 if(!deformation_->morphs||target>=deformation_->morphs->targets.size())throw std::invalid_argument("Select an existing shapekey to sculpt");
 if(rest.positions.size()>options.maximum_vertices)throw std::length_error("Shapekey sculpt vertex budget exceeded");
 if(world[3]!=0||world[7]!=0||world[11]!=0||world[15]!=1)throw std::invalid_argument("Shapekey sculpt requires an affine world matrix");
 world_inverse_=inverse_linear(world);auto normal=detail::source_frame_inverse_transpose(world);
 original_=rest.positions;const auto &delta=deformation_->morphs->targets[target].delta;
 for(size_t v=0;v<original_.size();++v){consume(1,cancelled);original_[v]+=delta[v];}
 positions_=original_;coverage_.assign(original_.size(),0);neighbors_.resize(original_.size());
 if(deformation_->skeleton&&deformation_->skin&&(!deformation_->pose.empty()||deformation_->skeleton->inverse_bind_override))
  joints_=deformation_->skeleton->skin_matrices(deformation_->pose);
 auto posed=preview();world_positions_.reserve(original_.size());world_normals_.reserve(original_.size());
 for(size_t v=0;v<original_.size();++v){consume(1,cancelled);auto p=point(world,posed.positions[v]);auto n=direction(normal,posed.normals[v]);
  if(!finite(p)||!finite(n)||length(n)<1e-10f)throw std::invalid_argument("Shapekey sculpt surface frame is degenerate");
  world_positions_.push_back(p);world_normals_.push_back(normalized(n));
 }
 for(size_t t=0;t<rest.indices.size();t+=3)for(size_t c=0;c<3;++c){consume(1,cancelled);auto a=rest.indices[t+c],b=rest.indices[t+(c+1)%3];neighbors_[a].push_back(b);neighbors_[b].push_back(a);}
 for(auto &row:neighbors_){consume(1,cancelled);std::sort(row.begin(),row.end());row.erase(std::unique(row.begin(),row.end()),row.end());}
 stop(cancelled);
}
Vec3 ShapeSculptDraft::unpose_delta(size_t vertex,Vec3 change)const{
 auto local=direction(world_inverse_,change);if(joints_.empty())return local;
 RigMatrix blended{};blended[15]=1;
 for(auto influence:deformation_->skin->vertices[vertex]){
  const auto &m=joints_.at(influence.joint);
  for(size_t col=0;col<3;++col)for(size_t row=0;row<3;++row)blended[col*4+row]+=m[col*4+row]*influence.weight;
 }
 // The inverse of the blended POSITION matrix is required here. Blending
 // inverse joints (or inverse-transpose normal matrices) is not equivalent.
 return direction(inverse_linear(blended),local);
}
bool ShapeSculptDraft::stamp(const Brush &brush,const std::function<bool()> &cancelled){
 stop(cancelled);
 if(!finite(brush.from)||!finite(brush.to)||!finite(brush.normal)||length(brush.normal)<1e-10f||
    !std::isfinite(brush.radius)||brush.radius<=0||brush.radius>100||
    !std::isfinite(brush.pressure)||brush.pressure<0||brush.pressure>1||
    !std::isfinite(brush.strength)||brush.strength<0||brush.strength>1||
    brush.tool<Tool::Clay||brush.tool>Tool::Flatten||brush.operation!=BrushOperation::Sculpt||brush.symmetry_axes)
  throw std::invalid_argument("Invalid shapekey sculpt brush; use Sculpt without mask/symmetry for this gesture");
 if(!brush.pressure||!brush.strength)return false;
 auto candidate=positions_;auto covered=coverage_;auto normal=normalized(brush.normal);
 auto segment=brush.to-brush.from;auto square=dot(segment,segment);bool changed=false;
 for(size_t v=0;v<candidate.size();++v){consume(1,cancelled);
  if(options_.front_faces_only&&dot(world_normals_[v],normal)<=0)continue;
  auto p=world_positions_[v];float t=square>1e-20f?std::clamp(dot(p-brush.from,segment)/square,0.f,1.f):0;
  float u=std::clamp(1-length(p-(brush.from+segment*t))/brush.radius,0.f,1.f);
  float weight=u*u*(3-2*u)*brush.strength*brush.pressure;
  if(weight<=covered[v])continue;
  Vec3 displacement;
  if(brush.tool==Tool::Clay||brush.tool==Tool::Carve)displacement=normal*(brush.radius*.15f*(brush.tool==Tool::Clay?1.f:-1.f));
  else if(brush.tool==Tool::Flatten)displacement=normal*(-dot(p-brush.to,normal));
  else{if(neighbors_[v].empty())continue;Vec3 average;for(auto other:neighbors_[v]){consume(1,cancelled);average+=world_positions_[other];}displacement=average/float(neighbors_[v].size())-p;}
  auto next=original_[v]+unpose_delta(v,displacement*weight);
  if(!finite(next)||std::abs(next.x)>1e6f||std::abs(next.y)>1e6f||std::abs(next.z)>1e6f)throw std::invalid_argument("Shapekey sculpt moved outside the native coordinate bounds");
  changed|=next.x!=candidate[v].x||next.y!=candidate[v].y||next.z!=candidate[v].z;
  candidate[v]=next;covered[v]=weight;
 }
 stop(cancelled);positions_.swap(candidate);coverage_.swap(covered);changed_|=changed;return changed;
}
Mesh ShapeSculptDraft::preview()const{
 const auto &rest=paint_->mesh();MorphSet target(rest);target.set_target(rest,"Sculpt draft",positions_);
 auto result=target.evaluate(rest,std::array<float,1>{1});
 if(!joints_.empty())result=deform_mesh(result,*deformation_->skin,*deformation_->skeleton,deformation_->pose);
 return result;
}
} // namespace dome
