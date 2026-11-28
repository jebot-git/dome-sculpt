// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "surface-heat-device.hpp"
#include "softfloat/dome_f64.hpp"
#include <cstddef>

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_HEAT_FN __host__ __device__ inline
#else
#define DOME_HEAT_FN inline
#endif

namespace dome::detail::surface_heat_device {
using Scalar=uint64_t;
using namespace softfloat64;
struct IterationState {
  IterationProgress progress;
  Scalar target, rho, scale;
};
inline constexpr Scalar sign_bit=UINT64_C(0x8000000000000000);
inline constexpr Scalar infinity=UINT64_C(0x7ff0000000000000);
DOME_HEAT_FN bool running(const IterationState *state) {
  return !state||state->progress.phase==IterationPhase::Running;
}
DOME_HEAT_FN bool positive_finite(Scalar value) {return value>0&&value<infinity;}

struct StartIterations {
  IterationState *state;
  Scalar target;
  DOME_HEAT_FN void operator()(size_t) const {
    *state={{0,IterationPhase::Running},target,0,0};
  }
};
struct SparseView {
  const uint32_t *starts, *columns;
  const Scalar *coefficients, *diagonal;
};
struct Zero {
  Scalar *output;
  DOME_HEAT_FN void operator()(size_t row) const {output[row]=0;}
};
struct Copy {
  Scalar *output;
  const Scalar *input;
  DOME_HEAT_FN void operator()(size_t row) const {output[row]=input[row];}
};
struct Combine {
  Scalar *output;
  const Scalar *first,*second;
  Scalar scale;
  DOME_HEAT_FN void operator()(size_t row) const {
    output[row]=f64_add({first[row]},f64_mul({scale},{second[row]})).v;
  }
};
struct Multiply {
  SparseView matrix;
  const Scalar *input;
  Scalar *output;
  const IterationState *state=nullptr;
  DOME_HEAT_FN void operator()(size_t row) const {
    if(!running(state))return;
    auto sum=f64_mul({matrix.diagonal[row]},{input[row]});
    for(uint32_t i=matrix.starts[row];i<matrix.starts[row+1];++i)
      sum=f64_add(sum,f64_mul({matrix.coefficients[i]},{input[matrix.columns[i]]}));
    output[row]=sum.v;
  }
};
struct Triangular {
  SparseView factor;
  const Scalar *input;
  Scalar *output;
  uint32_t first;
  const IterationState *state=nullptr;
  DOME_HEAT_FN void operator()(size_t index) const {
    if(!running(state))return;
    const auto row=first+index;
    float64_t value{input[row]};
    for(uint32_t i=factor.starts[row];i<factor.starts[row+1];++i)
      value=f64_sub(value,f64_mul({factor.coefficients[i]},{output[factor.columns[i]]}));
    output[row]=f64_div(value,{factor.diagonal[row]}).v;
  }
};
struct UpdateSolution {
  Scalar *solution,*residual;
  const Scalar *direction,*product;
  const IterationState *state;
  DOME_HEAT_FN void operator()(size_t row) const {
    if(!running(state))return;
    const Scalar alpha=state->scale;
    solution[row]=f64_add({solution[row]},f64_mul({alpha},{direction[row]})).v;
    residual[row]=f64_add({residual[row]},f64_mul({alpha^sign_bit},{product[row]})).v;
  }
};
struct UpdateDirection {
  Scalar *direction;
  const Scalar *preconditioned;
  const IterationState *state;
  DOME_HEAT_FN void operator()(size_t row) const {
    if(running(state))direction[row]=f64_add({preconditioned[row]},f64_mul({state->scale},{direction[row]})).v;
  }
};
enum class ReductionResult { Store, InitialRho, Alpha, Estimate, Beta };

DOME_HEAT_FN void finish_reduction(IterationState &state,ReductionResult result,Scalar value) {
  switch(result){
  case ReductionResult::InitialRho:
    state.rho=value;
    break;
  case ReductionResult::Alpha:
    if(!positive_finite(value)||!positive_finite(state.rho)){
      state.progress.phase=IterationPhase::InvalidCurvature;
      break;
    }
    state.scale=f64_div({state.rho},{value}).v;
    break;
  case ReductionResult::Estimate:
    ++state.progress.completed;
    if(value>=infinity)state.progress.phase=IterationPhase::InvalidResidual;
    else if(value<=state.target)state.progress.phase=IterationPhase::ReplayResidual;
    break;
  case ReductionResult::Beta:
    state.scale=f64_div({value},{state.rho}).v;
    state.rho=value;
    break;
  case ReductionResult::Store:
    break;
  }
}
inline constexpr size_t reduction_chunk=32;
struct DotChunks {
  const Scalar *first, *second;
  Scalar *output;
  size_t count;
  IterationState *state=nullptr;
  ReductionResult result=ReductionResult::Store;
  DOME_HEAT_FN void operator()(size_t group) const {
    if(!running(state))return;
    const size_t begin=group*reduction_chunk;
    const size_t remaining=count-begin;
    const size_t last=begin+(remaining<reduction_chunk?remaining:reduction_chunk);
    float64_t sum{0};
    for(size_t i=begin;i<last;++i)
      sum=f64_add(sum,second?f64_mul({first[i]},{second[i]}):float64_t{first[i]});
    if(result==ReductionResult::Store)output[group]=sum.v;
    else finish_reduction(*state,result,sum.v);
  }
};
} // namespace dome::detail::surface_heat_device
#undef DOME_HEAT_FN
