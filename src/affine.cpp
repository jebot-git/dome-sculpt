// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/affine.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace dome {
namespace {
constexpr std::array<double, 16> identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
double checked(long double x) {
  const double result=static_cast<double>(x);
  if(!std::isfinite(x) || !std::isfinite(result) || (x!=0 && result==0))
    throw std::overflow_error("Affine arithmetic exceeds finite double precision range");
  return result;
}
void valid(AffineVector v) {
  for(double x:v)if(!std::isfinite(x))throw std::invalid_argument("Affine vector must be finite");
}
struct Linear {
  // Column magnitudes are factored separately so orthogonal anisotropy does
  // not become an invented near-singularity (e.g. scale 1e-30,1,1e30).
  long double a[3][3]{}, scale[3]{}, cof[3][3]{}, det=0,sign_error=0;
  explicit Linear(const std::array<double,16> &m) {
    for(int c=0;c<3;++c) {
      scale[c]=std::max({std::abs(static_cast<long double>(m[c*4])),
                         std::abs(static_cast<long double>(m[c*4+1])),
                         std::abs(static_cast<long double>(m[c*4+2]))});
      if(scale[c])for(int r=0;r<3;++r)a[r][c]=m[c*4+r]/scale[c];
    }
    for(int r=0;r<3;++r)for(int c=0;c<3;++c) {
      const int r1=(r+1)%3,r2=(r+2)%3,c1=(c+1)%3,c2=(c+2)%3;
      cof[r][c]=a[r1][c1]*a[r2][c2]-a[r1][c2]*a[r2][c1];
    }
    for(int c=0;c<3;++c)det+=a[0][c]*cof[0][c];
    long double magnitude=0;
    for(int c=0;c<3;++c) {
      const int c1=(c+1)%3,c2=(c+2)%3;
      magnitude+=std::abs(a[0][c]*a[1][c1]*a[2][c2])+
                 std::abs(a[0][c]*a[1][c2]*a[2][c1]);
    }
    sign_error=64*std::numeric_limits<long double>::epsilon()*magnitude;
  }
  void invertible() const {
    if(scale[0]==0 || scale[1]==0 || scale[2]==0 || std::abs(det)<=1e-12L)
      throw std::invalid_argument("Affine inverse is singular or has nearly dependent axes");
  }
};
} // namespace
AffineTransform::AffineTransform() noexcept:matrix_(identity){}
AffineTransform::AffineTransform(const std::array<double,16> &m):matrix_(m) {
  for(double x:m)if(!std::isfinite(x))throw std::invalid_argument("Affine matrix must be finite");
  if(m[3]!=0 || m[7]!=0 || m[11]!=0 || m[15]!=1)
    throw std::invalid_argument("Affine matrix must have affine final row");
}
AffineTransform::AffineTransform(const std::array<float,16> &m) {
  for(size_t i=0;i<16;++i)matrix_[i]=m[i];
  *this=AffineTransform(matrix_);
}
AffineTransform AffineTransform::operator*(const AffineTransform &b) const {
  auto out=identity;
  for(int c=0;c<4;++c)for(int r=0;r<3;++r) {
    long double x=0;
    for(int k=0;k<4;++k)x+=static_cast<long double>(matrix_[k*4+r])*b.matrix_[c*4+k];
    out[c*4+r]=checked(x);
  }
  return AffineTransform(out);
}
AffineVector AffineTransform::point(AffineVector v) const {
  valid(v);AffineVector out{};
  for(int r=0;r<3;++r) {
    long double x=matrix_[12+r];
    for(int c=0;c<3;++c)x+=static_cast<long double>(matrix_[c*4+r])*v[c];
    out[r]=checked(x);
  }
  return out;
}
AffineVector AffineTransform::direction(AffineVector v) const {
  valid(v);AffineVector out{};
  for(int r=0;r<3;++r) {
    long double x=0;
    for(int c=0;c<3;++c)x+=static_cast<long double>(matrix_[c*4+r])*v[c];
    out[r]=checked(x);
  }
  return out;
}
AffineVector AffineTransform::cofactor_normal(AffineVector v) const {
  valid(v);Linear l(matrix_);AffineVector out{};
  for(int r=0;r<3;++r) {
    long double x=0;
    for(int c=0;c<3;++c)x+=l.cof[r][c]*l.scale[(c+1)%3]*l.scale[(c+2)%3]*v[c];
    out[r]=checked(x);
  }
  return out;
}
AffineVector AffineTransform::normal(AffineVector v) const {
  valid(v);Linear l(matrix_);l.invertible();AffineVector out{};
  for(int r=0;r<3;++r) {
    long double x=0;
    for(int c=0;c<3;++c)x+=(l.cof[r][c]/l.det)/l.scale[c]*v[c];
    out[r]=checked(x);
  }
  return out;
}
AffineTransform AffineTransform::inverse() const {
  Linear l(matrix_);l.invertible();auto out=identity;
  for(int c=0;c<3;++c)for(int r=0;r<3;++r)
    out[c*4+r]=checked((l.cof[c][r]/l.det)/l.scale[r]);
  for(int r=0;r<3;++r) {
    long double t=0;
    for(int c=0;c<3;++c)t-=static_cast<long double>(out[c*4+r])*matrix_[12+c];
    out[12+r]=checked(t);
  }
  return AffineTransform(out);
}
AffineOrientation AffineTransform::orientation() const {
  const Linear l(matrix_);
  // Do not invent a front-face flip from cancellation noise. Original local
  // chains can still certify their orientation through orientation_world().
  if(l.sign_error>0 && std::abs(l.det)<=l.sign_error)
    throw std::invalid_argument("Affine orientation is unresolved at available arithmetic precision; retain the original local chain");
  return l.det>0?AffineOrientation::Preserving:l.det<0?AffineOrientation::Reversing:AffineOrientation::Singular;
}
std::array<float,16> AffineTransform::float_matrix() const {
  std::array<float,16> out{};
  for(size_t i=0;i<16;++i) {
    out[i]=static_cast<float>(matrix_[i]);
    if(!std::isfinite(out[i]) || (matrix_[i]!=0 && out[i]==0))
      throw std::overflow_error("Affine matrix cannot be represented by a finite float GPU matrix");
  }
  return out;
}
} // namespace dome
