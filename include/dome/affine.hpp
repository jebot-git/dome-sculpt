// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
namespace dome {
using AffineVector = std::array<double, 3>;
enum class AffineOrientation { Singular = 0, Preserving = 1, Reversing = -1 };
// Column-major affine authority. Vectors remain raw: the caller's declared
// material/vertex interpolation convention owns normalization and TBN assembly.
// No approximate TRS decomposition or world-shear removal occurs here.
class AffineTransform {
public:
  AffineTransform() noexcept;
  explicit AffineTransform(const std::array<double, 16> &);
  explicit AffineTransform(const std::array<float, 16> &);
  const std::array<double, 16> &matrix() const noexcept { return matrix_; }
  // (a*b)(p) == a(b(p)). Checked finite arithmetic; input is never modified.
  AffineTransform operator*(const AffineTransform &) const;
  AffineTransform inverse() const;
  AffineVector point(AffineVector) const;
  AffineVector direction(AffineVector) const;
  // cross(A*u,A*v) == cofactor_normal(cross(u,v)), including reflections.
  AffineVector cofactor_normal(AffineVector) const;
  // Inverse transpose, with no determinant-sign fold or normalization.
  AffineVector normal(AffineVector) const;
  // Refuses an uncertifiable cancellation sign instead of inventing a flip.
  AffineOrientation orientation() const;
  // Explicit boundary to float GPU/legacy math; overflow and nonzero underflow
  // are errors rather than silently introducing a singular transform.
  std::array<float, 16> float_matrix() const;
private:
  std::array<double, 16> matrix_;
};
} // namespace dome
