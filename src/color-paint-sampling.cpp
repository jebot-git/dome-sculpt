// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/color-paint-sampling.hpp"
#include "dome/polygon-color.hpp"
#include "paint-sampling-access.hpp"
#include "source-paint-pairing.hpp"
#include "polygon-color-target.hpp"
#include <numeric>
namespace dome {namespace {
void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Retained corner paint cancelled");}
bool same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
CornerPaintSamplingReceipt prove_colors(const PaintedMesh &before,
    const SourceMaterialBindings &material,const PolygonColorSurface &current,
    const SourceColorUvLayout &layout,const Mesh &target,const PaintSamplingLimits &limits){
  stop(limits.cancelled);const auto &prior=*material.color_preparation;
  const auto old=prior.current_descriptor();
  if(!old||!old->topology||!old->field||!current.topology||!current.field||
     old->field!=current.field||old->field.owner_before(current.field)||current.field.owner_before(old->field))
    throw std::invalid_argument("Retained paint requires the exact unchanged original current corner field");
  // Stable face/corner/edge/vertex IDs and non-UV topology are checked, not just
  // equal triangulation counts or convenient samples from an unrelated Origin.
  const auto &a=*old->topology,&b=*current.topology;
  if(a.next_id()!=b.next_id()||a.vertices().size()!=b.vertices().size()||a.edges().size()!=b.edges().size()||a.faces().size()!=b.faces().size())
    throw std::invalid_argument("Retained corner paint changed stable topology dimensions");
  uint64_t work=0;auto tick=[&]{if(++work>limits.max_work)throw std::length_error("Retained corner field proof exceeds work budget");if((work&1023)==0)stop(limits.cancelled);};
  for(size_t i=0;i<a.vertices().size();++i){tick();const auto &x=a.vertices()[i],&y=b.vertices()[i];if(x.id!=y.id||!same(x.position,y.position)||x.provenance!=y.provenance)throw std::invalid_argument("Retained corner paint changed original vertex identities");}
  for(size_t i=0;i<a.edges().size();++i){tick();const auto &x=a.edges()[i],&y=b.edges()[i];if(x.id!=y.id||x.a!=y.a||x.b!=y.b||x.hard!=y.hard)throw std::invalid_argument("Retained corner paint changed original edge identities");}
  for(size_t i=0;i<a.faces().size();++i){tick();const auto &x=a.faces()[i],&y=b.faces()[i];if(x.id!=y.id||x.material!=y.material||x.corners.size()!=y.corners.size())throw std::invalid_argument("Retained corner paint changed original face identities");for(size_t k=0;k<x.corners.size();++k){tick();const auto &v=x.corners[k],&w=y.corners[k];if(v.id!=w.id||v.vertex!=w.vertex||v.generated_uv!=w.generated_uv)throw std::invalid_argument("Retained corner paint changed original corner identities");}}
  const size_t color_bytes=(before.mesh().positions.size()+target.positions.size())*sizeof(std::array<float,4>);
  if(color_bytes>limits.max_working_bytes)throw std::length_error("Retained corner colour proof exceeds scratch budget");
  auto old_colors=detail::polygon_target_colors(before.mesh(),*old,limits.cancelled);
  auto new_colors=detail::polygon_target_colors(target,current,limits.cancelled);
  CornerPaintSamplingReceipt receipt;receipt.resolution=before.resolution();receipt.triangles=target.indices.size()/3;
  for(size_t c=0;c<target.indices.size();++c){if((c&1023)==0)stop(limits.cancelled);
    tick();const auto now=target.indices[c],was=layout.source_vertices[now];
    if(was!=before.mesh().indices[c])throw std::invalid_argument("Retained corner color lacks exact ordered source correspondence");
    for(size_t k=0;k<4;++k)receipt.maximum_corner_error[k]=std::max(receipt.maximum_corner_error[k],std::abs(new_colors[now][k]-old_colors[was][k]));
    ++receipt.tested_corners;
  }
  receipt.validate();return receipt;
}
}
void CornerPaintSamplingReceipt::validate() const {
  if(version!=1||resolution<64||resolution>2048||(resolution&(resolution-1))||!triangles||triangles>2'000'000||tested_corners!=triangles*3)
    throw std::invalid_argument("Invalid exact retained corner-field receipt");
  for(float e:maximum_corner_error)if(e!=0)throw std::invalid_argument("Retained writable grid cannot authorize changed corner RGBA");
}
bool SourceColorPaintSampling::matches_current(const PaintedMesh &paint) const noexcept {
  return attachment_&&paint.valid_paint_sampling_attachment()&&attachment_->matches(paint.mesh_owner(),paint.paint_sampling_domain());
}
void SourceColorPaintSampling::validate(bool deep,const std::function<bool()> &cancelled) const {
  stop(cancelled);receipt_.validate();
  if(!before_paint_||before_paint_->in_stroke()||!before_material_||!before_material_->color_preparation||
     !before_paint_->color_projection()||!domain_||!layout_||!current_||!sampling_||!attachment_||
     attachment_->domain()!=domain_||before_appearance_||before_paint_->resolution()!=receipt_.resolution)
    throw std::invalid_argument("Retained corner paint lacks its own sealed complete source");
  const auto &prior=*before_material_->color_preparation;
  if(prior.receipt.source_field!=SourceColorField::PolygonCorners||!prior.current_descriptor()||
     sampling_->field!=prior.current_descriptor()->field||current_->field!=sampling_->field)
    throw std::invalid_argument("Retained paint sampling and current fields have different authority");
  before_material_->validate(deep);detail::source_paint_pairing(before_paint_.get(),before_material_.get(),nullptr,deep);
  if(!deep)return;
  PaintSamplingLimits limits;limits.cancelled=cancelled;
  auto target=layout_->target_mesh(before_paint_->mesh(),cancelled);domain_->validate_current(target,limits);
  const auto &sealed=*attachment_->authored_geometry();
  if(sealed.indices!=target.indices||sealed.uv!=target.uv||sealed.uv_pins!=target.uv_pins||sealed.uv_seams!=target.uv_seams||
     sealed.positions.size()!=target.positions.size()||!std::equal(sealed.positions.begin(),sealed.positions.end(),target.positions.begin(),same)||!std::equal(sealed.normals.begin(),sealed.normals.end(),target.normals.begin(),same))
    throw std::invalid_argument("Retained color current mesh differs from its sealed REST layout");
  if(before_deformation_)before_deformation_->validate(before_paint_->mesh());
  const auto expected=prove_colors(*before_paint_,*before_material_,*current_,*layout_,target,limits);
  if(expected.tested_corners!=receipt_.tested_corners||expected.maximum_corner_error!=receipt_.maximum_corner_error)
    throw std::invalid_argument("Retained color receipt differs from its own exact corner replay");
}
struct ColorPaintSamplingAccess {
 static RetainedCornerPaint retain(std::shared_ptr<const PaintedMesh> before,
    std::shared_ptr<const SourceMaterialBindings> material,std::shared_ptr<const DeformationState> deformation,
    std::shared_ptr<const PolygonColorSurface> descriptor,const Mesh &target,
    std::span<const uint32_t> remap,const PaintSamplingLimits &limits){
  stop(limits.cancelled);
  if(!before||before->in_stroke()||!material||!material->color_preparation||!descriptor||
     material->color_preparation->receipt.source_field!=SourceColorField::PolygonCorners||
     before->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored)
    throw std::invalid_argument("Retained corner paint requires its own strictly prepared source");
  if(!limits.max_vertices||limits.max_vertices>2'000'000||!limits.max_triangles||limits.max_triangles>2'000'000||!limits.max_basis_bytes||limits.max_basis_bytes>128*1024*1024||!limits.max_working_bytes||limits.max_working_bytes>512*1024*1024||!limits.max_work||limits.max_work>256'000'000)
    throw std::invalid_argument("Invalid retained corner paint limits");
  if(target.positions.empty()||target.positions.size()>limits.max_vertices||target.indices.empty()||target.indices.size()%3||target.indices.size()/3>limits.max_triangles||remap.size()!=target.positions.size())
    throw std::length_error("Retained corner layout exceeds its bounded source domain");
  // Capture/validate geometry limits before allocating the layout/field scratch.
  std::vector<uint32_t> corners(target.indices.size());std::iota(corners.begin(),corners.end(),0u);
  auto domain=retain_paint_sampling(before->mesh(),before->paint_sampling_domain(),target,corners,limits);
  material->validate();detail::source_paint_pairing(before.get(),material.get(),nullptr,true);
  auto layout=std::make_shared<SourceColorUvLayout>();layout->source_vertices.assign(remap.begin(),remap.end());
  layout->indices=target.indices;layout->uv=target.uv;layout->pins=target.uv_pins;layout->seams=target.uv_seams;
  const auto exact=layout->target_mesh(before->mesh(),limits.cancelled);
  if(exact.positions.size()!=target.positions.size()||!std::equal(exact.positions.begin(),exact.positions.end(),target.positions.begin(),same)||!std::equal(exact.normals.begin(),exact.normals.end(),target.normals.begin(),same))
    throw std::invalid_argument("Retained paint target is not its own exact ordered REST derivative");
  auto receipt=prove_colors(*before,*material,*descriptor,*layout,target,limits);
  auto paint=retain_paint_grid(*before,target,domain,limits);
  auto node=std::shared_ptr<SourceColorPaintSampling>(new SourceColorPaintSampling);
  node->receipt_=receipt;node->domain_=std::move(domain);node->layout_=std::move(layout);
  node->current_=std::make_shared<const PolygonColorSurface>(*descriptor);
  node->sampling_=material->color_preparation->current_descriptor();
  node->before_paint_=std::move(before);node->before_material_=std::move(material);node->before_deformation_=std::move(deformation);
  node->attachment_=paint->paint_sampling_attachment();node->validate(true,limits.cancelled);stop(limits.cancelled);
  return {std::move(paint),std::move(node)};
 }
};
RetainedCornerPaint retain_source_corner_paint(std::shared_ptr<const PaintedMesh> before,
    std::shared_ptr<const SourceMaterialBindings> material,std::shared_ptr<const DeformationState> deformation,
    std::shared_ptr<const PolygonColorSurface> descriptor,const Mesh &target,
    std::span<const uint32_t> remap,const PaintSamplingLimits &limits){
 return ColorPaintSamplingAccess::retain(std::move(before),std::move(material),std::move(deformation),std::move(descriptor),target,remap,limits);
}
} // namespace dome
