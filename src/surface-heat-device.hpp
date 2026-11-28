// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <span>

namespace dome::detail::surface_heat_device {
struct SparseRows {
  std::span<const uint32_t> starts, columns;
  std::span<const double> coefficients, diagonal;
};
struct System {
  SparseRows matrix, lower, upper;
};

enum class Vector { Right, Refined, Solution, Residual, Direction, Product, Forward, Preconditioned, Defect, Count };

class Fields {
public:
  virtual ~Fields() = default;
  virtual void write(Vector, std::span<const double>) = 0;
  virtual void read(Vector, std::span<double>) = 0;
  virtual void zero(Vector) = 0;
  virtual void copy(Vector result, Vector input) = 0;
  // result = first + scale * second. Result may alias either input.
  virtual void combine(Vector result, Vector first, double scale, Vector second) = 0;
  virtual void multiply(Vector result, Vector input) = 0;
  virtual void forward(uint32_t first_row,uint32_t last_row) = 0;
  virtual void backward(uint32_t first_row,uint32_t last_row) = 0;
  virtual double dot(Vector,Vector) = 0;
};
std::unique_ptr<Fields> create(const System &);
} // namespace dome::detail::surface_heat_device
