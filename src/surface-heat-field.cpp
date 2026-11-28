// SPDX-License-Identifier: GPL-3.0-or-later
#include "surface-heat-field.hpp"
#include "screened-continuation-solve.hpp"
#include "weight-surface-system.hpp"
#include <map>

#ifndef DOME_SURFACE_HEAT_REFERENCE
#include "surface-heat-device.hpp"

namespace dome::detail::visibility_heat {
namespace {
using surface_heat_device::SparseRows;
using enum surface_heat_device::Vector;

struct Matrix {
  std::vector<uint32_t> starts,columns;
  std::vector<double> values,diagonal;
  SparseRows transfer() const {return {starts,columns,values,diagonal};}
};

// Change only the elimination order of the preconditioner. The symmetric
// screened equation is permuted exactly. A colour is an independent set, so
// a single launch can safely compute every triangular row in that colour.
void color_rows(const weight_surface::System &system,std::vector<uint32_t> &order,
                std::vector<uint32_t> &ranges,weight_surface::Budget &work) {
  const auto n=system.diagonal.size();
  std::vector<uint32_t> color(n,UINT32_MAX),seen(n,UINT32_MAX);
  size_t colors=0;
  for(uint32_t row=0;row<n;++row){
    work.step();
    for(size_t at=system.starts[row];at<system.starts[row+1];++at){
      work.step();
      const auto neighbour=system.columns[at];
      if(neighbour<row)seen[color[neighbour]]=row;
    }
    uint32_t next=0;
    while(seen[next]==row){work.step();++next;}
    color[row]=next;
    colors=std::max(colors,size_t(next)+1);
  }
  ranges.assign(colors+1,0);
  order.resize(n);
  work.step(n*2+colors);
  for(auto c:color)++ranges[c+1];
  std::partial_sum(ranges.begin(),ranges.end(),ranges.begin());
  auto destinations=ranges;
  for(uint32_t row=0;row<n;++row)order[destinations[color[row]]++]=row;
}

Matrix permute(const weight_surface::System &system,std::span<const uint32_t> order,
               weight_surface::Budget &work) {
  if(system.columns.size()>UINT32_MAX)throw std::length_error("Surface Heat device edge index budget exceeded");
  Matrix matrix;
  const auto n=order.size();
  matrix.starts.resize(n+1);
  matrix.diagonal.resize(n);
  matrix.columns.reserve(system.columns.size());
  matrix.values.reserve(system.values.size());
  std::vector<uint32_t> inverse(n);
  for(uint32_t i=0;i<n;++i){work.step();inverse[order[i]]=i;}
  std::vector<std::pair<uint32_t,double>> edges;
  for(uint32_t i=0;i<n;++i){
    work.step();
    const auto row=order[i];
    matrix.diagonal[i]=system.diagonal[row];
    edges.clear();
    for(size_t at=system.starts[row];at<system.starts[row+1];++at){
      work.step();
      edges.emplace_back(inverse[system.columns[at]],system.values[at]);
    }
    std::sort(edges.begin(),edges.end(),[&](const auto &a,const auto &b){work.step();return a.first<b.first;});
    for(auto [column,value]:edges){work.step();matrix.columns.push_back(column);matrix.values.push_back(value);}
    matrix.starts[i+1]=uint32_t(matrix.columns.size());
  }
  return matrix;
}

template<class Factor>
Matrix transpose(const Factor &lower,weight_surface::Budget &work) {
  Matrix upper;
  const auto n=lower.diagonal().size();
  upper.starts.assign(n+1,0);
  upper.columns.resize(lower.columns().size());
  upper.values.resize(lower.values().size());
  work.step(n+2*lower.columns().size());
  for(auto column:lower.columns())++upper.starts[column+1];
  std::partial_sum(upper.starts.begin(),upper.starts.end(),upper.starts.begin());
  auto cursor=upper.starts;
  for(size_t i=n;i-->0;){
    for(size_t at=lower.starts()[i];at<lower.starts()[i+1];++at){
      const auto dest=cursor[lower.columns()[at]]++;
      upper.columns[dest]=uint32_t(i);
      upper.values[dest]=lower.values()[at];
    }
  }
  return upper;
}
} // namespace

struct FieldSolver::Impl {
  std::unique_ptr<surface_heat_device::Fields> device;
  std::vector<uint32_t> order,ranges;
  std::vector<double> staging;
  size_t matrix_edges=0,factor_edges=0;

