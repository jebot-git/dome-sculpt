// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-normal-conversion.hpp"
#include "normal-bake-frames.hpp"
#include "normal-bake-encoding.hpp"
#include "dome/bake.hpp"
#include "dome/source-maps.hpp"
#include <numeric>
namespace dome::detail {
CanonicalNormalConversionCandidate convert_native_canonical_normals(
    const ColorPreparationInput &input,const NormalPreparationOptions &options) {
  auto stop=[&]{if(options.cancelled&&options.cancelled())
    throw std::runtime_error("Canonical normal conversion cancelled");};stop();
  if(!std::isfinite(options.max_angular_error)||options.max_angular_error<0||options.max_angular_error>.25||
      !options.max_samples||options.max_samples>64'000'000||
      !options.max_working_bytes||options.max_working_bytes>512*1024*1024)
    throw std::invalid_argument("Invalid canonical normal conversion policy");
  if(!input.paint||input.paint->in_stroke()||input.paint->normal_projection())
    throw std::invalid_argument("Canonical conversion requires finished historical normal paint");
  const auto &paint=*input.paint;const auto baked=paint.baked();
  if(!baked||baked->normal_interpretation!=BakedNormalInterpretation::CanonicalMikk||baked->maps.normal.rgba.empty())
    throw std::invalid_argument("Canonical conversion requires the actual historical normal payload");
  if(input.appearance||input.material_bindings||paint.paint_sampling_domain()||paint.source_sampling_prepared())
    throw std::invalid_argument("Retained original/typed/corner normal conversion requires its owning adapter");
  const auto &diagnostics=paint.surface_diagnostics();
  if(diagnostics.policy!=PaintUvPolicy::Strict||diagnostics.additional_candidates||!diagnostics.unpaintable_uv_faces.empty())
    throw std::invalid_argument("Canonical conversion requires a strict unique writable UV atlas");
  const auto &mesh=paint.mesh();mesh.validate();baked->validate(paint.resolution());
  if(input.deformation)input.deformation->validate(mesh);
  const size_t count=mesh.indices.size(),vertices=mesh.positions.size(),pixels=size_t(paint.resolution())*paint.resolution();
  const size_t targets=input.deformation&&input.deformation->morphs?input.deformation->morphs->targets.size():0;
  if(targets>128||count>2'000'000||vertices>2'000'000||pixels>options.max_samples)
    throw std::length_error("Canonical conversion exceeds its geometry/sample budget");
  // Charge candidate N/T, target channels, detached corner generation, BVH,
  // source composite, bake images/owners and conservative container overhead.
  const size_t corner_bytes=256+targets*2*sizeof(Vec3),vertex_bytes=128;
  size_t remaining=options.max_working_bytes;
  auto charge=[&](size_t n,size_t unit){if(n>remaining/unit)
    throw std::length_error("Canonical conversion exceeds its working memory budget");remaining-=n*unit;};
  charge(count,corner_bytes);charge(vertices,vertex_bytes);charge(pixels,96);
  auto frames=normal_bake_frame_candidate(mesh,input.deformation.get(),nullptr,
      {128*1024*1024,std::min(size_t(128*1024*1024),options.max_working_bytes),options.cancelled});
  stop();auto maps=paint.composite(input.material,options.cancelled);
  if(maps->normal.rgba.empty()||maps->normal_projection||maps->source_only)
    throw std::invalid_argument("Canonical conversion lost its actual resolved legacy normal field");
  maps->normal.validate();
  BakeSource source;source.mesh=mesh;
  source.materials[0].normal_map=std::shared_ptr<const Image>(maps,&maps->normal);
  BakeOptions bake;bake.resolution=paint.resolution();bake.padding=8;bake.ao_samples=0;
  bake.max_texels=pixels;bake.max_raster_samples=options.max_samples;bake.cancelled=options.cancelled;
  bake.source_triangles.resize(count/3);std::iota(bake.source_triangles.begin(),bake.source_triangles.end(),uint32_t(0));
  bake.source_corner_barycentrics.resize(count);
  for(size_t c=0;c<count;++c){if((c&1023)==0)stop();bake.source_corner_barycentrics[c][c%3]=1;}
  CanonicalNormalConversionCandidate result;
  const auto target=frames.frames->pbr;
  bake.encode_target_normal=[&](uint32_t triangle,std::array<float,3> bary,Vec3 desired){
    std::array<NormalBakeVertex,3> v;
    for(size_t k=0;k<3;++k){const auto c=size_t(triangle)*3+k;const auto t=target->tangents.at(c);
      v[k]=normal_bake_vertex(target->normals.at(c),{t[0],t[1],t[2]},t[3],NormalVertexBitangent::UnitCross);}
    const auto basis=normal_bake_basis(v,{bary[0],bary[1],bary[2]},NormalFragmentBasis::UnitColumns);
    const auto encoded=encode_normal_bake(basis,desired);
    if(!encoded||encoded->quantized_angle_radians>options.max_angular_error)
      throw std::invalid_argument("Canonical conversion exceeds the explicit center conditioning/quantization bound");
    result.maximum_center_angular_error=std::max(result.maximum_center_angular_error,encoded->quantized_angle_radians);
    ++result.sampled_centers;return std::array{encoded->rgba,encoded->rgba};
  };
  auto output=bake_mesh(mesh,source,bake);stop();
  if(output.missed||output.overlapping||output.rays||result.sampled_centers!=output.projected)
    throw std::logic_error("Canonical conversion lost exact native triangle correspondence");
  auto projection=std::make_shared<PaintNormalProjection>();
  projection->pbr=std::make_shared<const Image>(std::move(output.normal));
  projection->mtoon=std::make_shared<const Image>(std::move(output.maps.mtoon_normal));
  projection->validate(paint.resolution());
  result.original_paint=input.paint;result.original_deformation=input.deformation;
  result.target_frames=std::move(frames.frames);result.projection=std::move(projection);
  result.raster_samples=output.raster_samples;stop();return result;
}
} // namespace dome::detail
