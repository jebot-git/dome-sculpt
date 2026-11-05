// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-paint-uv.hpp"
#include "paint-sampling-access.hpp"
#include "source-paint-pairing.hpp"
#include "baked-normal-field.hpp"
#include <numeric>
namespace dome::detail {
CanonicalPaintUvGeometry stage_canonical_paint_uv(const SculptObject &source,const Mesh &target,
 std::span<const uint32_t> vertices,const PaintSamplingLimits &limits){
 auto stop=[&]{if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Canonical writable UV stage cancelled");};stop();
 if(!limits.max_working_bytes||limits.max_working_bytes>512*1024*1024||!limits.max_vertices||limits.max_vertices>2'000'000||!limits.max_triangles||limits.max_triangles>2'000'000)
  throw std::invalid_argument("Invalid canonical writable UV stage limits");
 const auto paint_source=source.painted;const auto binding=source.material_bindings;const auto origin=binding?binding->canonical_attributes:nullptr;
 if(!source.mesh_active||!paint_source||paint_source->in_stroke()||!origin||!origin->valid_owner(origin)||
    binding->has_mtoon||binding->normal_preparation||binding->color_preparation||binding->native_paint_sampling||
    binding->key_derivation||binding->normal_frames!=origin->frames()||source.appearance||source.polygon||
    paint_source->surface_diagnostics().policy!=PaintUvPolicy::Strict||paint_source->source_maps()||paint_source->source_sampling_prepared()||paint_source->normal_projection()||paint_source->color_projection())
  throw std::invalid_argument("Canonical writable UV stage needs its genuine supported current normal/grid owners");
 const auto &live=*paint_source;const auto &mesh=live.mesh();
 if(vertices.size()!=target.positions.size()||target.positions.size()>limits.max_vertices||target.indices.size()/3>limits.max_triangles||target.indices.size()!=mesh.indices.size()||target.indices.size()%3)
  throw std::invalid_argument("Canonical writable UV correspondence dimensions differ");
 // Reserve live source, both metadata snapshots, normal-coordinate geometry and
 // explicit maps before the first output allocation. The retained-grid helper
 // additionally charges its actual domain/raster/SurfaceMap working storage.
 size_t used=0;auto charge=[&](size_t count,size_t unit){if(used>limits.max_working_bytes||count>(limits.max_working_bytes-used)/unit)throw std::length_error("Canonical writable UV aggregate working budget exceeded");used+=count*unit;};
 charge(live.storage_bytes(),1);charge(binding->normal_frames->storage_bytes(),1);charge(1,sizeof(CanonicalPaintUvGeometry));
 for(size_t copy=0;copy<2;++copy){charge(1,sizeof(PaintedMesh));charge(live.layers().size(),sizeof(PaintLayer));for(const auto &layer:live.layers()){charge(layer.name.size()+1,1);charge(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size(),sizeof(std::shared_ptr<const PaintTile>));}}
 charge(vertices.size(),sizeof(uint32_t));charge(target.indices.size(),sizeof(uint32_t));
 for(size_t c=0;c<target.indices.size();++c){if((c&1023)==0)stop();const auto v=target.indices[c];if(v>=vertices.size()||vertices[v]!=mesh.indices[c])throw std::invalid_argument("Canonical writable UV changed exact ordered source corners");}
 auto captured=live.snapshot();const auto &before=*captured;source_paint_pairing(captured.get(),binding.get(),nullptr,true);binding->validate();target.validate();
 if(const auto &field=before.baked()?before.baked()->normal_field:nullptr){if(field->origin()!=origin)throw std::invalid_argument("Canonical writable UV borrowed another normal field origin");field->validate();}
 CanonicalNormalUvOptions normal_limits;normal_limits.max_working_bytes=limits.max_working_bytes-used;normal_limits.cancelled=limits.cancelled;
 auto normal=binding->canonical_uv?binding->canonical_uv->reparameterize(target,vertices,normal_limits):CanonicalNormalUv::create(origin,target,vertices,normal_limits);
 charge(normal->storage_bytes(),1);auto grid_limits=limits;grid_limits.max_working_bytes-=used;grid_limits.max_basis_bytes=std::min(grid_limits.max_basis_bytes,grid_limits.max_working_bytes);
 std::vector<uint32_t> corners(target.indices.size());std::iota(corners.begin(),corners.end(),0);
 auto domain=retain_paint_sampling(mesh,before.paint_sampling_domain(),target,corners,grid_limits);
 auto paint=retain_paint_grid_borrowed(before,target,domain,grid_limits);
 if(paint->baked()!=before.baked()||paint->normal_projection()!=before.normal_projection()||paint->source_maps()!=before.source_maps())throw std::logic_error("Detached canonical UV altered independent immutable paint/normal metadata");
 CanonicalPaintUvGeometry out;out.before_paint=std::move(captured);out.paint=std::move(paint);out.normal_uv=std::move(normal);out.domain=std::move(domain);out.current_sources.assign(vertices.begin(),vertices.end());stop();return out;
}
}
