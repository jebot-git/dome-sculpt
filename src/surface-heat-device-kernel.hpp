// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
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
  DOME_HEAT_FN void operator()(size_t row) const {
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
  DOME_HEAT_FN void operator()(size_t index) const {
    const auto row=first+index;
    float64_t value{input[row]};
    for(uint32_t i=factor.starts[row];i<factor.starts[row+1];++i)
      value=f64_sub(value,f64_mul({factor.coefficients[i]},{output[factor.columns[i]]}));
    output[row]=f64_div(value,{factor.diagonal[row]}).v;
  }
};
inline constexpr size_t reduction_chunk=32;
struct DotChunks {
  const Scalar *first, *second;
  Scalar *output;
  size_t count;
  DOME_HEAT_FN void operator()(size_t group) const {
    const size_t begin=group*reduction_chunk;
    const size_t remaining=count-begin;
    const size_t last=begin+(remaining<reduction_chunk?remaining:reduction_chunk);
    float64_t sum{0};
    for(size_t i=begin;i<last;++i)
      sum=f64_add(sum,second?f64_mul({first[i]},{second[i]}):float64_t{first[i]});
    output[group]=sum.v;
  }
};
} // namespace dome::detail::surface_heat_device
#undef DOME_HEAT_FN
