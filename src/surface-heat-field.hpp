// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <memory>
#include <span>

namespace dome::detail {
struct ContinuationReport;
namespace weight_surface {
struct System;
struct Budget;
}
namespace visibility_heat {
class FieldSolver {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  FieldSolver(const weight_surface::System &,weight_surface::Budget &);
  ~FieldSolver();
  void solve(std::span<const double> rhs,std::span<double> field,
             weight_surface::Budget &,ContinuationReport &);
};
}
}