  Impl(const weight_surface::System &system,weight_surface::Budget &work) {
    color_rows(system,order,ranges,work);
    auto matrix=permute(system,order,work);
    ContinuationIC lower(matrix,work);
    auto upper=transpose(lower,work);
    std::vector<uint32_t> lower_starts(lower.starts().begin(),lower.starts().end());
    const SparseRows below{lower_starts,lower.columns(),lower.values(),lower.diagonal()};
    staging.resize(order.size());
    matrix_edges=matrix.columns.size();
    factor_edges=lower.columns().size();
    work.stop();
    device=surface_heat_device::create({matrix.transfer(),below,upper.transfer()});
    work.stop();
  }

  double true_residual(surface_heat_device::Vector rhs,surface_heat_device::Vector x,
                       weight_surface::Budget &work) {
    work.step(3*order.size()+matrix_edges);
    device->multiply(Product,x);
    device->combine(Residual,rhs,-1.,Product);
    return device->dot(Residual,Residual);
  }

  void linear(surface_heat_device::Vector rhs,double norm_squared,
              weight_surface::Budget &work,ContinuationReport &proof) {
    if(!(norm_squared>0)||!std::isfinite(norm_squared))
      throw std::runtime_error("Surface Heat received an invalid control norm");
    const size_t n=order.size();
    const auto target=1e-18*norm_squared;
    const size_t sweeps=2*(ranges.size()-1);
    const size_t precondition_work=4*n+4*factor_edges+sweeps;
    const size_t iteration_work=7*n+matrix_edges+precondition_work;
    const size_t batch_limit=std::max(size_t(1),std::min({size_t(8),size_t(1'000'000)/n,size_t(256)/(sweeps+12)}));
    constexpr size_t max_iterations=4096;
    using Phase=surface_heat_device::IterationPhase;

    work.step(5*n+precondition_work);
    device->zero(Solution);
    device->copy(Residual,rhs);
    device->start_iterations(target,ranges);
    size_t used=0,previous=0;
    while(used<max_iterations){
      work.stop();
      const size_t steps=std::min({batch_limit,max_iterations-used,(work.maximum-work.used)/iteration_work});
      if(!steps)throw std::length_error("Bone heat work budget exceeded");
      const auto status=device->iterate(steps,ranges);
      if(status.completed<previous||status.completed-previous>steps||
         (status.phase==Phase::Running&&status.completed-previous!=steps))
        throw std::logic_error("Surface Heat GPU iteration count changed unexpectedly");
      const size_t completed=status.completed-previous;
      used+=completed;
      previous=status.completed;
      proof.iterations+=completed;
      work.step(completed*iteration_work);
      work.stop();
      if(status.phase==Phase::InvalidCurvature)
        throw std::runtime_error("Surface Heat GPU solve lost positive curvature");
      if(status.phase==Phase::InvalidResidual)
        throw std::runtime_error("Surface Heat GPU residual is nonfinite");
      if(status.phase==Phase::Running&&used<max_iterations)continue;
      const double actual=true_residual(rhs,Solution,work);
      if(actual>=0&&actual<=target){
        proof.residual=std::max(proof.residual,std::sqrt(actual/norm_squared));
        return;
      }
      if(used==max_iterations)break;
      work.step(precondition_work+2*n);
      device->start_iterations(target,ranges);
      previous=0;
    }
    throw std::runtime_error("Surface Heat GPU solve failed the original residual bound");
  }

