// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "baked-emission-domain.hpp"
#include "dome/mesh-processing.hpp"
#include <limits>
#include <sstream>
#include <iomanip>
namespace dome::detail {
// Version1 FLOAT transport policy. Samples the actual source hit and stored
// continuous output independently. This is roundoff allowance, not permission
// to blur physical source extrema. Work/rays share the existing Bake budgets.
inline void verify_filtered_emission_quality(const Mesh&low,const BakeSource&high,
 const BakeOptions&o,BakeResult&result,const std::shared_ptr<const BakedEmissionDomain>&domain){
 if(!result.emission_field)return;
 if(domain){
  // The factory proves exact ordered placed geometry and paint coordinates,
  // non-occluding supporting faces and ONE genuine source contribution grid
  // with a constant background. Its immutable source-grid coefficients retain
  // the same continuous function without reconstruction on the target grid.
  // Re-querying FLOAT ray barycentrics would add geometric/UV roundoff to this
  // exact correspondence, not measure resampling fidelity. The uncertified
  // raster route below continues to require its independent sampled check.
  if(!domain->valid_owner(domain)||domain->target_resolution()!=o.resolution)
   throw std::invalid_argument("FLOAT emission lost its genuine exact correspondence certificate");
  return;
 }
 const auto r=o.resolution;SurfaceQuery query(high.mesh,o.cancelled);uint64_t tested=0;
 auto charge=[&]{if(!(result.raster_samples++%64)&&o.cancelled&&o.cancelled())throw std::runtime_error("FLOAT emission transfer cancelled");if(result.raster_samples>o.max_raster_samples)throw std::length_error("FLOAT emission transfer exceeds aggregate raster budget");};
 auto edge=[](auto a,auto b,std::array<double,2>q){return (double(b[0])-a[0])*(q[1]-a[1])-(double(b[1])-a[1])*(q[0]-a[0]);};
 constexpr std::array<double,3>offsets{.25,.5,.75};
 for(size_t tri=0;tri<low.indices.size()/3;++tri){std::array<std::array<float,2>,3>uv;for(size_t k=0;k<3;++k)uv[k]=low.uv.at(low.indices[tri*3+k]);const double area=edge(uv[0],uv[1],{uv[2][0],uv[2][1]});if(!std::isfinite(area)||std::abs(area)<1e-20)throw std::invalid_argument("FLOAT emission target chart is singular");
  auto bary=[&](double x,double y){const std::array<double,2>q{x/r,y/r};return std::array<double,3>{edge(uv[1],uv[2],q)/area,edge(uv[2],uv[0],q)/area,edge(uv[0],uv[1],q)/area};};
  const int left=std::max(0,int(std::floor(std::min({uv[0][0],uv[1][0],uv[2][0]})*r-.5))),right=std::min(int(r)-2,int(std::ceil(std::max({uv[0][0],uv[1][0],uv[2][0]})*r-.5))),bottom=std::max(0,int(std::floor(std::min({uv[0][1],uv[1][1],uv[2][1]})*r-.5))),top=std::min(int(r)-2,int(std::ceil(std::max({uv[0][1],uv[1][1],uv[2][1]})*r-.5)));
  for(int y=bottom;y<=top;++y)for(int x=left;x<=right;++x){charge();bool inside=true;for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto b=bary(x+.5+dx,y+.5+dy);inside&=*std::min_element(b.begin(),b.end())>1e-6&&result.coverage.at(size_t(y+dy)*r+x+dx)==1;}if(!inside)continue;
   for(double oy:offsets)for(double ox:offsets){charge();const auto b=bary(x+.5+ox,y+.5+oy);const std::array<float,3>weights{float(b[0]),float(b[1]),float(b[2])};Vec3 position{},normal{},cage{};for(size_t k=0;k<3;++k){const auto v=low.indices[tri*3+k];position+=low.positions[v]*weights[k];normal+=low.normals[v]*weights[k];if(!o.cage_positions.empty())cage+=o.cage_positions.at(v)*weights[k];}normal=normalized(normal);const Vec3 offset=o.cage_positions.empty()?normal*o.front_distance:cage-position;const float front=length(offset);const auto direction=front>1e-8f?offset/front:normal;
    if(++result.rays>o.max_rays)throw std::length_error("FLOAT emission transfer exceeds aggregate ray budget");const auto hit=query.raycast(position+offset,direction*-1,front+o.back_distance);if(!hit)throw std::invalid_argument("FLOAT emission projection misses between texture samples");const auto material=high.triangle_materials.empty()?0:high.triangle_materials.at(hit->triangle);const auto&source=high.materials.at(material);if(!source.resolved_emission)throw std::invalid_argument("Mixed physical emission projection requires its explicit material-domain adapter");
    const auto wanted=source.resolved_emission(hit->triangle,hit->barycentric);const float u=float((x+.5+ox)/r),v=float((y+.5+oy)/r);const auto actual=(domain?domain->field():result.emission_field)->sample(u,v).resolve({},1);
    for(size_t k=0;k<3;++k){const double allowance=32*std::numeric_limits<float>::epsilon()*std::max({1.,double(wanted[k]),double(actual[k])});if(!std::isfinite(actual[k])||std::abs(double(actual[k])-wanted[k])>allowance){std::ostringstream message;message<<std::setprecision(17)<<"Physical emission cannot retain source fidelity on this target grid; use a certified exact source-domain correspondence; retained="<<bool(domain)<<" grid="<<r<<" target_triangle="<<tri<<" source_triangle="<<hit->triangle<<" uv="<<u<<','<<v<<" axis="<<k<<" wanted="<<wanted[k]<<" actual="<<actual[k]<<" error="<<std::abs(double(actual[k])-wanted[k])<<" allowance="<<allowance;throw std::invalid_argument(message.str());}}++tested;
   }
  }
 }
 if(!tested)throw std::invalid_argument("Target grid has no interior FLOAT emission reconstruction samples");
}
} // namespace dome::detail
