// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "geometry-rig-correspondence.hpp"
namespace dome::detail {
// One owning worker call only. Cache the SOURCE's canonical reframe, never a
// target comparison or a mutable/public proof. Every target field is replayed.
class GeometryReferenceReplay {
  std::shared_ptr<const DeformationState> source_;
  std::optional<Skeleton> canonical_;
  std::array<double,16> change_{};
  size_t builds_=0,workspace_=0;
  static bool matrix(const RigMatrix&a,const RigMatrix&b,weight_surface::Budget&w){
    for(size_t n=0;n<16;++n){w.step(4);if(!geometry_rig_correspondence::same_float(a[n],b[n]))return false;}return true;
  }
  static bool optional_matrix(const std::optional<RigMatrix>&a,const std::optional<RigMatrix>&b,weight_surface::Budget&w){
    w.step(2);if(bool(a)!=bool(b))return false;return !a||matrix(*a,*b,w);
  }
  static bool joint(const JointTransform&a,const JointTransform&b,weight_surface::Budget&w){
    const std::array x{a.translation.x,a.translation.y,a.translation.z,a.rotation.w,a.rotation.x,a.rotation.y,a.rotation.z,a.scale.x,a.scale.y,a.scale.z};
    const std::array y{b.translation.x,b.translation.y,b.translation.z,b.rotation.w,b.rotation.x,b.rotation.y,b.rotation.z,b.scale.x,b.scale.y,b.scale.z};
    for(size_t n=0;n<x.size();++n){w.step(4);if(!geometry_rig_correspondence::same_float(x[n],y[n]))return false;}
    return optional_matrix(a.full_local_matrix,b.full_local_matrix,w);
  }
  static bool rig(const Skeleton&a,const Skeleton&b,weight_surface::Budget&w){
    w.step(4);if(a.bones.size()!=b.bones.size()||bool(a.inverse_bind_override)!=bool(b.inverse_bind_override))return false;
    if(!optional_matrix(a.space_matrix,b.space_matrix,w))return false;
    if(a.inverse_bind_override){w.step(2);if(a.inverse_bind_override->size()!=b.inverse_bind_override->size())return false;
      for(size_t n=0;n<a.inverse_bind_override->size();++n)if(!matrix(a.inverse_bind_override->at(n),b.inverse_bind_override->at(n),w))return false;}
    for(size_t n=0;n<a.bones.size();++n){const auto&x=a.bones[n];const auto&y=b.bones[n];
      w.step(4);if(x.parent!=y.parent||x.deform!=y.deform||x.name.size()!=y.name.size())return false;
      w.step_items(x.name.size()+1,2);if(x.name!=y.name)return false;
      w.step(4);if(!geometry_rig_correspondence::same_float(x.length,y.length)||!joint(x.rest,y.rest,w)||!optional_matrix(x.rest_matrix,y.rest_matrix,w))return false;
    }return true;
  }
  static bool key(const std::array<double,16>&a,const std::array<double,16>&b,weight_surface::Budget&w){
    for(size_t n=0;n<16;++n){w.step(4);if(std::bit_cast<uint64_t>(a[n])!=std::bit_cast<uint64_t>(b[n]))return false;}return true;
  }
public:
  explicit GeometryReferenceReplay(std::shared_ptr<const DeformationState>source):source_(std::move(source)){
    if(!source_||!source_->skeleton)throw std::invalid_argument("Canonical replay needs its immutable reference owner");
  }
  GeometryReferenceReplay(const GeometryReferenceReplay&)=delete;
  GeometryReferenceReplay&operator=(const GeometryReferenceReplay&)=delete;
  GeometryReferenceReplay(GeometryReferenceReplay&&)=delete;
  GeometryReferenceReplay&operator=(GeometryReferenceReplay&&)=delete;
  size_t canonical_builds()const noexcept{return builds_;}
  size_t retained_workspace()const noexcept{return workspace_;}
  bool matches(const Skeleton&target,const AffineTransform&change,bool same_frame,
               weight_surface::Budget&work,size_t maximum_workspace,size_t*peak=nullptr){
    const auto&source=*source_->skeleton;
    // Relative frames retain their original exact, uncached path. The owning
    // caller computes same_frame from actual current world matrices.
    if(!same_frame){canonical_.reset();workspace_=0;return geometry_reference_rig_matches(source,target,change,false,work,maximum_workspace,peak);}
    if(canonical_){
      if(workspace_>maximum_workspace)throw std::length_error("Retained canonical replay exceeds current workspace");
      if(peak)*peak=std::max(*peak,workspace_);
    }
    work.step(4);if(source.bones.size()>256||target.bones.size()>256||source.bones.size()!=target.bones.size())return false;
    if(rig(source,target,work))return true;
    if(!canonical_||!key(change_,change.matrix(),work)){
      const size_t metadata=sizeof(*this)+source.bones.size()*(sizeof(Bone)+3*sizeof(RigMatrix)+128)+8192;
      if(metadata>maximum_workspace)throw std::length_error("Canonical reference replay workspace before allocation");
      size_t names=0;for(const auto&bone:source.bones){work.step();if(2*(bone.name.size()+1)>maximum_workspace-metadata-names)throw std::length_error("Canonical reference replay names before allocation");names+=2*(bone.name.size()+1);}
      if(peak)*peak=std::max(*peak,metadata+names);
      // Charge the existing conservative matrix/hierarchy validation once per
      // actual canonical build. Row/field comparisons above charge each field.
      work.step_items(names+1,2);
      work.step_items(source.bones.size()*2+1,8192);
      canonical_.reset();workspace_=0; // Never retain two allocating reframe copies.
      SkinGeometryWork geometry(work.maximum-work.used,work.cancel);const auto available=geometry.remaining();
      auto expected=skin_reframe(source,change,geometry);work.step(available-geometry.remaining());
      static_assert(std::is_nothrow_move_constructible_v<Skeleton>);
      canonical_.emplace(std::move(expected));change_=change.matrix();workspace_=metadata+names;++builds_;
    }
    return rig(*canonical_,target,work);
  }
};
} // namespace dome::detail