  void solve(std::span<const double> rhs,std::span<double> field,weight_surface::Budget &work,
             ContinuationReport &proof) {
    const size_t n=order.size();
    work.step(2*n);
    for(size_t row=0;row<n;++row)staging[row]=rhs[order[row]];
    device->write(Right,staging);
    const double norm=device->dot(Right,Right);
    linear(Right,norm,work,proof);
    device->copy(Refined,Solution);
    const double defect=true_residual(Right,Refined,work);
    if(!(defect>=0)||!std::isfinite(defect))throw std::runtime_error("Surface Heat correction is nonfinite");
    if(defect>0){
      device->copy(Defect,Residual);
      linear(Defect,defect,work,proof);
      work.step(n);
      device->combine(Refined,Refined,1.,Solution);
    }
    const double residual=std::sqrt(true_residual(Right,Refined,work)/norm);
    if(!std::isfinite(residual)||residual>1e-9)
      throw std::runtime_error("Refined heat violates original replayed residual");
    proof.residual=std::max(proof.residual,residual);
    work.stop();
    device->read(Refined,staging);
    work.step(2*n);
    for(size_t row=0;row<n;++row){
      const auto value=staging[row];
      if(!std::isfinite(value)||value < -1e-9||value > 1+1e-9)
        throw std::runtime_error("Refined heat violates original maximum principle");
      proof.minimum=std::min(proof.minimum,value);
    }
    for(size_t row=0;row<n;++row)field[order[row]]=staging[row];
  }
};

} // namespace dome::detail::visibility_heat
#else

namespace dome::detail::visibility_heat {
namespace {
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

} // namespace

struct FieldSolver::Impl {
  const weight_surface::System &system;
  ContinuationIC<weight_surface::System,weight_surface::Budget> factor;
  std::vector<double> staging;

  Impl(const weight_surface::System &s,weight_surface::Budget &work)
    :system(s),factor(s,work),staging(s.diagonal.size()) {}
  void solve(std::span<const double> rhs,std::span<double> field,weight_surface::Budget &work,
             ContinuationReport &proof) {
    refined_solve(system,rhs,staging,factor,work,proof);
    work.step(field.size());
    work.stop();
    std::copy(staging.begin(),staging.end(),field.begin());
  }
};
} // namespace dome::detail::visibility_heat
#endif

