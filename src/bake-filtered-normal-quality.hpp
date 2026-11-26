// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh-processing.hpp"
#include "dome/normal-preparation.hpp"
#include "normal-bake-encoding.hpp"
#include "source-bake-normal.hpp"
namespace dome::detail {
// A sampled rest-space contract, independent from encoder quantization. The
// fixed quadrature samples are deliberately between texel centres. Never resize,
// smooth or change the caller's field to make this validation pass.
inline void verify_filtered_normal_quality(const Mesh &low,const BakeSource &high,
    const BakeOptions &options,const SourceBakeNormalResolver &source,
    const NormalFrameSet &frames,PreparedMToonNormalEvaluation mode,
    double toon_scale,double maximum_angle,BakeResult &result,
    const GenericBakeNormalSampler *generic=nullptr){
  using namespace normal_bake_math;
  const auto r=options.resolution;
  if(!frames.pbr||(!frames.mtoon&&mode!=PreparedMToonNormalEvaluation::Absent)||(!source&&(!generic||!bool(*generic)))||!std::isfinite(maximum_angle)||maximum_angle<0||
      frames.pbr->normals.size()!=low.indices.size()||frames.pbr->tangents.size()!=low.indices.size())
    throw std::invalid_argument("Filtered normal quality lost its actual target frame/projection owners");
  const auto &pbr=*frames.pbr;
  const auto &toon=mode==PreparedMToonNormalEvaluation::Absent?pbr:*frames.mtoon;
  if(toon.normals.size()!=low.indices.size()||toon.tangents.size()!=low.indices.size()||
      result.normal.width!=r||result.normal.height!=r||result.maps.mtoon_normal.width!=r||
      result.maps.mtoon_normal.height!=r||result.coverage.size()!=size_t(r)*r)
    throw std::invalid_argument("Filtered normal quality dimensions differ from their selected target grid");
  std::optional<SurfaceQuery> query;
  if(options.source_triangles.empty())query.emplace(high.mesh,options.cancelled);
  auto charge=[&]{
    if(!(result.raster_samples++%64)&&options.cancelled&&options.cancelled())
      throw std::runtime_error("Filtered normal transfer quality cancelled");
    if(result.raster_samples>options.max_raster_samples)
      throw std::length_error("Filtered normal transfer exceeds the aggregate raster-work budget");
  };
  auto edge=[](const std::array<float,2>&a,const std::array<float,2>&b,const std::array<double,2>&q){
    return (double(b[0])-a[0])*(q[1]-a[1])-(double(b[1])-a[1])*(q[0]-a[0]);};
  auto filtered=[&](const Image &im,int x,int y,double u,double v){
    D q{};
    for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
      const double w=(dx?u:1-u)*(dy?v:1-v);
      const size_t at=(size_t(y+dy)*r+size_t(x+dx))*4;
      for(size_t c=0;c<3;++c)q[c]+=(double(im.rgba.at(at+c))/255*2-1)*w;
    }
    if(options.normal_y_down)q[1]=-q[1];
    return q;
  };
  constexpr std::array<double,3> offsets{.25,.5,.75};
  uint64_t tested=0;
  for(size_t triangle=0;triangle<low.indices.size()/3;++triangle){
    std::array<std::array<float,2>,3> uv;
    std::array<NormalBakeVertex,3> pv,tv;
    for(size_t k=0;k<3;++k){const auto c=triangle*3+k;uv[k]=low.uv.at(low.indices.at(c));
      const auto a=pbr.tangents[c],b=toon.tangents[c];
      pv[k]=normal_bake_vertex(pbr.normals[c],{a[0],a[1],a[2]},a[3],NormalVertexBitangent::UnitCross);
      tv[k]=normal_bake_vertex(toon.normals[c],{b[0],b[1],b[2]},b[3],NormalVertexBitangent::RawCross);
    }
    const double area=edge(uv[0],uv[1],{uv[2][0],uv[2][1]});
    if(!std::isfinite(area)||std::abs(area)<1e-20)throw std::invalid_argument("Filtered normal quality has a singular target UV triangle");
    auto barycentric=[&](double x,double y){const std::array<double,2> q{x/r,y/r};
      return std::array<double,3>{edge(uv[1],uv[2],q)/area,edge(uv[2],uv[0],q)/area,edge(uv[0],uv[1],q)/area};};
    const int left=std::max(0,int(std::floor(std::min({uv[0][0],uv[1][0],uv[2][0]})*r-.5))),
      right=std::min(int(r)-2,int(std::ceil(std::max({uv[0][0],uv[1][0],uv[2][0]})*r-.5))),
      bottom=std::max(0,int(std::floor(std::min({uv[0][1],uv[1][1],uv[2][1]})*r-.5))),
      top=std::min(int(r)-2,int(std::ceil(std::max({uv[0][1],uv[1][1],uv[2][1]})*r-.5)));
    for(int y=bottom;y<=top;++y)for(int x=left;x<=right;++x){
      charge();bool interior=true;
      for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
        const auto bary=barycentric(x+.5+dx,y+.5+dy);
        interior&=*std::min_element(bary.begin(),bary.end())>1e-6&&result.coverage.at(size_t(y+dy)*r+size_t(x+dx))==1;
      }
      if(!interior)continue;
      for(double oy:offsets)for(double ox:offsets){
        charge();const auto weights=barycentric(x+.5+ox,y+.5+oy);
        const std::array<float,3> bary{float(weights[0]),float(weights[1]),float(weights[2])};
        Vec3 position{},normal{},cage{};
        for(size_t k=0;k<3;++k){const auto v=low.indices[triangle*3+k];
          position+=low.positions[v]*bary[k];normal+=low.normals[v]*bary[k];
          if(!options.cage_positions.empty())cage+=options.cage_positions.at(v)*bary[k];
        }
        normal=normal_bake_math::unit(normal);
        const Vec3 offset=options.cage_positions.empty()?normal*options.front_distance:cage-position;
        const float front=length(offset);const Vec3 direction=front>1e-8f?offset/front:normal;
        std::optional<SurfaceHit> hit;
        if(query){
          if(++result.rays>options.max_rays)throw std::length_error("Filtered normal transfer exceeds the aggregate ray budget");
          hit=query->raycast(position+offset,direction*-1,front+options.back_distance);
        }else{
          std::array<float,3> mapped{};for(size_t k=0;k<3;++k)for(size_t c=0;c<3;++c)mapped[c]+=bary[k]*options.source_corner_barycentrics.at(triangle*3+k)[c];
          hit=SurfaceHit{};hit->triangle=options.source_triangles.at(triangle);hit->barycentric=mapped;
          Vec3 projected{};for(size_t k=0;k<3;++k)projected+=high.mesh.normals.at(high.mesh.indices.at(size_t(hit->triangle)*3+k))*mapped[k];
          if(length(projected)<1e-8f){const auto c=size_t(hit->triangle)*3;const auto a=high.mesh.positions.at(high.mesh.indices.at(c)),b=high.mesh.positions.at(high.mesh.indices.at(c+1)),d=high.mesh.positions.at(high.mesh.indices.at(c+2));projected=cross(b-a,d-a);}
          hit->normal=normal_bake_math::unit(projected);
        }
        if(!hit)throw std::invalid_argument("Normal projection misses between texture samples; increase target paint resolution or adjust Bake projection");
        if(!options.allow_backfaces){
          const auto c=size_t(hit->triangle)*3;const auto a=high.mesh.positions.at(high.mesh.indices.at(c)),b=high.mesh.positions.at(high.mesh.indices.at(c+1)),d=high.mesh.positions.at(high.mesh.indices.at(c+2));
          if(dot(cross(b-a,d-a),normal)<=0)throw std::invalid_argument("Normal projection reaches a refused backface between texture samples; increase target paint resolution or adjust Bake projection");
        }
        const auto desired=source?source(hit->triangle,hit->barycentric):(*generic)(*hit);
        const auto pb=normal_bake_basis(pv,weights,NormalFragmentBasis::UnitColumns);
        const auto tb=mode==PreparedMToonNormalEvaluation::Absent?pb:normal_bake_basis(tv,weights,mode==PreparedMToonNormalEvaluation::LegacyRawInterpolated?NormalFragmentBasis::RawColumns:NormalFragmentBasis::UnitCross);
        for(size_t role=0;role<2;++role){
          auto q=filtered(role?result.maps.mtoon_normal:result.normal,x,y,ox,oy);
          if(role&&mode!=PreparedMToonNormalEvaluation::Absent){q[0]*=toon_scale;q[1]*=toon_scale;}
          const auto actual=normal_bake_math::unit(normal_bake_math::apply(role?tb:pb,q));
          const auto wanted=normal_bake_math::unit(normal_bake_math::vec(desired[role]));
          const auto difference=normal_bake_math::cross(actual,wanted);
          const double angle=std::atan2(std::sqrt(normal_bake_math::dot(difference,difference)),std::clamp(normal_bake_math::dot(actual,wanted),-1.,1.));
          if(!std::isfinite(angle)||angle>maximum_angle)
            throw std::invalid_argument("Baked normal detail exceeds the selected error between texture samples at "+std::to_string(r)+
                " px; choose a larger target paint resolution or adjust the projection. Target triangle "+
                std::to_string(triangle)+", source triangle "+std::to_string(hit->triangle)+
                (role?" (MToon): ":" (PBR): ")+std::to_string(angle*57.29577951308232)+
                " degrees, limit "+std::to_string(maximum_angle*57.29577951308232)+" degrees");
        }
        ++tested;
      }
    }
  }
  // An empty certificate must never authorize a texture with no independently
  // testable interior reconstruction footprint.
  if(!tested)throw std::invalid_argument("Target paint resolution has no interior normal reconstruction samples; choose a larger target paint resolution and Bake again");
  if(options.cancelled&&options.cancelled())throw std::runtime_error("Filtered normal transfer quality cancelled");
}
} // namespace dome::detail
