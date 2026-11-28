// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <span>

namespace dome {
struct SkinWeights;
struct WeightLimits;
}
namespace dome::detail {
struct ContinuationReport;
namespace weight_surface {
struct System;
struct Budget;
}
namespace visibility_heat {
using SupportMask=std::array<uint64_t,4>;
struct FieldDomainReport {
  size_t blocks=0, rigid_rows=0, coupled_rows=0, largest_block=0, field_rows=0;
  size_t whole_mesh_field_rows=0;
  double partition_error=0;
};

class FieldSolver {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  FieldSolver(const weight_surface::System &,weight_surface::Budget &);
  ~FieldSolver();
  void solve(std::span<const double> rhs,std::span<double> field,
             weight_surface::Budget &,ContinuationReport &);
};

SkinWeights solve_supported_domains(const weight_surface::System &,std::span<const SupportMask>,
                                    size_t bones,WeightLimits,weight_surface::Budget &,
                                    ContinuationReport &,FieldDomainReport &);
}
}
