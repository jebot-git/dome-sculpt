// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "visibility-geodesic-bind.hpp"
#include "screened-continuation-solve.hpp"
namespace dome::detail {
namespace visibility_heat {
// Same bounded IC-PCG linear solve as continuation, with no probability-range
// requirement on a signed residual CORRECTION. Original fields are certified
// separately after one fixed true-residual iterative-refinement pass.
template <class System, class Budget, class Preconditioner>
void solve_linear(const System &s, std::span<const double> rhs,
                        std::span<double> x, Preconditioner &ic, Budget &budget,
                        ContinuationReport &report) {
  const size_t n = rhs.size();
  std::vector<double> r(rhs.begin(), rhs.end()), z(n), p(n), ap(n);
  std::fill(x.begin(), x.end(), 0);
  double squared = 0;
  for (double v : rhs) {
    budget.step();
    squared += v * v;
  }
  if (!squared)
    throw std::runtime_error("Continuation omitted an eligible control");
  double scale = std::sqrt(squared);
  ic.apply(r, z, budget);
  p = z;
  double rho = std::inner_product(r.begin(), r.end(), z.begin(), 0.);
  budget.step(n * 2);
  for (size_t iteration = 0; iteration < 4096; ++iteration) {
    budget.stop();
    s.multiply(p, ap, budget);
    double curvature = std::inner_product(p.begin(), p.end(), ap.begin(), 0.);
    budget.step(n);
    if (!(curvature > 0) || !(rho > 0) || !std::isfinite(curvature))
      throw std::runtime_error("Continuation PCG lost positive curvature");
    double alpha = rho / curvature, error = 0;
    budget.step(n * 2);
    for (size_t i = 0; i < n; ++i) {
      x[i] += alpha * p[i];
      r[i] -= alpha * ap[i];
      error += r[i] * r[i];
    }
    ++report.iterations;
    if (std::sqrt(error) <= 1e-9 * scale || iteration == 4095) {
      s.multiply(x, ap, budget);
      error = 0;
      budget.step(n);
      for (size_t i = 0; i < n; ++i) {
        r[i] = rhs[i] - ap[i];
        error += r[i] * r[i];
      }
      double residual = std::sqrt(error) / scale;
      if (residual <= 1e-9) {
        report.residual = std::max(report.residual, residual);
        return;
      }
      if (iteration == 4095)
        break;
      ic.apply(r, z, budget);
      p = z;
      rho = std::inner_product(r.begin(), r.end(), z.begin(), 0.);
      budget.step(n * 2);
      continue;
    }
    ic.apply(r, z, budget);
    double next = std::inner_product(r.begin(), r.end(), z.begin(), 0.);
    double beta = next / rho;
    budget.step(2 * n);
    for (size_t i = 0; i < n; ++i)
      p[i] = z[i] + beta * p[i];
    rho = next;
  }
  throw std::runtime_error("Continuation failed original residual bound");
}

template<class System,class Preconditioner>
void refined_solve(const System&s,std::span<const double>rhs,std::span<double>x,
    Preconditioner&ic,weight_surface::Budget&work,ContinuationReport&report){
  solve_linear(s,rhs,x,ic,work,report);
  std::vector<double>defect(rhs.size()),correction(rhs.size()),ap(rhs.size());
  s.multiply(x,ap,work);double norm=0,original=0;
  for(size_t i=0;i<rhs.size();++i){work.step(4);defect[i]=rhs[i]-ap[i];norm+=defect[i]*defect[i];original+=rhs[i]*rhs[i];}
  if(norm>0){solve_linear(s,defect,correction,ic,work,report);for(size_t i=0;i<x.size();++i){work.step();x[i]+=correction[i];}}
  s.multiply(x,ap,work);double error=0;
  for(size_t i=0;i<x.size();++i){work.step(4);const auto value=x[i];if(!std::isfinite(value)||value < -1e-9||value > 1+1e-9)throw std::runtime_error("Refined heat violates original maximum principle");report.minimum=std::min(report.minimum,value);const double e=rhs[i]-ap[i];error+=e*e;}
  const double residual=std::sqrt(error/original);
  if(!std::isfinite(residual)||residual>1e-9)throw std::runtime_error("Refined heat violates original replayed residual");report.residual=std::max(report.residual,residual);
}

struct Prepared {
  weight_surface::System system;
  size_t visible=0,occluded=0;
  std::vector<std::array<uint64_t,4>>component_sources;
};
// A distinct positive SURFACE screened energy, not a volumetric solve. The
// nearest/visibility domain is fixed before solving any field. Cotangent sums
// are clipped per edge to obtain a genuine M-matrix; this deviation from raw
// Blender FEM is reported, never a retry after an unsuccessful solve.
inline Prepared prepare(const Mesh&m,const Skeleton&rig,BindOptions options,
    weight_surface::Budget&work,visibility_geodesic::Bytes&bytes){
  const size_t n=m.positions.size(),corners=m.indices.size(),bones=rig.bones.size();
  if(!n||n>2'000'000||!corners||corners>6'000'000||corners%3||!bones||bones>256||
     !options.maximum_work||options.maximum_work>500'000'000||
     !std::isfinite(options.maximum_seed_distance)||options.maximum_seed_distance<=0||options.maximum_seed_distance>10000||
     !options.limits.max_influences||options.limits.max_influences>32||!std::isfinite(options.limits.prune)||options.limits.prune<0||options.limits.prune>=1)
    throw std::invalid_argument("Invalid positive visibility heat domain/options");
  // Concurrent CSR/map construction, exact owning BVH/scratch, source masks,
  // IC factors, all PCG vectors and staged top-K raw output are forecast first.
  bytes.add(65536);bytes.add(n,1024+2*(sizeof(VertexWeights)+options.limits.max_influences*sizeof(Influence)+64));bytes.add(corners,384);bytes.add(bones,2048+4*sizeof(RigMatrix));
  work.step_items(n,4);work.step_items(corners,4*std::bit_width(corners+1)+4);work.step_items(bones,256);
  m.validate();rig.validate();
  std::vector<bone_shadow::Point>points;points.reserve(n);for(auto p:m.positions){work.step(3);if(std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>10000)throw std::invalid_argument("Heat metric exceeds bounded coordinates");points.push_back({p.x,p.y,p.z});}
  visibility_geodesic::ClosestRayVisibility query(points,m.indices,bytes.maximum-bytes.used,work,true);bytes.add(query.planned_bytes);visibility_geodesic::ClosestRayVisibility::Scratch scratch(query);
  auto segments=rig.segments();std::vector<HeatSourceSegment>sources;sources.reserve(bones);for(uint32_t b=0;b<bones;++b)if(rig.bones[b].deform){work.step(8);auto a=segments[b][0],z=segments[b][1];sources.push_back({{a.x,a.y,a.z},{z.x,z.y,z.z},b});}
  NearestWeightSources nearest(sources,work);Prepared out;
  out.system=weight_surface::assemble(m,work,HeatOperator::PositiveGraph,{},HeatNormals::AreaWeighted,HeatMass::MixedVoronoi,HeatEdges::IncidentMean);auto&s=out.system;
  std::vector<uint8_t>supported(n);out.component_sources.resize(n);
  for(uint32_t row=0;row<n;++row){work.stop();auto mask=nearest.nearest(points[row],work);double minimum=INFINITY;size_t ties=0;
    for(size_t k=0;k<4;++k){auto bits=mask[k];while(bits){work.step(8);auto b=uint32_t(k*64+std::countr_zero(bits));bits&=bits-1;auto a=segments.at(b)[0],z=segments.at(b)[1];auto p=points[row];
      const double d=visibility_geodesic::distance(p,{a.x,a.y,a.z},{z.x,z.y,z.z});if(d>options.maximum_seed_distance)continue;
      if(!query.visible(row,{a.x,a.y,a.z},{z.x,z.y,z.z},scratch,work)){++out.occluded;continue;}
      minimum=std::min(minimum,d);s.seeds[row][k]|=uint64_t(1)<<(b%64);++ties;++out.visible;
    }}
    if(ties){
      // Pinned Blender varea accumulation is2*MixedVoronoi, then inverted as
      //0.5/varea. Incident-mean stiffness therefore has row factor1/(4M).
      // Multiplying that row by4M gives symmetric K +4M*H, H=ties/d^2.
      const double d=std::max(minimum,1e-4);s.heat[row]=4*s.mass[row]*double(ties)/(d*d);s.ties[row]=double(ties);
      if(!std::isfinite(s.heat[row])||!(s.heat[row]>0))throw std::runtime_error("Heat screen overflow");
      s.diagonal[row]+=s.heat[row];supported[s.parent[row]]=1;for(size_t k=0;k<4;++k)out.component_sources[s.parent[row]][k]|=s.seeds[row][k];
    }
  }
  for(size_t row=0;row<n;++row){work.step();if(!supported[s.parent[row]])throw std::runtime_error("Positive heat component has no visible nearest source");}
  return out;
}
}
inline VisibilityGeodesicResult visibility_heat_bind(const Mesh&m,const Skeleton&rig,BindOptions options={},
    const std::function<bool()>&cancelled={},size_t maximum_bytes=256u*1024u*1024u){
  if(maximum_bytes<65536||maximum_bytes>256u*1024u*1024u)throw std::invalid_argument("Heat workspace bound");
  weight_surface::Budget work{0,options.maximum_work,cancelled};work.stop();visibility_geodesic::Bytes bytes{0,maximum_bytes};
  auto prepared=visibility_heat::prepare(m,rig,options,work,bytes);auto&s=prepared.system;const size_t n=m.positions.size();
  ContinuationIC ic(s,work);std::vector<double>rhs(n),field(n),sum(n);std::vector<VertexWeights>rows(n);for(auto&r:rows)r.reserve(options.limits.max_influences+1);
  ContinuationReport proof;
  std::vector<uint32_t>sole(n,UINT32_MAX);
  for(size_t i=0;i<n;++i){work.step(8);auto mask=prepared.component_sources[s.parent[i]];size_t count=0;uint32_t joint=UINT32_MAX;for(size_t k=0;k<4;++k){count+=std::popcount(mask[k]);if(mask[k])joint=uint32_t(k*64+std::countr_zero(mask[k]));}if(count==1)sole[i]=joint;}
  // Exact connected-component minimizer: with one supported source the
  // screened M-matrix equation has x=1 for that source,0 for every other.
  // Eliminate these blocks prospectively, not after a numerical failure.

  for(uint32_t bone=0;bone<rig.bones.size();++bone)if(rig.bones[bone].deform){
    bool active=false;for(size_t row=0;row<n;++row){work.step(4);bool yes=(s.seeds[row][bone/64]&(uint64_t(1)<<(bone%64)))!=0;rhs[row]=yes&&sole[row]==UINT32_MAX?s.heat[row]/s.ties[row]:0;active|=rhs[row]>0;}
    if(active)visibility_heat::refined_solve(s,rhs,field,ic,work,proof);else std::fill(field.begin(),field.end(),0);
    for(size_t row=0;row<n;++row){work.step();if(sole[row]!=UINT32_MAX)field[row]=sole[row]==bone?1.:0.;}
    for(size_t row=0;row<n;++row){work.step(8);sum[row]+=field[row];if(field[row]<=0)continue;auto&r=rows[row];r.push_back({bone,float(field[row])});if(r.size()>options.limits.max_influences){auto small=std::min_element(r.begin(),r.end(),[&](auto a,auto b){work.step();return a.weight==b.weight?a.joint>b.joint:a.weight<b.weight;});r.erase(small);}}
  }
  VisibilityGeodesicResult result;result.negative_stiffness_edges=s.negative_edges;result.heat_iterations=proof.iterations;result.heat_residual=proof.residual;
  result.weights.vertices.reserve(n);for(size_t row=0;row<n;++row){work.step(64+rows[row].size()*8);result.partition_error=std::max(result.partition_error,std::abs(sum[row]-1));result.weights.vertices.push_back(normalize_weights(std::move(rows[row]),rig.bones.size(),{},options.limits));}
  if(result.partition_error>1e-7)throw std::runtime_error("Heat failed independently replayed pre-prune partition bound");
  work.step_items(n,options.limits.max_influences+1);result.weights.validate(n,rig.bones.size());work.stop();result.visible_seeds=prepared.visible;result.occluded_candidates=prepared.occluded;result.work=work.used;result.workspace=bytes.used;return result;
}
inline VisibilityGeodesicResult visibility_heat_bind_object(const SculptObject&object,const Skeleton&rig,
    BindOptions options={},const std::function<bool()>&cancelled={},size_t maximum_bytes=256u*1024u*1024u){
  return bind_visibility_object(object,rig,options,cancelled,maximum_bytes,
    [](const Mesh&m,const Skeleton&r,BindOptions o,const std::function<bool()>&c,size_t b){return visibility_heat_bind(m,r,o,c,b);});
}
}
