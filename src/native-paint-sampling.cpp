// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-paint-sampling.hpp"
#include "paint-sampling-access.hpp"
#include <numeric>
#include <cmath>
namespace dome::detail {namespace {
void stop(const PaintSamplingLimits &limits){if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Native paint retention cancelled");}
bool equal(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
void same_mesh(const Mesh &a,const Mesh &b,const PaintSamplingLimits &limits){
  if(a.positions.size()!=b.positions.size()||a.normals.size()!=b.normals.size()||a.indices!=b.indices||a.uv!=b.uv||a.uv_pins!=b.uv_pins||a.uv_seams!=b.uv_seams)
    throw std::invalid_argument("Native retained paint differs from its exact ordered REST layout");
  for(size_t i=0;i<a.positions.size();++i){if((i&1023)==0)stop(limits);if(!equal(a.positions[i],b.positions[i])||!equal(a.normals[i],b.normals[i]))throw std::invalid_argument("Native retained paint changed REST positions or normals");}
}
}
void NativePaintSampling::validate(const PaintSamplingLimits &limits) const {
  stop(limits);native_.validate();
  if(!before_||!layout_||!domain_||!attachment_||before_->in_stroke()||attachment_->domain()!=domain_)
    throw std::invalid_argument("Native retained paint lacks its independently captured owner");
  const auto target=layout_->target_mesh(before_->mesh(),limits.cancelled);
  domain_->validate_current(target,limits);same_mesh(target,*attachment_->authored_geometry(),limits);
}
void NativePaintSampling::validate_current(const PaintedMesh &paint,const PaintSamplingLimits &limits) const {
  validate(limits);
  if(!paint.valid_paint_sampling_attachment()||paint.paint_sampling_domain()!=domain_||
      !attachment_->matches(paint.mesh_owner(),paint.paint_sampling_domain())||paint.resolution()!=before_->resolution())
    throw std::invalid_argument("Native retained paint borrowed another current attachment or grid");
}
RetainedNativePaint retain_native_paint(const SculptObject &source,Mesh target,
    std::span<const uint32_t> vertices,const PaintSamplingLimits &limits){
  stop(limits);
  // Initial detached slice. Existing prepared/typed/image/rig/history routes
  // keep their guards until this independent owner can genuinely compose them.
  if(!source.mesh_active||!source.painted||source.painted->in_stroke()||source.material_bindings||
     source.appearance||source.deformation||source.shape_source||source.rig_source||source.polygon||
     source.painted->source_maps()||source.painted->color_projection()||source.painted->normal_projection()||source.painted->baked()||
     source.painted->source_sampling_prepared()||source.painted->paint_sampling_domain()||source.painted->surface_diagnostics().policy!=PaintUvPolicy::Strict)
    throw std::invalid_argument("Native retention prototype requires its own unprepared native grid");
  if(!limits.max_vertices||limits.max_vertices>2'000'000||!limits.max_triangles||limits.max_triangles>2'000'000||
     !limits.max_basis_bytes||limits.max_basis_bytes>128*1024*1024||!limits.max_working_bytes||limits.max_working_bytes>512*1024*1024||!limits.max_work||limits.max_work>256'000'000)
    throw std::invalid_argument("Invalid native retained paint limits");
  const auto &before=source.painted->mesh();
  if(before.positions.size()>limits.max_vertices||target.positions.size()>limits.max_vertices||
     before.indices.size()/3>limits.max_triangles||target.indices.size()/3>limits.max_triangles||
     vertices.size()!=target.positions.size()||before.indices.size()!=target.indices.size())
    throw std::length_error("Native retained layout exceeds geometry budget");
  const size_t basis_bytes=before.positions.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+before.indices.size()*2*sizeof(uint32_t)+before.uv_pins.size()+before.uv_seams.size();
  if(basis_bytes>limits.max_basis_bytes)throw std::length_error("Native retained basis exceeds budget before allocation");
  const uint64_t pixels=uint64_t(source.painted->resolution())*source.painted->resolution();
  uint64_t work=pixels*16+uint64_t(before.positions.size())*8+uint64_t(before.indices.size())*16;
  if(work>limits.max_work)throw std::length_error("Native retained fixed work exceeds budget before allocation");
  for(size_t c=0;c<before.indices.size();c+=3){
    if((c&1023)==0)stop(limits);
    const auto a=before.uv.at(before.indices[c]),b=before.uv.at(before.indices[c+1]),d=before.uv.at(before.indices[c+2]);
    const int r=int(source.painted->resolution());
    auto extent=[&](size_t axis){const int low=std::max(0,int(std::floor(std::min({a[axis],b[axis],d[axis]})*r))),high=std::min(r-1,int(std::ceil(std::max({a[axis],b[axis],d[axis]})*r)));return high<low?uint64_t(0):uint64_t(high-low+1);};
    const auto samples=extent(0)*extent(1);if(samples>limits.max_work-work)throw std::length_error("Native retained raster work exceeds budget before allocation");work+=samples;
  }
  size_t metadata=sizeof(NativePaintSampling),scratch=0;
  if(metadata>limits.max_working_bytes)throw std::length_error("Native retained owner exceeds working budget before allocation");
  auto charge=[&](size_t n,size_t unit){if(n>limits.max_working_bytes/unit||n*unit>limits.max_working_bytes-metadata)throw std::length_error("Native retained metadata exceeds working budget before allocation");metadata+=n*unit;};
  for(const auto &layer:source.painted->layers()){charge(layer.name.size()+1,3);charge(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size(),3*sizeof(std::shared_ptr<const void>));}
  charge(target.positions.size(),sizeof(uint32_t)+sizeof(std::array<float,2>));charge(target.indices.size(),2*sizeof(uint32_t));charge(target.uv_pins.size()+target.uv_seams.size(),1);
  scratch=metadata;auto budget=limits;if(scratch>=budget.max_working_bytes)throw std::length_error("Native retained metadata exhausts working budget");budget.max_working_bytes-=scratch;budget.max_basis_bytes=std::min(budget.max_basis_bytes,budget.max_working_bytes);
  auto layout=std::make_shared<SourceNormalUvLayout>();layout->source_vertices.assign(vertices.begin(),vertices.end());layout->indices=target.indices;layout->uv=target.uv;layout->pins=target.uv_pins;layout->seams=target.uv_seams;
  const auto replay=layout->target_mesh(before,limits.cancelled);same_mesh(target,replay,limits);
  std::vector<uint32_t> corners(target.indices.size());std::iota(corners.begin(),corners.end(),0u);
  auto domain=retain_paint_sampling(before,{},target,corners,budget);
  // Freeze mutable layer names/visibility/opacity and tile tables. Surface and
  // immutable pixel owners remain shared; current metadata cannot edit anchor.
  auto captured=source.painted->snapshot();
  auto paint=retain_paint_grid(*captured,std::move(target),domain,budget);
  auto owner=std::shared_ptr<NativePaintSampling>(new NativePaintSampling);owner->before_=std::move(captured);owner->native_=source.material;owner->layout_=std::move(layout);owner->domain_=std::move(domain);owner->attachment_=paint->paint_sampling_attachment();
  owner->validate_current(*paint,limits);stop(limits);return {std::move(paint),std::move(owner)};
}
} // namespace dome::detail