namespace dome::detail::visibility_heat {
FieldSolver::FieldSolver(const weight_surface::System &system,weight_surface::Budget &work)
  :impl_(std::make_unique<Impl>(system,work)) {}
FieldSolver::~FieldSolver()=default;
void FieldSolver::solve(std::span<const double> rhs,std::span<double> field,
                        weight_surface::Budget &work,ContinuationReport &proof) {
  impl_->solve(rhs,field,work,proof);
}

namespace {
struct CoupledBlock {
  SupportMask support;
  std::vector<uint32_t> rows;
};
struct ComponentField {
  uint32_t block=UINT32_MAX;
  uint32_t sole_joint=UINT32_MAX;
};

std::vector<CoupledBlock> group_supported_rows(const weight_surface::System &system,
    std::span<const SupportMask> supports,size_t bones,std::span<uint32_t> compact_index,
    std::span<VertexWeights> weights,weight_surface::Budget &work,FieldDomainReport &report) {
  std::map<SupportMask,uint32_t> index_by_support;
  std::vector<ComponentField> components(supports.size());
  std::vector<CoupledBlock> blocks;
  for(uint32_t row=0;row<system.diagonal.size();++row){
    work.step();
    auto &component=components.at(system.parent[row]);
    if(component.block==UINT32_MAX&&component.sole_joint==UINT32_MAX){
      work.step(8);
      const auto &support=supports[system.parent[row]];
      uint32_t joint=UINT32_MAX;
      size_t count=0;
      for(size_t word=0;word<support.size();++word){
        const uint64_t bits=support[word];
        if(!bits)continue;
        count+=std::popcount(bits);
        joint=uint32_t(64*word+63-std::countl_zero(bits));
      }
      if(!count||joint>=bones)throw std::logic_error("Surface Heat component lost its supported joints");
      if(count==1)component.sole_joint=joint;
      else{
        work.step(16*std::bit_width(index_by_support.size()+1));
        auto [at,inserted]=index_by_support.try_emplace(support,uint32_t(blocks.size()));
        if(inserted)blocks.push_back({support,{}});
        component.block=at->second;
      }
    }
    if(component.sole_joint!=UINT32_MAX){
      // The screened equation on this connected component has the constant
      // solution one for its only supported joint and zero for other joints.
      weights[row].push_back({component.sole_joint,1.f});
      ++report.rigid_rows;
    }else{
      auto &rows=blocks[component.block].rows;
      compact_index[row]=uint32_t(rows.size());
      rows.push_back(row);
      ++report.coupled_rows;
    }
  }
  report.blocks=blocks.size();
  SupportMask all_coupled_sources{};
  for(const auto &block:blocks){
    work.step(block.support.size());
    for(size_t word=0;word<block.support.size();++word)all_coupled_sources[word]|=block.support[word];
  }
  for(auto word:all_coupled_sources)report.whole_mesh_field_rows+=system.diagonal.size()*std::popcount(word);
  return blocks;
}

weight_surface::System compact_matrix(const weight_surface::System &system,
    std::span<const uint32_t> rows,std::span<const uint32_t> compact_index,
    weight_surface::Budget &work) {
  weight_surface::System matrix;
  matrix.starts.reserve(rows.size()+1);
  matrix.starts.push_back(0);
  matrix.diagonal.reserve(rows.size());
  size_t edges=0;
  for(auto row:rows){work.step();edges+=system.starts[row+1]-system.starts[row];}
  matrix.columns.reserve(edges);
  matrix.values.reserve(edges);
  for(auto row:rows){
    work.step();
    matrix.diagonal.push_back(system.diagonal[row]);
    for(size_t at=system.starts[row];at<system.starts[row+1];++at){
      work.step();
      const auto neighbor=system.columns[at];
      if(system.parent[row]!=system.parent[neighbor])
        throw std::logic_error("Surface Heat component would cut a nonzero matrix entry");
      matrix.columns.push_back(compact_index[neighbor]);
      matrix.values.push_back(system.values[at]);
    }
    matrix.starts.push_back(matrix.columns.size());
  }
  return matrix;
}
} // namespace

SkinWeights solve_supported_domains(const weight_surface::System &system,std::span<const SupportMask> supports,
    size_t bones,WeightLimits limits,weight_surface::Budget &work,ContinuationReport &proof,
    FieldDomainReport &report) {
  const size_t n=system.diagonal.size();
  if(supports.size()!=n||system.parent.size()!=n)
    throw std::logic_error("Surface Heat support dimensions do not match the numerical field");
  SkinWeights weights;
  weights.vertices.resize(n);
  std::vector<uint32_t> compact_index(n);
  const auto blocks=group_supported_rows(system,supports,bones,compact_index,weights.vertices,work,report);
  const auto weaker=[&](Influence a,Influence b){
    work.step();
    return a.weight==b.weight?a.joint>b.joint:a.weight<b.weight;
  };

  for(const auto &block:blocks){
    work.stop();
    const auto count=block.rows.size();
    report.largest_block=std::max(report.largest_block,count);
    // The GPU only receives this block. All its components admit the same
    // joint set. For excluded joints the homogeneous equation is exactly zero.
    weight_surface::System compact;
    if(count!=n)compact=compact_matrix(system,block.rows,compact_index,work);
    FieldSolver solver(count==n?system:compact,work);
    std::vector<double> rhs(count),field(count),sum(count);
    for(auto row:block.rows){work.step();weights.vertices[row].reserve(limits.max_influences+1);}
    for(size_t word=0;word<block.support.size();++word){
      for(uint64_t bits=block.support[word];bits;bits&=bits-1){
        work.stop();
        const auto bone=uint32_t(64*word+std::countr_zero(bits));
        const uint64_t bit=uint64_t(1)<<(bone%64);
        for(size_t local=0;local<count;++local){
          work.step(4);
          const auto row=block.rows[local];
          rhs[local]=(system.seeds[row][word]&bit)?system.heat[row]/system.ties[row]:0.;
        }
        solver.solve(rhs,field,work,proof);
        report.field_rows+=count;
        for(size_t local=0;local<count;++local){
          work.step(8);
          sum[local]+=field[local];
          if(field[local]<=0)continue;
          auto &row=weights.vertices[block.rows[local]];
          row.push_back({bone,float(field[local])});
          if(row.size()>limits.max_influences)row.erase(std::min_element(row.begin(),row.end(),weaker));
        }
      }
    }
    work.step(count);
    for(auto value:sum)report.partition_error=std::max(report.partition_error,std::abs(value-1.));
  }
  if(report.partition_error>1e-7)throw std::runtime_error("Heat failed independently replayed pre-prune partition bound");
  for(auto &row:weights.vertices){
    work.step(64+row.size()*8);
    row=normalize_weights(std::move(row),bones,{},limits);
  }
  work.step_items(n,limits.max_influences+1);
  weights.validate(n,bones);
  work.stop();
  return weights;
}
} // namespace dome::detail::visibility_heat
