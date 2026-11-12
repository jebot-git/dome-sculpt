// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-normal-conversion.hpp"
#include "normal-bake-frames.hpp"
#include "normal-bake-encoding.hpp"
#include "dome/tangents.hpp"
#include "dome/source-maps.hpp"
#include "paint-normal-access.hpp"
#include "emission-paint-access.hpp"
#include "emission-authority.hpp"
namespace dome::detail {
namespace {
bool touches(const std::array<std::array<double,2>,3> &t,double left,double bottom,double right,double top){
  for(const auto axis:std::array<std::array<double,2>,2>{{{1,0},{0,1}}}){
    double low=axis[0]*t[0][0]+axis[1]*t[0][1],high=low;
    for(auto p:t){const double x=axis[0]*p[0]+axis[1]*p[1];low=std::min(low,x);high=std::max(high,x);}
    if(high<axis[0]*left+axis[1]*bottom||low>axis[0]*right+axis[1]*top)return false;
  }
  for(size_t k=0;k<3;++k){const auto a=t[k],b=t[(k+1)%3];const double x=a[1]-b[1],y=b[0]-a[0];
    double low=x*t[0][0]+y*t[0][1],high=low;
    for(auto p:t){const double q=x*p[0]+y*p[1];low=std::min(low,q);high=std::max(high,q);}
    const double r0=x*(x>=0?left:right)+y*(y>=0?bottom:top),r1=x*(x>=0?right:left)+y*(y>=0?top:bottom);
    const double epsilon=1e-12*std::max({1.,std::abs(low),std::abs(high),std::abs(r0),std::abs(r1)});
    if(high<r0-epsilon||low>r1+epsilon)return false;
  }
  return true;
}
// Border texel centers may lie outside the triangle they filter into. Extend
// its actual affine varying fields there; do not clamp to a nearest interior
// normal or reuse a padding texel's different decoder. All directions still
// undergo the same finite/conditioning/quantization checks as interior data.
NormalBakeBasis extrapolate(const std::array<NormalBakeVertex,3> &v,
    std::array<double,3> bary,bool columns){
  NormalBakeBasis out{};
  for(size_t k=0;k<3;++k){out.n+=v[k].n*float(bary[k]);out.t+=v[k].t*float(bary[k]);out.b+=v[k].b*float(bary[k]);}
  if(columns){out.n=normal_bake_math::unit(out.n);out.t=normal_bake_math::unit(out.t);out.b=normal_bake_math::unit(out.b);}
  return out;
}
}
static CanonicalNormalConversionCandidate convert_native_normal_grid(
    const ColorPreparationInput &input,const NormalPreparationOptions &options,
    std::shared_ptr<const Image> intrinsic) {
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
  auto charge=[&](size_t n,size_t unit){
    if(n>remaining/unit)throw std::length_error("Canonical conversion exceeds its working memory budget");
    remaining-=n*unit;
  };
  charge(count,corner_bytes);charge(vertices,vertex_bytes);charge(pixels,96);
  auto frames=normal_bake_frame_candidate(mesh,input.deformation.get(),nullptr,
      {128*1024*1024,std::min(size_t(128*1024*1024),options.max_working_bytes),options.cancelled});
  stop();auto maps=intrinsic?std::shared_ptr<const TextureMaps>{}:
    (paint.has_physical_emission()||hdr_emission(input.material.emissive)
      ?EmissionExportAccess::other_channels(paint,input.material,options.cancelled)
      :paint.composite(input.material,options.cancelled));
  if(!intrinsic&&(maps->normal.rgba.empty()||maps->normal_projection||maps->source_only))
    throw std::invalid_argument("Canonical conversion lost its actual resolved legacy normal field");
  const auto &grid=intrinsic?*intrinsic:maps->normal;grid.validate();
  if(grid.width!=paint.resolution()||grid.height!=paint.resolution())
    throw std::invalid_argument("Canonical conversion normal grid differs from its owning paint");
  CanonicalNormalConversionCandidate result;
  const auto target=frames.frames->pbr;
  auto converted=std::make_shared<Image>(grid);
  const auto old_tangents=tangent_corners(mesh,options.cancelled);std::vector<uint8_t> visited(pixels);
  const int resolution=int(paint.resolution());
  for(size_t face=0;face<count/3;++face){
    stop();std::array<std::array<double,2>,3> uv;std::array<NormalBakeVertex,3> old_v,new_v;
    for(size_t k=0;k<3;++k){const size_t c=face*3+k;const auto v=mesh.indices[c];const auto p=mesh.uv.at(v);
      if(p[0]<0||p[0]>1||p[1]<0||p[1]>1)throw std::invalid_argument("Canonical conversion requires unit-atlas UVs");
      uv[k]={double(p[0])*resolution,double(p[1])*resolution};const auto t=old_tangents[c],nt=target->tangents[c];
      old_v[k]=normal_bake_vertex(mesh.normals[v],{t[0],t[1],t[2]},t[3],NormalVertexBitangent::RawCross);
      new_v[k]=normal_bake_vertex(target->normals[c],{nt[0],nt[1],nt[2]},nt[3],NormalVertexBitangent::UnitCross);
    }
    if(old_v[0].w!=old_v[1].w||old_v[0].w!=old_v[2].w||new_v[0].w!=new_v[1].w||new_v[0].w!=new_v[2].w)
      throw std::invalid_argument("Canonical conversion handedness changes inside a triangle");
    const double det=(uv[1][1]-uv[2][1])*(uv[0][0]-uv[2][0])+(uv[2][0]-uv[1][0])*(uv[0][1]-uv[2][1]);
    if(!std::isfinite(det)||std::abs(det)<1e-12)throw std::invalid_argument("Canonical conversion has singular UV derivatives");
    double lowx=uv[0][0],highx=lowx,lowy=uv[0][1],highy=lowy;
    for(auto p:uv){lowx=std::min(lowx,p[0]);highx=std::max(highx,p[0]);lowy=std::min(lowy,p[1]);highy=std::max(highy,p[1]);}
    const int x0=std::max(0,int(std::ceil(lowx-1.5))),x1=std::min(resolution-1,int(std::floor(highx+.5))),
      y0=std::max(0,int(std::ceil(lowy-1.5))),y1=std::min(resolution-1,int(std::floor(highy+.5)));
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
      if(++result.raster_samples>options.max_samples)throw std::length_error("Canonical conversion exceeds its sample budget");
      if((result.raster_samples&255)==0)stop();
      if(!touches(uv,std::max(0.,x-.5),std::max(0.,y-.5),std::min(double(resolution),x+1.5),std::min(double(resolution),y+1.5)))continue;
      const double u=x+.5,v=y+.5;std::array<double,3>bary{
        ((uv[1][1]-uv[2][1])*(u-uv[2][0])+(uv[2][0]-uv[1][0])*(v-uv[2][1]))/det,
        ((uv[2][1]-uv[0][1])*(u-uv[2][0])+(uv[0][0]-uv[2][0])*(v-uv[2][1]))/det,0};bary[2]=1-bary[0]-bary[1];
      const size_t at=size_t(y)*resolution+x;std::array<uint8_t,4> pixel;std::copy_n(grid.rgba.begin()+at*4,4,pixel.begin());
      const auto desired=normal_bake_evaluate(extrapolate(old_v,bary,false),pixel);
      const auto basis=extrapolate(new_v,bary,true);double error=0;
      if(visited[at]){
        std::copy_n(converted->rgba.begin()+at*4,4,pixel.begin());const auto actual=normal_bake_evaluate(basis,pixel);
        error=std::atan2(double(length(cross(desired,actual))),std::clamp(double(dot(desired,actual)),-1.,1.));
        if(error>options.max_angular_error)throw std::invalid_argument("Shared canonical normal footprint requires a compatible joint conversion or separate atlas");
      }else{
        const auto encoded=encode_normal_bake(basis,desired);
        if(!encoded||encoded->quantized_angle_radians>options.max_angular_error)
          throw std::invalid_argument("Canonical conversion exceeds the explicit center conditioning/quantization bound");
        std::copy_n(encoded->rgba.begin(),4,converted->rgba.begin()+at*4);error=encoded->quantized_angle_radians;visited[at]=1;++result.sampled_centers;
      }
      result.maximum_center_angular_error=std::max(result.maximum_center_angular_error,error);
    }
  }
  if(!result.sampled_centers)throw std::invalid_argument("Canonical conversion has no sampled native footprint");
  stop();
  auto projection=std::make_shared<PaintNormalProjection>();
  projection->pbr=projection->mtoon=std::move(converted);
  projection->validate(paint.resolution());
  result.original_paint=input.paint;result.original_deformation=input.deformation;
  result.target_frames=std::move(frames.frames);result.projection=std::move(projection);
  stop();return result;
}
CanonicalNormalConversionCandidate convert_native_canonical_normals(
    const ColorPreparationInput &input,const NormalPreparationOptions &options){
  return convert_native_normal_grid(input,options,{});
}
CanonicalNormalConversionCandidate convert_native_intrinsic_normals(
    const ColorPreparationInput &input,const NormalPreparationOptions &options){
  if(!input.paint)throw std::invalid_argument("Intrinsic baked baseline requires genuine input paint");
  const size_t bytes=size_t(input.paint->resolution())*input.paint->resolution()*4;
  if(bytes>=options.max_working_bytes)throw std::length_error("Intrinsic baked baseline exceeds working budget");
  auto reduced=options;reduced.max_working_bytes-=bytes;
  return convert_native_normal_grid(input,reduced,SourceNormalPreparationAccess::canonical_intrinsic(*input.paint,options.cancelled));
}
} // namespace dome::detail
