// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-paint-metadata.hpp"
#include "native-paint-frame-source.hpp"
namespace dome::detail {namespace {
template<class T>bool own(const std::shared_ptr<const T>&a,const std::shared_ptr<const T>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
bool grid(const PaintedMesh &p){return !p.in_stroke()&&!p.source_maps()&&!p.color_projection()&&!p.normal_projection()&&!p.baked()&&!p.source_sampling_prepared()&&p.surface_diagnostics().policy==PaintUvPolicy::Strict;}
}
void native_paint_metadata(const std::shared_ptr<const NativePaintSampling>&root){
 if(!root||!root->valid_owner(root))throw std::invalid_argument("Native paint metadata requires its genuine immutable owner");
 auto node=root;uint32_t remaining=root->history_depth();if(!remaining||remaining>32)throw std::invalid_argument("Native paint metadata history exceeds bounded depth");
 while(node){
  if(!node->valid_owner(node)||node->history_depth()!=remaining||!node->before_paint()||!node->layout()||!node->domain()||!node->attachment())throw std::invalid_argument("Native paint metadata lost exact owning history");
  node->before_native_material().validate();const auto &paint=*node->before_paint();const auto &layout=*node->layout();const auto &domain=*node->domain();const auto &geometry=node->attachment()->authored_geometry();
  if(!grid(paint)||!geometry||!own(node->attachment()->domain(),node->domain()))throw std::invalid_argument("Native paint metadata changed its immutable basis/attachment interpretation");
  const auto n=geometry->positions.size(),c=geometry->indices.size();
  if(!n||n>2'000'000||!c||c>6'000'000||layout.source_vertices.size()!=n||layout.uv.size()!=n||layout.indices.size()!=c||(!layout.pins.empty()&&layout.pins.size()!=n)||(!layout.seams.empty()&&layout.seams.size()!=c)||paint.mesh().indices.size()!=c||domain.source_corners().size()!=c||!domain.basis()||domain.basis()->positions.size()>2'000'000||domain.basis()->indices.size()>6'000'000)throw std::invalid_argument("Native paint metadata dimensions differ from factory-sealed source geometry");
  const auto &before=node->before_material();auto previous=before?before->native_paint_sampling:nullptr;
  if(bool(previous)!=bool(paint.paint_sampling_domain())||(previous&&(!previous->valid_owner(previous)||!previous->matches_current(paint)||previous->history_depth()+1!=remaining))||(!previous&&remaining!=1))throw std::invalid_argument("Native paint metadata changed its prior current-paint owner");
  if(previous&&!own(previous->domain()->basis(),node->domain()->basis()))throw std::invalid_argument("Native paint metadata changed its actual original basis owner");
  node=std::move(previous);if(!remaining--)throw std::invalid_argument("Native paint metadata history underflow");
 }
 if(remaining)throw std::invalid_argument("Native paint metadata history is incomplete");
 // The before holder is a private true-const captured value. Its scalar/role
 // checks and any independent normal certificate remain mandatory, cheap and
 // bounded by the existing per-call material visitation ledger.
 if(root->before_material())root->before_material()->validate(false);
}
bool native_frame_preview_metadata(const std::shared_ptr<const SourceMaterialBindings> &binding,
    const std::shared_ptr<const PaintSamplingAttachment> &attachment){
 if(!binding||!binding->native_paint_frames)return false;
 if(!attachment||!binding->native_paint_sampling||!own(attachment,binding->native_paint_sampling->attachment()))throw std::invalid_argument("Native normal preview lost its actual current writable-paint attachment");
 binding->native_paint_frames->validate_binding(*binding,false);native_paint_metadata(binding->native_paint_sampling);return true;
}
} // namespace dome::detail
