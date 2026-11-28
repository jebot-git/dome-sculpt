// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "visibility-geodesic-bind.hpp"
#include "screened-continuation-solve.hpp"
#include "surface-heat-field.hpp"
#include "closed-mesh-source-domain.hpp"
namespace dome::detail {
enum class HeatDomainPolicy { IntrinsicSurface, ClosedMaterial, ComponentHandleSupport };
namespace visibility_heat {
struct Prepared {
  weight_surface::System system;
  size_t visible=0,occluded=0;
  size_t closed_components=0,interior_components=0,exterior_components=0,membership_rejected=0,nonembedded_components=0;
  std::vector<SupportMask> component_sources;
};
// A distinct positive SURFACE screened energy, not a volumetric solve. The
// nearest/visibility domain is fixed before solving any field. Cotangent sums
// are clipped per edge to obtain a genuine M-matrix; this deviation from raw
// Blender FEM is reported, never a retry after an unsuccessful solve.
inline Prepared prepare(const Mesh&m,const Skeleton&rig,BindOptions options,
    weight_surface::Budget&work,visibility_geodesic::Bytes&bytes,std::span<const HeatSourceSegment>input_sources={},HeatDomainPolicy policy=HeatDomainPolicy::IntrinsicSurface){
  const size_t n=m.positions.size(),corners=m.indices.size(),bones=rig.bones.size();
  if((policy!=HeatDomainPolicy::IntrinsicSurface&&policy!=HeatDomainPolicy::ClosedMaterial&&policy!=HeatDomainPolicy::ComponentHandleSupport)||!n||n>2'000'000||!corners||corners>6'000'000||corners%3||!bones||bones>256||
     !options.maximum_work||options.maximum_work>500'000'000||
     !std::isfinite(options.maximum_seed_distance)||options.maximum_seed_distance<=0||options.maximum_seed_distance>10000||
     !options.limits.max_influences||options.limits.max_influences>32||!std::isfinite(options.limits.prune)||options.limits.prune<0||options.limits.prune>=1)
    throw std::invalid_argument("Invalid positive visibility heat domain/options");
  // Concurrent CSR/map construction, exact owning BVH/scratch, source masks,
  // Support blocks, compact CSR, IC factors, device PCG vectors and
  // staged top-K output are forecast before constructing any field.
  bytes.add(65536);bytes.add(n,1024+2*(sizeof(VertexWeights)+options.limits.max_influences*sizeof(Influence)+64));bytes.add(corners,384);bytes.add(bones,2048+4*sizeof(RigMatrix));
  work.step_items(n,4);work.step_items(corners,4*std::bit_width(corners+1)+4);work.step_items(bones,256);
  m.validate();rig.validate();
  std::vector<bone_shadow::Point>points;points.reserve(n);for(auto p:m.positions){work.step(3);if(std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>10000)throw std::invalid_argument("Heat metric exceeds bounded coordinates");points.push_back({p.x,p.y,p.z});}
  auto query=std::make_shared<const visibility_geodesic::ClosestRayVisibility>(points,m.indices,bytes.maximum-bytes.used,work,true);bytes.add(query->planned_bytes);visibility_geodesic::ClosestRayVisibility::Scratch scratch(*query);
  std::optional<visibility_geodesic::ClosedMeshSourceDomain>solid;
  if(policy!=HeatDomainPolicy::IntrinsicSurface){solid.emplace(query,bytes.maximum-bytes.used,work,policy==HeatDomainPolicy::ComponentHandleSupport);bytes.add(solid->planned_bytes);}
  auto segments=rig.segments();std::vector<HeatSourceSegment>sources;sources.reserve(bones);for(uint32_t b=0;b<bones;++b)if(rig.bones[b].deform){work.step(8);auto a=segments[b][0],z=segments[b][1];sources.push_back({{a.x,a.y,a.z},{z.x,z.y,z.z},b});}
  if(!input_sources.empty()){
    if(input_sources.size()!=sources.size())throw std::invalid_argument("Declared heat sources omit actual deform identities");
    std::array<bool,256>seen{};sources.clear();
    for(const auto&source:input_sources){work.step(32);if(source.bone>=bones||!rig.bones[source.bone].deform||seen[source.bone])throw std::invalid_argument("Declared heat source identity repeated/nondeform");seen[source.bone]=true;
      for(size_t k=0;k<3;++k){const float a=float(source.head[k]),z=float(source.tail[k]);if(!std::isfinite(a)||!std::isfinite(z)||double(a)!=source.head[k]||double(z)!=source.tail[k]||std::max(std::abs(a),std::abs(z))>10000)throw std::invalid_argument("Declared heat source requires bounded captured binary32 geometry");}
      segments[source.bone][0]={float(source.head[0]),float(source.head[1]),float(source.head[2])};segments[source.bone][1]={float(source.tail[0]),float(source.tail[1]),float(source.tail[2])};sources.push_back(source);
    }
  }
  for(const auto&source:sources)for(const auto&p:{source.head,source.tail})for(double value:p){work.step();if(!std::isfinite(value)||std::abs(value)>10000)throw std::invalid_argument("Heat source exceeds exact bounded metric coordinates");}
  NearestWeightSources nearest(sources,work);Prepared out;
  out.system=weight_surface::assemble(m,work,HeatOperator::PositiveGraph,{},HeatNormals::AreaWeighted,HeatMass::MixedVoronoi,HeatEdges::IncidentMean);auto&s=out.system;
  enum class Support : uint8_t { None, Closest, Visible };
  std::vector<Support>support(n);out.component_sources.resize(n);
  size_t seeded_components=0;
  using Mask=SupportMask;std::vector<Mask>permitted;std::vector<uint8_t>classified,interior;
  if(solid){bytes.add(n,sizeof(Mask)+2*sizeof(uint8_t));permitted.resize(n);classified.resize(n);interior.resize(n);out.closed_components=solid->closed_components;out.nonembedded_components=solid->nonembedded_components;
    for(uint32_t row=0;row<n;++row)if(solid->closed(row)){auto component=solid->component(row);if(classified[component])continue;classified[component]=1;
      // Prospectively classify every finite source BEFORE nearest selection.
      // Only all-source strict exterior proof admits an attachment domain.
      // A possible crossing/contact never triggers an exterior solve retry.
      bool all_exterior=true;
      for(const auto&source:sources){work.step(8);bone_shadow::Point a{float(source.head[0]),float(source.head[1]),float(source.head[2])},b{float(source.tail[0]),float(source.tail[1]),float(source.tail[2])};
        if(policy==HeatDomainPolicy::ComponentHandleSupport){if(solid->has_interior_support(row,a,b,scratch,work)){permitted[component][source.bone/64]|=uint64_t(1)<<(source.bone%64);all_exterior=false;}else all_exterior&=solid->exterior(row,a,b,scratch,work);}
        else if(!solid->exterior(row,a,b,scratch,work))permitted[component][source.bone/64]|=uint64_t(1)<<(source.bone%64);
      }
      if(policy==HeatDomainPolicy::ComponentHandleSupport&&permitted[component]==Mask{}&&!all_exterior)throw std::runtime_error("Closed component has boundary-only handles without actual interior support");
      interior[component]=permitted[component]!=Mask{};if(interior[component])++out.interior_components;else ++out.exterior_components;
    }
  }
  const auto allowed=[&](uint32_t row)->Mask{
    if(solid&&solid->closed(row)&&interior[solid->component(row)])return permitted[solid->component(row)];
    return {UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX};
  };
  const auto distance=[&](uint32_t row,uint32_t joint){
    const auto [a,b]=segments.at(joint);
    return double(visibility_geodesic::distance(points[row],{a.x,a.y,a.z},{b.x,b.y,b.z}));
  };
  const auto admissible=[&](uint32_t row,uint32_t joint,double d){
    if(d>options.maximum_seed_distance)return false;
    const auto [a,b]=segments.at(joint);
    const bone_shadow::Point from{a.x,a.y,a.z},to{b.x,b.y,b.z};
    if(policy==HeatDomainPolicy::ClosedMaterial&&solid&&solid->closed(row)&&interior[solid->component(row)]&&
       !solid->nearest_material_closure(row,from,to,scratch,work)){
      ++out.membership_rejected;
      return false;
    }
    if(query->visible(row,from,to,scratch,work))return true;
    ++out.occluded;
    return false;
  };
  const auto add_seeds=[&](uint32_t row,Mask mask,Support kind){
    double minimum=INFINITY;
    size_t ties=0;
    for(size_t word=0;word<mask.size();++word){
      for(auto bits=mask[word];bits;bits&=bits-1){
        work.step(8);
        const auto joint=uint32_t(word*64+std::countr_zero(bits));
        const double d=distance(row,joint);
        if(kind==Support::Closest&&!admissible(row,joint,d))continue;
        minimum=std::min(minimum,d);
        s.seeds[row][word]|=uint64_t(1)<<(joint%64);
        ++ties;
      }
    }
    if(!ties)return;
    // Multiplying the reference row by four times the mixed Voronoi mass
    // produces the symmetric K + 4M*H system, with H = ties / distance².
    const double d=std::max(minimum,1e-4);
    s.heat[row]=4*s.mass[row]*double(ties)/(d*d);
    s.ties[row]=double(ties);
    if(!std::isfinite(s.heat[row])||!(s.heat[row]>0))throw std::runtime_error("Heat screen overflow");
    s.diagonal[row]+=s.heat[row];
    out.visible+=ties;
    const auto component=s.parent[row];
    if(support[component]==Support::None)++seeded_components;
    support[component]=kind;
    for(size_t word=0;word<mask.size();++word)out.component_sources[component][word]|=s.seeds[row][word];
  };
  for(uint32_t row=0;row<n;++row){
    work.stop();
    add_seeds(row,nearest.nearest(points[row],allowed(row),work),Support::Closest);
  }
  if(policy==HeatDomainPolicy::ComponentHandleSupport&&seeded_components<s.components){
    // Complete the support of initially unseeded diffusion components before
    // assembly of any right-hand side. Every admitted candidate still passes
    // the component handle mask, seed radius and exact closest-ray predicate.
    for(uint32_t row=0;row<n;++row){
      work.step();
      if(support[s.parent[row]]==Support::Closest)continue;
      const auto mask=nearest.nearest_if(points[row],allowed(row),work,[&](uint32_t joint){
        work.step(8);
        return admissible(row,joint,distance(row,joint));
      });
      add_seeds(row,mask,Support::Visible);
    }
  }
  for(size_t row=0;row<n;++row){work.step();if(support[s.parent[row]]==Support::None)throw std::runtime_error("Positive heat component has no admissible visible source within the seed radius");}
  return out;
}
}
inline VisibilityGeodesicResult visibility_heat_bind(const Mesh&m,const Skeleton&rig,BindOptions options={},
    const std::function<bool()>&cancelled={},size_t maximum_bytes=256u*1024u*1024u,std::span<const HeatSourceSegment>sources={},HeatDomainPolicy policy=HeatDomainPolicy::IntrinsicSurface){
  if(maximum_bytes<65536||maximum_bytes>256u*1024u*1024u)throw std::invalid_argument("Heat workspace bound");
  weight_surface::Budget work{0,options.maximum_work,cancelled};work.stop();visibility_geodesic::Bytes bytes{0,maximum_bytes};
  auto prepared=visibility_heat::prepare(m,rig,options,work,bytes,sources,policy);
  ContinuationReport proof;
  VisibilityGeodesicResult result;
  result.weights=visibility_heat::solve_supported_domains(prepared.system,prepared.component_sources,
      rig.bones.size(),options.limits,work,proof,result.heat_domains);
  result.negative_stiffness_edges=prepared.system.negative_edges;
  result.heat_iterations=proof.iterations;
  result.heat_residual=proof.residual;
  result.partition_error=result.heat_domains.partition_error;
  result.nonembedded_components=prepared.nonembedded_components;
  result.closed_components=prepared.closed_components;
  result.interior_components=prepared.interior_components;
  result.exterior_components=prepared.exterior_components;
  result.membership_rejected=prepared.membership_rejected;
  result.visible_seeds=prepared.visible;
  result.occluded_candidates=prepared.occluded;
  result.work=work.used;
  result.workspace=bytes.used;
  return result;
}
inline VisibilityGeodesicResult visibility_heat_bind_object(const SculptObject&object,const Skeleton&rig,
    BindOptions options={},const std::function<bool()>&cancelled={},size_t maximum_bytes=256u*1024u*1024u,std::span<const HeatSourceSegment>sources={},HeatDomainPolicy policy=HeatDomainPolicy::IntrinsicSurface){
  return bind_visibility_object(object,rig,options,cancelled,maximum_bytes,
    [sources,policy](const Mesh&m,const Skeleton&r,BindOptions o,const std::function<bool()>&c,size_t b){return visibility_heat_bind(m,r,o,c,b,sources,policy);});
}
}
